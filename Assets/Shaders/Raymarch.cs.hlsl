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
#define USE_MULTIPLE_SCATTERING  1   // Approx. multiple scattering

// === Debug / Visualization ===
#define DEBUG_AABB_INTERSECT     0   // Visualize volume/hull hits
#define DEBUG_STEP_COUNT         0   // Step-count gradient debug

Texture2D gInput : register(t0);
Texture3D sdfTex : register(t1); // Cached sdf for accelerating sdf 
Texture3D nvdfTex : register(t2); // Model textures combined [sdf.r, model.r, model.g, model.b] 
Texture3D noiseTex : register(t3); // Low frequency, high frequency noises for wispy and billowy clouds 
Texture3D lightCacheTex : register(t4); // Optical depth from the centers of voxel 
Texture2D depthStencilBuffer : register(t5); // The scene's depth-stencil buffer, bound hsere post-graphics passes
Texture3D proceduralNvdfTex : register(t6); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 

SamplerState linearWrap : register(s2);
SamplerState linearClamp : register(s3); 
RWTexture2D<float4> gOutput : register(u0);

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

// Compute optical depth from samplePos toward sunDir
// Returns tau; transmittance is exp(-tau)
float GetApproxOpticalDepthToSun(float3 samplePos, float3 sunDir)
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
    
    //return lightCacheTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos), 0.0f).r;

    // Start inside the box at the sample position
    float t = 0.0; // we are already at samplePos, so relative distance along sunDir
    float depth = 0.0; // optical depth tau
    const float minStepSize = AUTHORING_TO_WORLD_SCALE; // ~1 NVDF voxel
    const float depthThreshold = 5.0; // Appr 99% extinction)

    // Ray march for the first two steps 
    [loop]
    for (int i = 0; i < 2; ++i)
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
                densityScale,
                noiseTex,
                linearWrap
            );

            float sigma = dimensionalProfile * (1 - DIRECT_LIGHTING_SCALE);

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
    
    // Use cached values to approximate remaining light ray march
    float cachedOpticalDepth = lightCacheTex.SampleLevel(linearClamp, WorldToNvdfUV(samplePos + sunDir * t), 0.0f).r;
    return depth + cachedOpticalDepth;
#endif
}

// Simple Henyey–Greenstein phase function
float HenyeyGreenstein(float cosAngle, float eccentricity)
{
    float eccentricity2 = eccentricity * eccentricity;
    float denom = pow(1.0 + eccentricity2 - 2.0 * eccentricity * cosAngle, 1.5);
    return (1.0 - eccentricity2) / (4.0 * PI * denom);
}


float CloudPhase(float cosTheta)
{
    // Very forward lobe for silver lining
    float g1 = 0.9;
    float w1 = 0.8;

    // Softer lobe for general fill
    float g2 = 0.5;
    float w2 = 0.2;

    float p = w1 * HenyeyGreenstein(cosTheta, g1) + w2 * HenyeyGreenstein(cosTheta, g2);

    // Scale to a reasonable range
    return p * 4.0;
}


float3 ComputeDirectLighting(
    RayMarchInfo rayMarchInfo,
    float3 samplePos,
    float3 viewDir,
    float density,
    float sigma,
    float stepSize)
{
    float opticalDepthToSun = GetApproxOpticalDepthToSun(samplePos, DIR_SUN);
    float T_sun = exp(-opticalDepthToSun); // transmittance from sun to point

    // Phase term: how strongly this point scatters sun light toward the camera
    // DIR_SUN points from world towards the sun
    float3 sunDirToPoint = DIR_SUN; // direction from point to sun
    float cosAngle = dot(normalize(sunDirToPoint), normalize(viewDir)); // cos(theta) between sun and view
    float eccentricity = 0.75; // forward-scattering; tweak for look
    float phase = saturate(HenyeyGreenstein(cosAngle, 0.75) * 3.0);

    // Segment scattering amount
    float segmentScatter = 1.0 - exp(-sigma * stepSize);

    // Direct lighting contribution for this step (before camera transmittance)
    float3 L_step = LIGHT_SUN * T_sun * phase * segmentScatter;

    return L_step;
}

float3 ComputeAmbientLighting(
    float3 samplePos,
    float density)
{
    return float3(0.0, 0.0, 0.0);
}

float3 ComputeMultipleScattering(
    float dimensionalProfile,
    float opticalDepthToSun,
    float sunDot,
    float sdfDistance)
{
    float ms_volume = dimensionalProfile;
    float cloud_distance = sdfDistance;
    float depthTerm = ValueRemap(cloud_distance, -128.0, 0.0, 0.05, 0.25);
    float factor = ValueRemap(sunDot, 0.0, 0.9, 0.25, depthTerm);

    // Exponential shaping based on summed density / tau to sun
    ms_volume *= exp(-opticalDepthToSun * factor);

    // Turn into a soft, slightly warm secondary color
    float3 SECONDARY_COLOR = float3(1.0, 0.96, 0.9);
    float SECONDARY_STRENGTH = 2;

    return SECONDARY_COLOR * ms_volume * SECONDARY_STRENGTH;
}


float3 VolumeRaymarchNvdf(float3 eyePos, float3 dir, float3 bgColor, float maxRayDist, int3 dispatchThreadID)
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
    tExit = min(min(tExit, MAX_DIST), maxRayDist);

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
                densityScale,
                noiseTex,
                linearWrap
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
                        float opticalDepthToSun = GetApproxOpticalDepthToSun(samplePos, DIR_SUN);
                        
                        float sunDot = dot(normalize(DIR_SUN), normalize(dir));

                        float3 msL = ComputeMultipleScattering(
                            dimensionalProfile,        // or density
                            opticalDepthToSun,         // inSunLightSummedDensitySamples analog
                            sunDot,
                            sdfDistance                // cloud_distance analog
                        );
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
    // Apply a power curve to make small alphas vanish faster so there are less dark edge artifacts 
    cloudAlpha = saturate(pow(cloudAlpha, 2.0)); 

    float3 outColor = lerp(bgColorDisplay, cloudMapped, cloudAlpha);
    return outColor;
#endif
}

float GetMaxRayDist(float depth, float3 eyePos, int3 dispatchThreadID)
{ 
    uint width, height;
    gOutput.GetDimensions(width, height);
    
    int2 pixelCoord = dispatchThreadID.xy;
    
    // NDC coordinates (already computed)
    float2 uv = (float2(pixelCoord) + 0.5) / float2(width, height);
    uv = uv * 2.0 - 1.0;
    uv.y = -uv.y;

    // Reconstruct clip-space position
    float4 clipPos = float4(uv.x, uv.y, depth * 2.0f - 1.0f, 1.0f);

    // To view space
    float4 viewPos = mul(invProj, clipPos);
    viewPos.xyz /= viewPos.w;

    // To world space
    float3 worldPosAtDepth = mul(invView, float4(viewPos.xyz, 1.0f)).xyz;

    // Distance from eye along the view ray
    float maxRayDist = length(worldPosAtDepth - eyePos);
    return maxRayDist;
}

 
[numthreads(8, 8, 1)]
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
    float depth = depthStencilBuffer[dispatchThreadID.xy].r;
    float maxRayDist = GetMaxRayDist(depth, eyePos, dispatchThreadID);

    float3 finalColor = VolumeRaymarchNvdf(eyePos, worldDir, bgColor, maxRayDist, dispatchThreadID);
    
    gOutput[dispatchThreadID.xy] = float4(finalColor, 1.0);
}