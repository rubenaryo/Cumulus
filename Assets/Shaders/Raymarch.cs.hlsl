#include "VS_Common.hlsli"
#include "Raymarch_Common.hlsli"

// === Raymarch / Quality ===
#define GPU_CLOUD 0
#define USE_ADAPTIVE_STEP        1   // Adaptive step size along ray
#define USE_JITTERED_STEP        1   // Stochastic jitter per step
#define USE_HIGH_HIGH_FREQUENCY  1   // Extra near-camera detail

// === Lighting ===
// Preset: "Density only"  -> all USE_*_LIGHTING = 0
// Preset: "Lit clouds"    -> enable desired USE_*_LIGHTING = 1
#define USE_DIRECT_LIGHTING      1   // Sun / directional lighting
#define USE_AMBIENT_LIGHTING     0   // Sky / ambient term
#define USE_MULTIPLE_SCATTERING  0   // Approx. multiple scattering

// === Debug / Visualization ===
#define DEBUG_AABB_INTERSECT     0   // Visualize volume/hull hits
#define DEBUG_STEP_COUNT         0   // Step-count gradient debug

Texture2D gInput : register(t0);
Texture3D sdfTex : register(t1); // Cached sdf for accelerating sdf 
Texture3D nvdfTex : register(t2); // Model textures combined [sdf.r, model.r, model.g, model.b] 
Texture3D noiseTex : register(t3); // Low frequency, high frequency noises for wispy and billowy clouds 
Texture2D depthStencilBuffer : register(t3); // The scene's depth-stencil buffer, bound here post-graphics passes
Texture3D proceduralNvdfTex : register(t4); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 

SamplerState linearWrap : register(s2);
SamplerState linearClamp : register(s3); 
RWTexture2D<float4> gOutput : register(u0);

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
    // Map n in [0,1] to a "folded" pattern
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

// Compute optical depth from samplePos toward sunDir
// Returns τ; transmittance is exp(-τ)
float GetOpticalDepthToSun(float3 samplePos, float3 sunDir)
{
#if GPU_CLOUD
    // TODO: Implement optical depth calculation to the sun using procedural NVDF 
    return 0.0;
#else
    // Intersect light ray with cloud volume
    float tEnter, tExit;
    if (!RayBoxIntersect(samplePos, sunDir, VOLUME_MIN_WS, VOLUME_MAX_WS, tEnter, tExit))
    {
        // Ray from samplePos in sunDir never enters volume
        return 0.0;
    }

    // Start inside the box at the sample position
    float t = 0.0; // we are already at samplePos, so relative distance along sunDir
    float depth = 0.0; // optical depth tau
    const float minStepSize = AUTHORING_TO_WORLD_SCALE; // ~1 NVDF voxel
    const float depthThreshold = 5.0; // Appr 99% extinction)

    [loop]
    for (int i = 0; i < MAX_STEPS; ++i)
    {
        float3 lightPos = samplePos + sunDir * t;

        // Exit if beyond volume intersection
        if (t > tExit)
            break;

        // Sample SDF
        float sdfEncoded = sdfTex.SampleLevel(linearClamp, WorldToNvdfUV(lightPos), 0.0f).r;
        float sdfDistance = DecodeSdf(sdfEncoded) * AUTHORING_TO_WORLD_SCALE;

        if (sdfDistance < 0.0)
        {
            // Inside cloud: approximate density similar to view ray
            float4 nvdfSample = nvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(lightPos), 0.0f);
            float dimensionalProfile = nvdfSample.g;
            float detailType = nvdfSample.b;
            float densityScale = nvdfSample.a;

            // Ideally reuse GetUprezzedVoxelCloudDensity here
            float density = GetUprezzedVoxelCloudDensity(
                /*dummy*/ (RayMarchInfo) 0,
                lightPos,
                dimensionalProfile,
                detailType,
                densityScale
            );

            float sigma = density * 0.1;

            float stepSizeInside = minStepSize; // or a tuned fixed step
            depth += sigma * stepSizeInside;

            t += stepSizeInside;

            if (depth >= depthThreshold)
                return depth; // early-out: almost fully shadowed
        }
        else
        {
            // Outside cloud: advance by SDF distance, at least minStepSize
            float stepSize = max(sdfDistance, minStepSize);
            t += stepSize;
        }
    }

    return depth;
