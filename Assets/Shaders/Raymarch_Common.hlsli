/*----------------------------------------------
Eli Asimow (eliasimow@gmail.com)
Avi Serebrenik
Date : 2025/11
Description : Common Raymarching Structures for Collision and Noise for Cloud Data Generatin
----------------------------------------------*/
#ifndef RAYMARCH_COMMON_HLSLI
#define RAYMARCH_COMMON_HLSLI

// Constants 
static const float PI = 3.14159265359;

// Raymarch settings
static const int MAX_STEPS = 256; // Max steps per ray
static const float MIN_DIST = 0.001; // Global near distance
static const float MAX_DIST = 4000.0; // Global far distance
static const float EPSILON = 0.001; // Small epsilon for safety
static const float MIN_TRANSMITTANCE = 0.01; // Early-out when mostly opaque

// Lighting Settings 
static const float3 DIR_SUN = normalize(float3(0.0, 1.0, 0.0)); // Temporary hardcoded light dir
static const float3 LIGHT_SUN = float3(220, 240, 250);; // sun color/brightness
static const float DIRECT_EXTINCTION_SCALE = 0.02;
static const float3 SECONDARY_COLOR = float3(1.0, 0.96, 0.9);
static const float SECONDARY_STRENGTH = 2;
static const float3 AMBIENT_COLOR = float3(1.0, 0.96, 0.9);
static const float AMBIENT_STRENGTH = 1;
static const float AMBIENT_EXTINCTION_SCALE = 0.05; 

// Volume bounds in world space
static const float SIDE_LENGTH = 4000.0; 
static const float3 VOLUME_MIN_WS = float3(-SIDE_LENGTH / 2, 0.0, -SIDE_LENGTH / 2);
static const float3 VOLUME_MAX_WS = float3(SIDE_LENGTH / 2, SIDE_LENGTH / 8, SIDE_LENGTH / 2);

// Mapping from NVDF authoring space to world 
static const float NVDF_DOMAIN_SIDE_LENGTH = 4000.0; // NVDF authoring domain: 4km x 4km x 0.5km (matches the world volume).
static const float NOISE_DOMAIN_SIDE_LENGTH = 100.0; // Noise domain: 3D noise pattern repeats every 100m in X/Y/Z.
static const float AUTHORING_TO_WORLD_SCALE = SIDE_LENGTH / NVDF_DOMAIN_SIDE_LENGTH;

// Density -> extinction scaling
static const float DENSITY_SCALE = 1; // To be tuned / driven by NVDF


struct RayMarchInfo
{
    // Ray segment within the volume in world units
    float tEnter; // where we start marching inside the volume
    float tExit; // where we stop marching inside the volume

    // Current marching state
    float distance; // current param along the ray (world units)
    float stepSize; // step size for this iteration

    // Optical state
    float transmittance; // remaining light (1 = fully transparent, 0 = fully opaque)
    float3 accumColor; // accumulated in-scattered radiance / cloud color

    // Bookkeeping
    uint stepIndex; // current step index in the loop (for jitter, etc.)
};

void InitRayMarchInfo(out RayMarchInfo info, float tEnter, float tExit)
{
    info.tEnter = tEnter;
    info.tExit = tExit;
    info.distance = tEnter;
    info.stepSize = 0.0f;
    info.transmittance = 1.0f;
    info.accumColor = float3(0.0f, 0.0f, 0.0f);
    info.stepIndex = 0;
}

struct NoiseSample
{
    float lowFreqWispy;
    float highFreqWispy;
    float lowFreqBillow;
    float highFreqBillow;
};

NoiseSample MakeNoiseSample(float4 sample)
{
    NoiseSample ns;
    ns.lowFreqWispy = sample.x;
    ns.highFreqWispy = sample.y;
    ns.lowFreqBillow = sample.z;
    ns.highFreqBillow = sample.w;
    return ns;
}

struct LightCacheSample
{
    // Tau is optical depth to X 
    float tauSun;  
    float tauVertical; 
};

LightCacheSample MakeLightCacheSample(float2 sample)
{
    LightCacheSample lcs;
    lcs.tauSun = sample.r; 
    lcs.tauVertical = sample.g; 
    return lcs; 
}

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

        float dist0 = dot(normal.xyz, localOrigin) + distance;
        float denom = dot(normal.xyz, localDir);

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

