#include "VS_Common.hlsli"
#include "Raymarch_Common.hlsli"

// Toggle features
#define USE_ADAPTIVE_STEP 1 // Shows some artifact ATM. Will debug again after uprez. 
#define USE_JITTERED_STEP 1
#define USE_HIGH_HIGH_FREQUENCY 1
#define DEBUG_AABB_INTERSECT 1

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

Texture2D gInput : register(t0);
Texture3D sdfNvdfTex : register(t1); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 
Texture3D noiseTex : register(t2); // Low frequency, high frequency noises for wispy and billowy clouds 
Texture2D depthStencilBuffer : register(t3); // The scene's depth-stencil buffer, bound here post-graphics passes
SamplerState linearWrap : register(s2);
SamplerState linearClamp : register(s3); 
RWTexture2D<float4> gOutput : register(u0);

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


bool RayNearHullPoints(
    float3 origin,
    float3 dir,
    ConvexHull hull,
    float threshold)
{

    float3 localOrigin = mul(hull.invWorld, float4(origin, 1.0)).xyz;
    float3 localDir    = mul(hull.invWorld, float4(dir, 0.0)).xyz;

    float thresholdSq = threshold * threshold;

    uint start = hull.pointOffset;
    uint end   = start + hull.pointCount;

    for (uint i = start; i < end; ++i)
    {
        float3 P = hullPoints[i].xyz;

        if(length(P) < 0.001){
            return false;
        }

        float3 v = P - localOrigin;
        float t  = dot(v, localDir);

        // If t < 0, closest point is behind ray origin
        if (t < 0.0f)
            continue;

        float3 closestPoint = localOrigin + t * localDir;
        float distSq = dot(closestPoint - P, closestPoint - P);

        if (distSq <= thresholdSq)
            return true;
    }

    return false;
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
    float distanceNvdf = distanceWorld / AUTHORING_TO_WORLD_SCALE;

    // Original Nubis-style adaptive step in NVDF units
    float adaptiveNvdf = max(1.0, max(sqrt(distanceNvdf), EPSILON) * 0.08);

    // Convert step size back to world units
    return adaptiveNvdf * AUTHORING_TO_WORLD_SCALE;
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

// Erode normalized base value by erosionValue (noise), re-normalizing remaining range into [0,1].
float ValueErosion(float baseValue, float erosionValue)
{
    // baseValue: [0,1], erosionValue: [0,1]
    float denom = max(1e-4, 1.0 - erosionValue);
    float v = (baseValue - erosionValue) / denom;
    return saturate(v);
}

float FoldBase(float n)
{
    // Map n in [0,1] to a "folded" 0�1 pattern
    return abs(abs(n * 2.0 - 1.0) * 2.0 - 1.0);
}

float FoldPow2(float n)
{
    float f = FoldBase(n);
    return f * f; // f^2
}

float FoldPow4(float n)
{
    float f = FoldBase(n);
    float f2 = f * f;
    return f2 * f2; // f^4
}

float ComputeHighHighFreqNoise(NoiseSample noiseSample, float detailType)
{
    // Wispy branch: inverted, sharper (power 4)
    float wispyFolded = 1.0 - FoldPow4(noiseSample.highFreqWispy);

    // Billow branch: less sharp (power 2)
    float billowFolded = FoldPow2(noiseSample.highFreqBillow);

    // Blend between the two based on detailType
    float hhf = lerp(wispyFolded, billowFolded, detailType);

    return saturate(hhf);
}

float ValueRemap(float x, float inMin, float inMax, float outMin, float outMax)
{
    // Normalize x into [0,1] in the input range
    float t = (x - inMin) / (inMax - inMin);
    t = saturate(t);

    // Remap into the output range
    return lerp(outMin, outMax, t);
}


float ApplyHighHighFreqNoise(
    float baseNoise, // existing noise_composite
    float hhfNoise, // from ComputeHighHighFreqNoise
    float distanceWorld // rayMarchInfo.distance
)
{
    // Strong near camera, fades out by 150m
    float t = ValueRemap(distanceWorld, 50.0, 150.0, 0.9, 1.0);
    t = saturate(t);

    return lerp(hhfNoise, baseNoise, t);
}

float GetFractionFromValue(float x, float minVal, float maxVal)
{
    float t = (x - minVal) / (maxVal - minVal);
    return saturate(t);
}

// Compute uprezzed voxel cloud density from dimensional profile, type and density scale.
float GetUprezzedVoxelCloudDensity(
    RayMarchInfo rayMarchInfo,
    float3 samplePositionWS,
    float dimensionalProfile,
    float detailType,
    float densityScale)
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

float3 VolumeRaymarchNvdf(float3 eyePos, float3 dir, float3 bgColor, int3 dispatchThreadID)
{
    float tEnter, tExit;
    if (!RayBoxIntersect(eyePos, dir, VOLUME_MIN_WS, VOLUME_MAX_WS, tEnter, tExit))
    {
        // Ray misses the volume entirely
        //return float3(1, 1, 1);
        return bgColor;
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
           // return float3(1, 0, 0) * bgColor; // Visualize AABB intersection
#endif
        }
    }


    for(uint i = 0; i < hullCount; ++i)
    {
        float hullEnter, hullExit;
        ConvexHull ch = hulls[i];

        if (RayConvexHullIntersect(eyePos, dir, ch, hullEnter, hullExit))
        {
            // minBoxEnter = min(minBoxEnter, hullEnter);
            // maxBoxExit = max(maxBoxExit, hullExit);
#if DEBUG_AABB_INTERSECT
            return float3(1, 0, 0) * bgColor; // Visualize hull intersection
#endif
        }
    }

    // Clamp to your global near/far
    tEnter = max(tEnter, MIN_DIST);
    tExit = min(tExit, MAX_DIST);

    if (tExit <= tEnter)
        return bgColor;

    const float3 cloudColor = float3(1.0, 1.0, 1.0);

    RayMarchInfo march;
    InitRayMarchInfo(march, tEnter, tExit);

    // Ray march until the ray exits the volume or max steps are reached
    [loop]
    for (int i = 0; i < MAX_STEPS && march.distance < march.tExit; ++i)
    {
        march.stepIndex = i;

        float3 samplePos = eyePos + march.distance * dir;

        // Sample NVDF volume: .r = encoded SDF, .g = density (dimensional profile)
        float4 sdfSample = sdfNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
        float sdfDistance = DecodeSdf(sdfSample.r) * AUTHORING_TO_WORLD_SCALE;

#if USE_ADAPTIVE_STEP
        float adaptive = ComputeAdaptiveStepSize(march.distance);
        march.stepSize = ComputeBaseStepSize(sdfDistance, adaptive);
#else
        march.stepSize = max(sdfDistance, AUTHORING_TO_WORLD_SCALE);
#endif

#if USE_JITTERED_STEP
        float jitter = StaticStepJitter(dispatchThreadID.xy, march.stepIndex); // [-0.5, 0.5]
        float jitterDistance = jitter * march.stepSize;
        samplePos += dir * jitterDistance;
#endif
        
        if (sdfDistance < 0.0)
        {
            float dimensionalProfile = sdfSample.g;
            float detailType = sdfSample.b;
            float densityScale = sdfSample.a;
            
            float density = GetUprezzedVoxelCloudDensity(
                march,
                samplePos,
                dimensionalProfile,
                detailType,
                densityScale
            );
            
            float sigma = density * DENSITY_SCALE;
            float alpha = 1.0 - exp(-sigma * march.stepSize);

            float3 contrib = cloudColor * alpha * march.transmittance;
            march.accumColor += contrib;
        
            march.transmittance *= (1.0 - alpha);
            if (march.transmittance < MIN_TRANSMITTANCE)
                break;
        }

        march.distance += march.stepSize;
    }

    float3 finalColor = march.accumColor + bgColor * march.transmittance;
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