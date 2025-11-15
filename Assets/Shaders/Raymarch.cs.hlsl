#include "VS_Common.hlsli"
#include "Raymarch_Common.hlsli"

// Toggle features
#define USE_ADAPTIVE_STEP 1 // Shows some artifact ATM. Will debug again after uprez. 
#define USE_JITTERED_STEP 1
#define DEBUG_AABB_INTERSECT 0
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

// Mapping from NVDF authoring space to world space
static const float NVDF_DOMAIN_SIDE_LENGTH = 4000.0; // authoring space extent
static const float NVDF_TO_WORLD_SCALE = SIDE_LENGTH / NVDF_DOMAIN_SIDE_LENGTH;

// Density -> extinction scaling
static const float DENSITY_SCALE = .04; // To be tuned / driven by NVDF

Texture2D gInput : register(t0);
Texture3D sdfNvdfTex : register(t1); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 
Texture2D depthStencilBuffer : register(t2); // The scene's depth-stencil buffer, bound here post-graphics passes.
SamplerState linearClamp : register(s3); 
RWTexture2D<float4> gOutput : register(u0);

float3 WorldToNvdfUV(float3 worldPos)
{
    float3 local = (worldPos - VOLUME_MIN_WS) / (VOLUME_MAX_WS - VOLUME_MIN_WS);

    // local: (X, Y, Z) normalized into [0,1]

    float u = local.x; // world X -> texture X
    float v = local.z; // world Z -> texture Y (so each slice is an XZ plane)
    float w = local.y; // world Y -> texture Z (stacking along Y)

    return float3(u, v, w);
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

// Take smaller steps near the camera
float ComputeAdaptiveStepSize(float distanceWorld)
{
    // Convert world-space distance to NVDF-space distance
    float distanceNvdf = distanceWorld / NVDF_TO_WORLD_SCALE;

    // Original Nubis-style adaptive step in NVDF units
    float adaptiveNvdf = max(1.0, max(sqrt(distanceNvdf), EPSILON) * 0.08);

    // Convert step size back to world units
    return adaptiveNvdf * NVDF_TO_WORLD_SCALE;
}


// The larger of the adaptiveStepSize and sdfDistance
float ComputeBaseStepSize(float sdfDistance, float adaptiveStepSize)
{
    return max(sdfDistance, adaptiveStepSize);
}

// Hash a 2D coord + step index into [0,1)
float Hash231(uint2 p, uint stepIndex)
{
    uint n = p.x * 1973u ^ p.y * 9277u ^ stepIndex * 26699u ^ 0x68bc21ebu;
    n = (n << 13u) ^ n;
    return frac((n * (n * n * 15731u + 789221u) + 1376312589u) / 4294967296.0);
}

// Turn into [-0.5, 0.5]
float StaticStepJitter(uint2 pixelCoord, uint stepIndex)
{
    return Hash231(pixelCoord, stepIndex) - 0.5f;
}

float3 VolumeRaymarchNvdf(float3 eyePos, float3 dir, float3 bgColor, int3 dispathThreadID)
{
    float tEnter, tExit;
    if (!RayBoxIntersect(eyePos, dir, VOLUME_MIN_WS, VOLUME_MAX_WS, tEnter, tExit))
    {
        // Ray misses the volume entirely
        return float3(1, 1, 1);
    }

    float minBoxEnter = tEnter;
    float maxBoxExit = tExit;
    for (uint i = 0; i < aabbCount; ++i)
    {
        float aabbEnter, aabbExit;
        if (RayBoxIntersect(eyePos, dir, aabbs[i].minBounds, aabbs[i].maxBounds, aabbEnter, aabbExit))
        {
            minBoxEnter = min(minBoxEnter, aabbEnter);
            maxBoxExit = max(maxBoxExit, aabbExit);
#if DEBUG_AABB_INTERSECT
            return float3(1, 0, 0); // Visualize AABB intersection
#endif
        }
    }

    // Clamp to your global near/far
    tEnter = max(tEnter, MIN_DIST);
    tExit = min(tExit, MAX_DIST);

    if (tExit <= tEnter)
        return bgColor;

    float3 accumColor = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    // Simple white cloud color for now
    const float3 cloudColor = float3(1.0, 1.0, 1.0);

    float distance = tEnter; 
    // Ray march until the ray exits the volume or max steps are reached
    [loop]
    for (int i = 0; i < MAX_STEPS && distance < tExit; ++i)
    {
        float3 samplePos = eyePos + distance * dir;

        // Sample NVDF volume: .r = encoded SDF, .g = density (dimensional profile)
        float4 sdfSample = sdfNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
        // Decode SDF from [0,1] texture range into NVDF space, then scale into world units
        float sdfDistance = DecodeSdf(sdfSample.r) * NVDF_TO_WORLD_SCALE;
        
#if USE_ADAPTIVE_STEP
        // Step size: SDF-guided, but never smaller than the distance-based adaptive step
        float adaptive = ComputeAdaptiveStepSize(distance);
        float stepSize = ComputeBaseStepSize(sdfDistance, adaptive);
#else
        // Pure SDF sphere-march; clamp to at least one voxel in world space
        float stepSize = max(sdfDistance, NVDF_TO_WORLD_SCALE);
#endif

#if USE_JITTERED_STEP
        float jitter = StaticStepJitter(dispathThreadID.xy, i); // [-0.5, 0.5]
        float jitterDistance = jitter * stepSize;
        samplePos += dir * jitterDistance;
#endif
        
        if (sdfDistance < 0.0)
        {
            // Map density to extinction
            float4 nvdfSample = sdfNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
            float density = nvdfSample.g;
            float sigma = density * DENSITY_SCALE;
   
            // Beer-Lambert: alpha for this step
            float alpha = 1.0 - exp(-sigma * stepSize);

            // How much new cloud light this step contributes to the pixel, after accounting for previous absorption
            float3 contrib = cloudColor * alpha * transmittance;
            accumColor += contrib;
        
            // Less background color is left for further steps
            transmittance *= (1.0 - alpha);
            if (transmittance < MIN_TRANSMITTANCE)
                break;
        }

        distance += stepSize;
    }

    // Composite over background
    float3 finalColor = accumColor + bgColor * transmittance;
    return finalColor;
}

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pixelCoord = dispatchThreadID.xy;

    uint width, height;
    gOutput.GetDimensions(width, height);
    
    // NDC
    float2 uv = (float2(pixelCoord) + 0.5) / float2(width, height);
    uv = uv * 2.0 - 1.0;
    uv.y = -uv.y;
    
    float tanHalfFovY = 1.0 / proj[1][1];
    float tanHalfFovX = 1.0 / proj[0][0];
    
    float3 viewDir = normalize(float3(uv.x * tanHalfFovX,
                                       uv.y * tanHalfFovY,
                                       1.0));
    
    // Transform to world space
    float3 worldDir = normalize(mul(invView, float4(viewDir, 0.0)).xyz);
    float3 eyePos = float3(invView[0][3], invView[1][3], invView[2][3]); // from the 4th column instead of row..
    
    float3 bgColor = gInput[pixelCoord].rgb;

    // Volume composite against NVDF dimensional profile (green channel)
    float3 finalColor = VolumeRaymarchNvdf(eyePos, worldDir, bgColor, dispatchThreadID);

    float depth = depthStencilBuffer[dispatchThreadID.xy].r;
    
    // Example: Visulize depth
    // finalColor = depth.rrr;
    
    gOutput[dispatchThreadID.xy] = float4(finalColor, 1.0);
}