#endif
}

// Simple Henyey–Greenstein phase function
float HenyeyGreenstein(float cosAngle, float eccentricity)
{
    float eccentricity2 = eccentricity * eccentricity;
    float denom = pow(1.0 + eccentricity2 - 2.0 * eccentricity * cosAngle, 1.5);
    return (1.0 - eccentricity2) / (4.0 * PI * denom);
}

float3 ComputeDirectLighting(
    RayMarchInfo rayMarchInfo,
    float3 samplePos,
    float3 viewDir,
    float density,
    float sigma,
    float stepSize)
{
    float opticalDepthToSun = GetOpticalDepthToSun(samplePos, DIR_SUN);
    float T_sun = exp(-opticalDepthToSun); // transmittance from sun to point

    // Phase term: how strongly this point scatters sun light toward the camera
    // DIR_SUN points from world towards the sun
    float3 sunDirToPoint = -DIR_SUN; // direction from point to sun
    float cosAngle = dot(normalize(sunDirToPoint), normalize(viewDir)); // cos θ between sun and view
    float eccentricity = 0.75; // forward-scattering; tweak for look
    float phase = HenyeyGreenstein(cosAngle, eccentricity);

    // Segment scattering amount
    float segmentScatter = 1.0 - exp(-sigma * stepSize);

    // Direct lighting contribution for this step (before camera transmittance)
    float3 L_step = LIGHT_SUN * T_sun * phase* segmentScatter;

    return L_step;
}

float3 ComputeAmbientLighting(
    float3 samplePos,
    float density)
{
    return float3(0.0, 0.0, 0.0);
}

float3 ComputeMultipleScattering(
    float3 samplePos,
    float density)
{
    return float3(0.0, 0.0, 0.0);
}


float3 VolumeRaymarchNvdf(float3 eyePos, float3 dir, float3 bgColor, int3 dispatchThreadID)
{
    float tEnter, tExit;
    if (!RayBoxIntersect(eyePos, dir, VOLUME_MIN_WS, VOLUME_MAX_WS, tEnter, tExit))
    {
        // Ray misses the volume entirely
        return bgColor;
    }

#if DEBUG_AABB_INTERSECT
    for(uint i = 0; i < hullCount; ++i)
    {
        float hullEnter, hullExit;
        ConvexHull ch = hulls[i];

        if (RayConvexHullIntersect(eyePos, dir, ch, hullEnter, hullExit))
        {
            // minBoxEnter = min(minBoxEnter, hullEnter);
            // maxBoxExit = max(maxBoxExit, hullExit);
            return float3(1, 0, 0) * bgColor; // Visualize hull intersection
        }
    }
#endif

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
#if GPU_CLOUD
        float4 sdfSample = proceduralNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
        float collisionValue = sdfSample.a;
        float sdfDistance =  DecodeSdf(sdfSample.r) * AUTHORING_TO_WORLD_SCALE * (1.0 - collisionValue);
        // NVDF range for a is [0.2, 0.6] -> mapping smoothstep [0, 1] to it
        sdfSample.a = smoothstep(-4.0, -12.0, sdfDistance) * 0.4 + 0.2;
        sdfSample.g *= 1.0 - collisionValue;
#else
        float4 sdfSample = sdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
        float collisionValue = proceduralNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f).a;
        sdfSample.g *= (1.0 - collisionValue);
        // collision hack, step size is reduced to show the hole better
        float sdfDistance = DecodeSdf(sdfSample.r) * AUTHORING_TO_WORLD_SCALE * (1.0 - collisionValue);
