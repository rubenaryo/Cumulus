#include "Raymarch_Common.hlsli"

SamplerState linearWrap : register(s2);
SamplerState linearClamp : register(s3);

Texture3D sdfTex : register(t1); // Cached sdf for accelerating sdf 
Texture3D nvdfTex : register(t2); // Model textures combined [sdf.r, model.r, model.g, model.b] 
Texture3D noiseTex : register(t3); // Low frequency, high frequency noises for wispy and billowy clouds 
RWTexture3D<float4> gCache : register(u0);

// Compute optical depth from samplePos toward sunDir
// Returns tau; transmittance is exp(-tau)
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
    for (int i = 0; i < 16; ++i)
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

            float sigma = dimensionalProfile * 0.01;

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

// TODO: Change workgroup size
[numthreads(1, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float sdf = sdfTex.SampleLevel(linearClamp, dispatchThreadID, 0.0f).r;
    float dimensionalProfile = nvdfTex.SampleLevel(linearClamp, dispatchThreadID, 0.0f).g;
    NoiseSample noiseSample = MakeNoiseSample(noiseTex.SampleLevel(
        linearWrap,
        dispatchThreadID,
        0.0f
    ));
    
    // Debug: Fill with random stuff to test binding
    gCache[dispatchThreadID] = float4(sdf, dimensionalProfile, noiseSample.highFreqBillow, 1.0);

}