float DistToEdge(float3 p)
{
    float3 closest_point;
    closest_point.x = clamp(p.x, VOLUME_MIN_WS.x, VOLUME_MAX_WS.x);
    closest_point.y = clamp(p.y, VOLUME_MIN_WS.y, VOLUME_MAX_WS.y);
    closest_point.z = clamp(p.z, VOLUME_MIN_WS.z, VOLUME_MAX_WS.z);
    return distance(p, closest_point);
}

//------------------
//NOISE FUNCTIONS
//------------------
// Dots a vec with itself
float dot2(in float3 v)
{
    return dot(v, v);
}
// pseudo random float given an int
float random(int x)
{
    x = (x << 13) ^ x;
    return (1 - (x * (x * x * 15731 + 789221)
            + 1376312589) & (0x7fffffff))
            / 10737741824.0;

}
// pseudo random float3 given a float3
float3 random3(float3 p)
{
    return frac(sin(float3(dot(p, float3(127.1, 311.7, 191.999)),
                          dot(p, float3(269.5, 183.3, 765.54)),
                          dot(p, float3(420.69, 631.2, 109.21))))
                 * 43758.5453);
}

// standard worley noise that Avi translated from the GLSL version
// provided in CIS5600
float WorleyNoise3D(float3 p, float tiles)
{
    p *= tiles;
    // Tile the space
    float3 pointInt = floor(p);
    float3 pointfrac = frac(p);

    float minDist = 1.0; // Minimum distance initialized to max.

    // Search all neighboring cells and this cell for their point
    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                float3 neighbor = float3(float(x), float(y), float(z));

                // Random point inside current neighboring cell
                float3 pt = random3(pointInt + neighbor);

                // Compute the distance b/t the point and the fragment
                // Store the min dist thus far
                float3 diff = neighbor + pt - pointfrac;
                float dist = length(diff);
                minDist = min(minDist, dist);
            }
        }
    }
    return minDist;
}
// random hash function, created by IQ
float hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}
// vec3 variant of hash function, created by IQ
float3 hash3(float3 p)
{
    return float3(
        hash(p),
        hash(p + float3(1.0, 0.0, 0.0)),
        hash(p + float3(0.0, 1.0, 0.0))
    );
}

// Below are two different Perlin-based noise functions with FBM helpers I made.
// They are both created by Stefan Gustavson in GLSL, and the copyright is below with the MIT license.
//
// The first one, cnoise can be found at https://github.com/ashima/webgl-noise/blob/master/src/classicnoise3D.glsl
// and I ported it to HLSL. It is a fast simple perlin noise.
//
// The second one is based on a similar peprlin noise, but was altered to be more "billowy"
// with some otating gradiants and analytic derivatives. It was also originally created by
// Sefan Gustavson, together with Ian McEwan (don't think its the author haha), and was ported
// to HLSL by "domportera" on github. The link to the original is: https://github.com/domportera/hlsl-noise/blob/main/stegu-psrdnoise/psrdnoise3.hlsl
// It follows a 2021 copyright and the same MIT license.

// HELPER MATH FUNCTIONS
#define mod(x, y) (x - y * floor(x / y))

float lt(float a, float b)
{
    return a < b ? 1.0 : 0.0;
}
float lessThan(float a, float b)
{
    return lt(a, b);
}
float2 lessThan(float2 a, float2 b)
{
    return float2(lt(a.x, b.x), lt(a.y, b.y));
}
float3 lessThan(float3 a, float3 b)
{
    return float3(lt(a.x, b.x), lt(a.y, b.y), lt(a.z, b.z));
}
float4 lessThan(float4 a, float4 b)
{
    return float4(lt(a.x, b.x), lt(a.y, b.y), lt(a.z, b.z), lt(a.w, b.w));
}
float gt(float a, float b)
{
    return a > b ? 1.0 : 0.0;
}
float greaterThan(float a, float b)
{
    return gt(a, b);
}
float2 greaterThan(float2 a, float2 b)
{
    return float2(gt(a.x, b.x), gt(a.y, b.y));
}
float3 greaterThan(float3 a, float3 b)
{
    return float3(gt(a.x, b.x), gt(a.y, b.y), gt(a.z, b.z));
}
float4 greaterThan(float4 a, float4 b)
{
    return float4(gt(a.x, b.x), gt(a.y, b.y), gt(a.z, b.z), gt(a.w, b.w));
}