#endif
        // Sample NVDF volume: .r = encoded SDF, .g = density (dimensional profile)
        
        // NOTE: doing nothing atm, need to fix this below
        // TODO: FIX THIS TO WORK WITH NEW METHOD

        #if USE_ADAPTIVE_STEP
            float adaptive = ComputeAdaptiveStepSize(march.distance);
            march.stepSize = ComputeBaseStepSize(sdfDistance, adaptive);
        #else
            march.stepSize = max(sdfDistance, AUTHORING_TO_WORLD_SCALE);
        #endif
        
        if (sdfDistance < 0.0)
        {
#if USE_JITTERED_STEP
            float jitter = StaticStepJitter(dispatchThreadID.xy, march.stepIndex); // [-0.5, 0.5]
            float jitterDistance = jitter * march.stepSize;
            samplePos += dir * jitterDistance;
#endif
            
#if GPU_CLOUD
            float4 nvdfSample = proceduralNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
            float collisionValue = nvdfSample.a;
        
            float dimensionalProfile = nvdfSample.g;
            float detailType = nvdfSample.b;
            // NVDF range for density scale is [0.2, 0.6] -> mapping smoothstep [0, 1] to it
            float densityScale = smoothstep(-4.0, -12.0, sdfDistance) * 0.4 + 0.2;
            dimensionalProfile *= (1.0 - collisionValue);
#else
            float4 nvdfSample = nvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f);
            float dimensionalProfile = nvdfSample.g;
            float detailType = nvdfSample.b;
            float densityScale = nvdfSample.a;
            float collisionValue = proceduralNvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f).a;
            dimensionalProfile *= (1.0 - collisionValue);
#endif
            
            float density = GetUprezzedVoxelCloudDensity(
                march,
                samplePos,
                dimensionalProfile,
                detailType,
                densityScale
                );

            float sigma = density * DENSITY_SCALE;
            float alpha = 1.0 - exp(-sigma * march.stepSize);

            // Lighting 
            #if (USE_DIRECT_LIGHTING || USE_AMBIENT_LIGHTING || USE_MULTIPLE_SCATTERING)
                        float3 lighting = 0.0.xxx;

                #if USE_DIRECT_LIGHTING
                        float3 directL = ComputeDirectLighting(
                            march,
                            samplePos,
                            dir, // viewDir
                            density,
                            sigma,
                            march.stepSize
                        );
                        lighting += directL;
                #endif

                #if USE_AMBIENT_LIGHTING
                        float3 ambientL = ComputeAmbientLighting(samplePos, density);
                        lighting += ambientL;
                #endif

                #if USE_MULTIPLE_SCATTERING
                        float3 msL = ComputeMultipleScattering(samplePos, density);
                        lighting += msL;
                #endif

                // Single contribution: lighting * alpha * T_cam
                        float3 contrib = lighting * alpha * march.transmittance;
                        march.accumColor += contrib;

                // Update camera transmittance using alpha
                        march.transmittance *= (1.0 - alpha);
                        if (march.transmittance < MIN_TRANSMITTANCE)
                            break;
            #else
                // Pure density-based fallback (your current behavior)
                float3 contrib = cloudColor * alpha * march.transmittance;
                march.accumColor += contrib;
                march.transmittance *= (1.0 - alpha);
                if (march.transmittance < MIN_TRANSMITTANCE)
                    break;
            #endif
        }
        march.distance += march.stepSize;
    }
    
#if DEBUG_STEP_COUNT
    // Number of steps actually taken (body executions)
    float stepsTaken = (march.stepIndex + 1);

    // Normalize to [0,1] using MAX_STEPS as the "max step count"
    float t = stepsTaken / (float) MAX_STEPS;
    t = saturate(t);

    // Simple black→white gradient
    float3 debugColor = lerp(float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0), t);
    return debugColor;
#else

    float3 finalColor = march.accumColor + bgColor * march.transmittance;

    // bgColor is already tonemapped/gamma-corrected, so:
    // 1) Work in linear for clouds.
    // 2) Apply tonemapping only to the cloud contribution.
    // 3) Composite clouds over bgColor in display space.

    // Split terms:
    float3 cloudColorLin = march.accumColor; // HDR / linear clouds
    float3 bgColorDisplay = bgColor; // already tonemapped

    // Tonemap clouds only (Reinhard in linear)
    float3 cloudMapped = cloudColorLin / (1.0 + cloudColorLin);

    // Optional gamma for clouds if bgColor is in gamma 2.2
    cloudMapped = pow(cloudMapped, 1.0 / 2.2);

    // Composite: clouds over background, using cloud alpha ≈ (1 - transmittance)
    float cloudAlpha = 1.0 - march.transmittance;
    cloudAlpha = saturate(cloudAlpha);

    float3 outColor = lerp(bgColorDisplay, cloudMapped, cloudAlpha);
    return outColor;
#endif
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