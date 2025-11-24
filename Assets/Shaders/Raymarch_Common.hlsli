/*----------------------------------------------
Eli Asimow (eliasimow@gmail.com)
Date : 2025/11
Description : Common Raymarching Structures for Collision
----------------------------------------------*/
#ifndef RAYMARCH_COMMON_HLSLI
#define RAYMARCH_COMMON_HLSLI

// Raymarch settings
static const int MAX_STEPS = 1024; // Max steps per ray
static const float MIN_DIST = 0.001; // Global near distance
static const float MAX_DIST = 1000.0; // Global far distance
static const float EPSILON = 0.001; // Small epsilon for safety
static const float MIN_TRANSMITTANCE = 0.01; // Early-out when mostly opaque

// Volume bounds in world space
static const float SIDE_LENGTH = 4000.0; 
static const float3 VOLUME_MIN_WS = float3(-SIDE_LENGTH / 2, 0.0, -SIDE_LENGTH / 2);
static const float3 VOLUME_MAX_WS = float3(SIDE_LENGTH / 2, SIDE_LENGTH / 8, SIDE_LENGTH / 2);

// Mapping from NVDF authoring space to world 
static const float NVDF_DOMAIN_SIDE_LENGTH = 4000.0; // NVDF authoring domain: 4km x 4km x 0.5km (matches the world volume).
static const float NOISE_DOMAIN_SIDE_LENGTH = 100.0; // Noise domain: 3D noise pattern repeats every 100m in X/Y/Z.
static const float AUTHORING_TO_WORLD_SCALE = SIDE_LENGTH / NVDF_DOMAIN_SIDE_LENGTH;

// Density -> extinction scaling
static const float DENSITY_SCALE = .035; // To be tuned / driven by NVDF

// GPU cloud or Nubis
static const bool USE_GPU_CLOUD = true;


struct AABB
{
	float3 minBounds;
	float3 maxBounds;
};

cbuffer AABBBuffer : register(b3)
{
	uint aabbCount;
	AABB aabbs[1];
};

struct ConvexHull
{
    uint buffer1;   // offset into HullPoints
    uint buffer2;

    uint faceOffset;    // offset into HullFaces
    uint faceCount;

    float4x4 world;
	float4x4 invWorld;
};

cbuffer HullsBuffer : register(b4)
{
	uint hullCount;
	ConvexHull hulls[1];
};

cbuffer HullFacesBuffer : register(b5)
{
	float4 hullFaces[1024];
};


float3 WorldToNvdfUV(float3 worldPos)
{
    float3 local = (worldPos - VOLUME_MIN_WS) / (VOLUME_MAX_WS - VOLUME_MIN_WS);

    // local: (X, Y, Z) normalized into [0,1]

    float u = local.x; // world X -> texture X
    float v = local.z; // world Z -> texture Y (so each slice is an XZ plane)
    float w = local.y; // world Y -> texture Z (stacking along Y)

    return float3(u, v, w);
}

float3 NvdfUVToWorld(float3 uvw)
{
    float u = uvw.x;
    float v = uvw.y;
    float w = uvw.z;

    // Reconstruct normalized local space
    float3 local;
    local.x = u;  // texture U  -> world X
    local.z = v;  // texture V  -> world Z
    local.y = w;  // texture W  -> world Y

    // Convert normalized back to world space
    float3 worldPos = local * (VOLUME_MAX_WS - VOLUME_MIN_WS) + VOLUME_MIN_WS;

    return worldPos;
}

bool RayConvexHullIntersect(
    float3 origin,
    float3 dir,
    ConvexHull hull,
    out float tEnter,
    out float tExit)
{
    // Initial interval: (-∞, +∞)
    tEnter = -1e20;
    tExit  =  1e20;

    uint faceStart = hull.faceOffset;
    uint faceEnd   = hull.faceOffset + hull.faceCount;

    float3 localOrigin = mul(hull.invWorld, float4(origin, 1.0)).xyz;
    float3 localDir    = mul(hull.invWorld, float4(dir, 0.0)).xyz;


    for (uint fi = faceStart; fi < faceEnd; ++fi)
    {
        float4 face = hullFaces[fi];
        float distance = face.w;
        float4 normal = float4(face.xyz, 0.0);

        float dist0 = dot(normal, localOrigin) + distance;
        float denom = dot(normal, localDir);

        if (abs(denom) < 1e-8)
        {
            // Ray is parallel to plane
            if (dist0 > 0.0)
                return false;   // outside → cannot intersect
            else
                continue;       // inside → this plane imposes no limit
        }

        float tHit = -dist0 / denom;

        if (denom < 0.0)
        {
            // entering half-space
            tEnter = max(tEnter, tHit);
        }
        else
        {
            // exiting half-space
            tExit = min(tExit, tHit);
        }

        if (tEnter > tExit)
            return false;
    }

    // Need exit to be after enter, and at least one must be in front
    return tExit > max(tEnter, 0.0);
}

bool PointInsideConvexHull(float3 pointWS, ConvexHull hull)
{
    // Transform point into hull local space
    float3 p = mul(hull.invWorld, float4(pointWS, 1.0)).xyz;

    uint faceStart = hull.faceOffset;
    uint faceEnd   = hull.faceOffset + hull.faceCount;

    // For every plane: dot(n, x) + d <= 0 means inside
    for (uint fi = faceStart; fi < faceEnd; ++fi)
    {
        float4 face = hullFaces[fi];
        float3 n = face.xyz;    // outward normal
        float  d = face.w;      // plane offset

        float dist = dot(n, p) + d;

        // If point is outside any plane → outside hull
        if (dist > 0.0f)
            return false;
    }

    return true;
}


#endif