float3 mod289(float3 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 mod289(float4 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 permute(float4 x)
{
    return mod289(((x * 34.0) + 10.0) * x);
}

// Permutation polynomial for the hash value
float4 permute2(float4 x)
{
    float4 xm = mod(x, 289.0);
    return mod(((xm*34.0)+10.0)*xm, 289.0);
}

float4 taylorInvSqrt(float4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float3 fade(float3 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// Classic Perlin noise
//
// Author:  Stefan Gustavson (stefan.gustavson@liu.se)
// Version: 2024-11-07
//
// Many thanks to Ian McEwan of Ashima Arts for the
// ideas for permutation and gradient selection.
//
// Copyright (c) 2011 Stefan Gustavson. All rights reserved.
// Distributed under the MIT license. See LICENSE file.
// https://github.com/stegu/webgl-noise
float cnoise(float3 P)
{
    float3 Pi0 = floor(P); // Integer part for indexing
    float3 Pi1 = Pi0 + float3(1.0, 1.0, 1.0); // Integer part + 1
    Pi0 = mod289(Pi0);
    Pi1 = mod289(Pi1);
    float3 Pf0 = frac(P); // fracional part for interpolation
    float3 Pf1 = Pf0 - float3(1.0, 1.0, 1.0); // fracional part - 1.0
    float4 ix = float4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    float4 iy = float4(Pi0.yy, Pi1.yy);
    float4 iz0 = Pi0.zzzz;
    float4 iz1 = Pi1.zzzz;

    float4 ixy = permute(permute(ix) + iy);
    float4 ixy0 = permute(ixy + iz0);
    float4 ixy1 = permute(ixy + iz1);

    float4 gx0 = ixy0 * (1.0 / 7.0);
    float4 gy0 = frac(floor(gx0) * (1.0 / 7.0)) - 0.5;
    gx0 = frac(gx0);
    float4 gz0 = float4(0.5, 0.5, 0.5, 0.5) - abs(gx0) - abs(gy0);
    float4 sz0 = step(gz0, float4(0.0, 0.0, 0.0, 0.0));
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);

    float4 gx1 = ixy1 * (1.0 / 7.0);
    float4 gy1 = frac(floor(gx1) * (1.0 / 7.0)) - 0.5;
    gx1 = frac(gx1);
    float4 gz1 = float4(0.5, 0.5, 0.5, 0.5) - abs(gx1) - abs(gy1);
    float4 sz1 = step(gz1, float4(0.0, 0.0, 0.0, 0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);

    float3 g000 = float3(gx0.x, gy0.x, gz0.x);
    float3 g100 = float3(gx0.y, gy0.y, gz0.y);
    float3 g010 = float3(gx0.z, gy0.z, gz0.z);
    float3 g110 = float3(gx0.w, gy0.w, gz0.w);
    float3 g001 = float3(gx1.x, gy1.x, gz1.x);
    float3 g101 = float3(gx1.y, gy1.y, gz1.y);
    float3 g011 = float3(gx1.z, gy1.z, gz1.z);
    float3 g111 = float3(gx1.w, gy1.w, gz1.w);

    float4 norm0 = taylorInvSqrt(float4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
    float4 norm1 = taylorInvSqrt(float4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));

    float n000 = norm0.x * dot(g000, Pf0);
    float n010 = norm0.y * dot(g010, float3(Pf0.x, Pf1.y, Pf0.z));
    float n100 = norm0.z * dot(g100, float3(Pf1.x, Pf0.yz));
    float n110 = norm0.w * dot(g110, float3(Pf1.xy, Pf0.z));
    float n001 = norm1.x * dot(g001, float3(Pf0.xy, Pf1.z));
    float n011 = norm1.y * dot(g011, float3(Pf0.x, Pf1.yz));
    float n101 = norm1.z * dot(g101, float3(Pf1.x, Pf0.y, Pf1.z));
    float n111 = norm1.w * dot(g111, Pf1);

    float3 fade_xyz = fade(Pf0);
    float4 n_z = lerp(float4(n000, n100, n010, n110), float4(n001, n101, n011, n111), fade_xyz.z);
    float2 n_yz = lerp(n_z.xy, n_z.zw, fade_xyz.y);
    float n_xyz = lerp(n_yz.x, n_yz.y, fade_xyz.x);
    return 2.2 * n_xyz;
}

// Simple 3D FBM calling the Perlin above
float fbm_3D_Perlin(float3 uvw, int iterations)
{
    float a = 0.5;
    float f = 5.0;
    float n = 0.f;
    for (int i = 0; i < iterations; ++i)
    {
        n += cnoise(uvw * f) * a;
        a *= 0.25f;
        f *= 5.f;
    }
    return n;
}

// psrdnoise3.glsl
//
// Authors: Stefan Gustavson (stefan.gustavson@gmail.com)
// and Ian McEwan (ijm567@gmail.com)
// Version 2021-12-02, published under the MIT license (see below)
//
// Copyright (c) 2021 Stefan Gustavson and Ian McEwan.
// 
// Adapted to HLSL by Dom Portera, published under the MIT license
// https://github.com/domportera/hlsl-noise

// Use Perlin's rotated grid instead of the new tiling grid?
// Enabling this adds about 1% to the execution time and
// requires all periods to be multiples of 3. Other
// integer periods can be specified, but when not evenly
// divisible by 3, the actual period will be 3 times longer.
// Take care not to overstep the maximum allowed period (288).
#define PERLINGRID

// Enable faster gradient rotations?
// Enabling this saves about 10% on execution time,
// but the function will not run faster for alpha = 0.
#define FASTROTATION

// 3-D tiling simplex noise with rotating gradients and first order
// analytical derivatives.
// "float3 x" is the point (x,y,z) to evaluate
// "float3 period" is the desired periods along x,y,z, up to 289.
// (If Perlin's grid is used, multiples of 3 up to 288 are allowed.)
// "float alpha" is the rotation (in radians) for the swirling gradients.
// The "float" return value is the noise value, and
// the "out float3 gradient" argument returns the x,y,z partial derivatives.
//
// The function executes 15-20% faster if alpha is constant == 0.0
// across all fragments being executed in parallel.
// (This speedup will not happen if FASTROTATION is enabled. Do not specify
// FASTROTATION if you are not actually going to use the rotation.)
//
// Setting any period to 0.0 or a negative value will skip the periodic
// wrap for that dimension. Setting all periods to 0.0 makes the function
// execute 10-15% faster.
//
// Not using the return value for the gradient will make the compiler
// eliminate the code for computing it. This speeds up the function by
// around 10%.

float psrdnoise(float3 x, float3 period, float alpha, out float3 gradient)
{

#ifndef PERLINGRID
  // Transformation matrices for the axis-aligned simplex grid
  // Convert matrices from column-major to row major for glsl -> hlsl using transpose()
    const float3x3 M = transpose(float3x3(0.0, 1.0, 1.0,
                      1.0, 0.0, 1.0,
                      1.0, 1.0, 0.0));

    const float3x3 Mi = transpose(float3x3(-0.5, 0.5, 0.5,
                        0.5, -0.5, 0.5,
                        0.5, 0.5, -0.5));
#endif

    float3 uvw;

  // Transform to simplex space (tetrahedral grid)
#ifndef PERLINGRID
  // Use matrix multiplication, let the compiler optimise
    uvw = mul(M, x);
#else
  // Optimised transformation to uvw (slightly faster than
  // the equivalent matrix multiplication on most platforms)
  float third = 1.0/3.0;
  uvw = x + dot(x, float3(third, third, third));
#endif

  // Determine which simplex we're in, i0 is the "base corner"
    float3 i0 = floor(uvw);
    float3 f0 = frac(uvw); // coords within "skewed cube"

  // To determine which simplex corners are closest, rank order the
  // magnitudes of u,v,w, resolving ties in priority order u,v,w,
  // and traverse the four corners from largest to smallest magnitude.
  // o1, o2 are offsets in simplex space to the 2nd and 3rd corners.
    float3 g_ = step(f0.xyx, f0.yzz); // Makes comparison "less-than"
    float3 l_ = 1.0 - g_; // complement is "greater-or-equal"
    float3 g = float3(l_.z, g_.xy);
    float3 l = float3(l_.xy, g_.z);
    float3 o1 = min(g, l);
    float3 o2 = max(g, l);

  // Enumerate the remaining simplex corners
    float3 i1 = i0 + o1;
    float3 i2 = i0 + o2;
    float3 i3 = i0 + float3(1.0, 1.0, 1.0);

    float3 v0, v1, v2, v3;

  // Transform the corners back to texture space
#ifndef PERLINGRID
    v0 = mul(Mi, i0);
    v1 = mul(Mi, i1);
    v2 = mul(Mi, i2);
    v3 = mul(Mi, i3);
#else
  // Optimised transformation (mostly slightly faster than a matrix)
    v0 = i0 - dot(i0, float3(0.166666666f, 0.166666666f, 0.166666666f));
    v1 = i1 - dot(i1, float3(0.166666666f, 0.166666666f, 0.166666666f));
    v2 = i2 - dot(i2, float3(0.166666666f, 0.166666666f, 0.166666666f));
    v3 = i3 - dot(i3, float3(0.166666666f, 0.166666666f, 0.166666666f));
#endif

  // Compute vectors to each of the simplex corners
    float3 x0 = x - v0;
    float3 x1 = x - v1;
    float3 x2 = x - v2;
    float3 x3 = x - v3;

    if (any(greaterThan(period, float3(0.0, 0.0, 0.0))))
    {
    // Wrap to periods and transform back to simplex space
        float4 vx = float4(v0.x, v1.x, v2.x, v3.x);
        float4 vy = float4(v0.y, v1.y, v2.y, v3.y);
        float4 vz = float4(v0.z, v1.z, v2.z, v3.z);
	// Wrap to periods where specified
        if (period.x > 0.0)
            vx = mod(vx, period.x);
        if (period.y > 0.0)
            vy = mod(vy, period.y);
        if (period.z > 0.0)
            vz = mod(vz, period.z);
    // Transform back
#ifndef PERLINGRID
        i0 = mul(M, float3(vx.x, vy.x, vz.x));
        i1 = mul(M, float3(vx.y, vy.y, vz.y));
        i2 = mul(M, float3(vx.z, vy.z, vz.z));
        i3 = mul(M, float3(vx.w, vy.w, vz.w));
#else
    v0 = float3(vx.x, vy.x, vz.x);
    v1 = float3(vx.y, vy.y, vz.y);
    v2 = float3(vx.z, vy.z, vz.z);
    v3 = float3(vx.w, vy.w, vz.w);
    // Transform wrapped coordinates back to uvw
        i0 = v0 + dot(v0, float3(0.3333333333f, 0.3333333333f, 0.3333333333f));
        i1 = v1 + dot(v1, float3(0.3333333333f, 0.3333333333f, 0.3333333333f));
        i2 = v2 + dot(v2, float3(0.3333333333f, 0.3333333333f, 0.3333333333f));
        i3 = v3 + dot(v3, float3(0.3333333333f, 0.3333333333f, 0.3333333333f));
#endif
	// Fix rounding errors
        i0 = floor(i0 + 0.5);
        i1 = floor(i1 + 0.5);
        i2 = floor(i2 + 0.5);
        i3 = floor(i3 + 0.5);
    }

  // Compute one pseudo-random hash value for each corner
    float4 hash = permute2(permute2(permute2(
              float4(i0.z, i1.z, i2.z, i3.z))
            + float4(i0.y, i1.y, i2.y, i3.y))
            + float4(i0.x, i1.x, i2.x, i3.x));

  // Compute generating gradients from a Fibonacci spiral on the unit sphere
    float4 theta = hash * 3.883222077; // 2*pi/golden ratio
    float4 sz = hash * -0.006920415 + 0.996539792; // 1-(hash+0.5)*2/289
    float4 psi = hash * 0.108705628; // 10*pi/289, chosen to avoid correlation

    float4 Ct = cos(theta);
    float4 St = sin(theta);
    float4 sz_prime = sqrt(1.0 - sz * sz); // s is a point on a unit fib-sphere

    float4 gx, gy, gz;

  // Rotate gradients by angle alpha around a pseudo-random ortogonal axis
#ifdef FASTROTATION
  // Fast algorithm, but without dynamic shortcut for alpha = 0
  float4 qx = St;         // q' = norm ( cross(s, n) )  on the equator
  float4 qy = -Ct; 
  float4 qz = float4(0.0, 0.0, 0.0, 0.0);

  float4 px =  sz * qy;   // p' = cross(q, s)
  float4 py = -sz * qx;
  float4 pz = sz_prime;

  psi += alpha;         // psi and alpha in the same plane
  float4 Sa = sin(psi);
  float4 Ca = cos(psi);

  gx = Ca * px + Sa * qx;
  gy = Ca * py + Sa * qy;
  gz = Ca * pz + Sa * qz;
#else
  // Slightly slower algorithm, but with g = s for alpha = 0, and a
  // useful conditional speedup for alpha = 0 across all fragments
    if (alpha != 0.0)
    {
        float sinpsi = sin(psi);
        float cospsi = cos(psi);
        float4 Sp = float4(sinpsi, sinpsi, sinpsi, sinpsi); // q' from psi on equator
        float4 Cp = float4(cospsi, cospsi, cospsi, cospsi);

        float4 px = Ct * sz_prime; // px = sx
        float4 py = St * sz_prime; // py = sy
        float4 pz = sz;

        float4 Ctp = St * Sp - Ct * Cp; // q = (rotate( cross(s,n), dot(s,n))(q')
        float4 qx = lerp(Ctp * St, Sp, sz);
        float4 qy = lerp(-Ctp * Ct, Cp, sz);
        float4 qz = -(py * Cp + px * Sp);

        float sinalpha = sin(alpha);
        float cosalpha = cos(alpha);
        float4 Sa = float4(sinalpha, sinalpha, sinalpha, sinalpha); // psi and alpha in different planes
        float4 Ca = float4(cosalpha, cosalpha, cosalpha, cosalpha);

        gx = Ca * px + Sa * qx;
        gy = Ca * py + Sa * qy;
        gz = Ca * pz + Sa * qz;
    }
    else
    {
        gx = Ct * sz_prime; // alpha = 0, use s directly as gradient
        gy = St * sz_prime;
        gz = sz;
    }
#endif

  // Reorganize for dot products below
    float3 g0 = float3(gx.x, gy.x, gz.x);
    float3 g1 = float3(gx.y, gy.y, gz.y);
    float3 g2 = float3(gx.z, gy.z, gz.z);
    float3 g3 = float3(gx.w, gy.w, gz.w);

  // Radial decay with distance from each simplex corner
    float4 w = 0.5 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3));
    w = max(w, 0.0);
    float4 w2 = w * w;
    float4 w3 = w2 * w;

  // The value of the linear ramp from each of the corners
    float4 gdotx = float4(dot(g0, x0), dot(g1, x1), dot(g2, x2), dot(g3, x3));

  // Multiply by the radial decay and sum up the noise value
    float n = dot(w3, gdotx);

  // Compute the first order partial derivatives
    float4 dw = -6.0 * w2 * gdotx;
    float3 dn0 = w3.x * g0 + dw.x * x0;
    float3 dn1 = w3.y * g1 + dw.y * x1;
    float3 dn2 = w3.z * g2 + dw.z * x2;
    float3 dn3 = w3.w * g3 + dw.w * x3;
    gradient = 39.5 * (dn0 + dn1 + dn2 + dn3);

  // Scale the return value to fit nicely into the range [-1,1]
    return 39.5 * n;
}

// calls the noise above
// I got the values from https://stegu.github.io/psrdnoise/3d-gallery/puffysphere-threejs.html
float fbm_3D_BillowNoise(float3 x, float3 period, int iteration)
{
    float3 grad;
    float3 disp = float3(0.0, 0.0, 0.0);
    
    const float o = 0.6;
    float n = 0.5;
    float a = 0.4;
    float f = 1.0;
    
    for (int i = 0; i < iteration; ++i)
    {
        n += a * psrdnoise(f * x + disp, period, f * 2.0, grad);
        a *= 0.5;
        f *= 2;
        disp += a * o * grad;
    }
    
    n += a * psrdnoise(f * x + disp, period, f * 2.0, grad);
    
    return n;
}

bool RayBoxIntersect(
    float3 origin,
    float3 dir,
    float3 boxMin,
    float3 boxMax,
    out float tEnter,
    out float tExit)
{
    float3 invDir = 1.0 / dir;

    float3 t0s = (boxMin - origin) * invDir;
    float3 t1s = (boxMax - origin) * invDir;

    float3 tMin = min(t0s, t1s);
    float3 tMax = max(t0s, t1s);

    tEnter = max(max(tMin.x, tMin.y), tMin.z);
    tExit = min(min(tMax.x, tMax.y), tMax.z);

    // Need a positive interval where enter < exit
    return tExit > max(tEnter, 0.0);
}

// Encoded value in [0, 1]  ->  real SDF in [-256, 4096]
float DecodeSdf(float encodedSdf)
{
    const float sdfMin = -256.0;
    const float sdfMax = 4096.0;
    return lerp(sdfMin, sdfMax, encodedSdf);
}

// Erode normalized base value by erosionValue (noise), re-normalizing remaining range into [0,1].
float ValueErosion(float baseValue, float erosionValue)
{
    // baseValue: [0,1], erosionValue: [0,1]
    float denom = max(1e-4, 1.0 - erosionValue);
    float v = (baseValue - erosionValue) / denom;
    return saturate(v);
}

// Compute uprezzed voxel cloud density from dimensional profile, type and density scale.
float GetUprezzedVoxelCloudDensity(
    RayMarchInfo rayMarchInfo,
    float3 samplePositionWS,
    float dimensionalProfile,
    float detailType,
    float densityScale, 
    Texture3D<float4> noiseTex,
    SamplerState linearWrap)
{
    // Convert world position into NVDF authoring space (so noise sticks to authored asset, not world scale).
    float3 samplePosNvdf = samplePositionWS / AUTHORING_TO_WORLD_SCALE;
    
    // Map NVDF space into noise UVW: one noise tile spans NOISE_DOMAIN_SIDE_LENGTH author units.
    float nvdfToNoiseScale = 1.0 / NOISE_DOMAIN_SIDE_LENGTH;
    float3 noiseUVW = samplePosNvdf * nvdfToNoiseScale;

    // 3D noise look-up in authoring-relative space.
    NoiseSample noiseSample = MakeNoiseSample(noiseTex.SampleLevel(
        linearWrap,
        noiseUVW,
        0.0f // TODO: plug in distance-based mip
    ));
    
    // Define wispy noise
    float wispy_noise = lerp(noiseSample.lowFreqWispy, noiseSample.highFreqWispy, dimensionalProfile);

    // Define billowy noise
    float billowy_type_gradient = pow(dimensionalProfile, 0.25);
    float billowy_noise = lerp(noiseSample.lowFreqBillow * 0.3, noiseSample.highFreqBillow * 0.3, billowy_type_gradient);
    
    // Define Noise composite - blend to wispy as the density scale decreases.
    float noise_composite = lerp(wispy_noise, billowy_noise, detailType);

#if USE_HIGH_HIGH_FREQUENCY
    //Use HF details for parts near camera
    float hhf = ComputeHighHighFreqNoise(noiseSample, detailType);
    noise_composite = ApplyHighHighFreqNoise(noise_composite, hhf, rayMarchInfo.distance);
#endif
    
    // Composite Noises and use as a Value Erosion
    float uprezzed_density = ValueErosion(dimensionalProfile, noise_composite);
    
    // Modify User density scale
    float powered_density_scale = pow(saturate(densityScale), 4.0);
    
    // Apply User Density Scale Data to Result
    uprezzed_density *= powered_density_scale;
    
    // Sharpen result and lower Density close to camera to both add details and reduce undersampling noise
    uprezzed_density = pow(uprezzed_density, lerp(0.3, 0.6, max(EPSILON, powered_density_scale)));
    
#if USE_HIGH_HIGH_FREQUENCY
    float distance_range_blender = GetFractionFromValue(rayMarchInfo.distance, 50.0, 150.0);
    uprezzed_density = pow(uprezzed_density, lerp(0.5, 1.0, distance_range_blender)) * lerp(0.666, 1.0, distance_range_blender);
#endif
    
    return uprezzed_density;
}

#endif