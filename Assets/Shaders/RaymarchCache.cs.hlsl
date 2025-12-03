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
    for (int i = 0; i < 128; ++i)
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

    return depth;
#endif
}

float3 UVToWorld(float3 uvw)
{
    float3 diff = VOLUME_MAX_WS - VOLUME_MIN_WS;

    float worldX = uvw.x * diff.x + VOLUME_MIN_WS.x;
    float worldY = uvw.z * diff.y + VOLUME_MIN_WS.y;
    float worldZ = uvw.y * diff.z + VOLUME_MIN_WS.z;

    return float3(worldX, worldY, worldZ);
}

float3 VoxelIndexToNvdfUV(uint3 idx, uint3 dim)
{
    float3 dimF = max(float3(dim), 1.0.xxx);
    float3 uvw = (float3(idx) + 0.5f) / dimF;

    // This assumes the cache texture was created with same layout as NVDF
    return uvw; // tex.x->world.x, tex.y->world.z, tex.z->world.y
}

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height, depth;
    gCache.GetDimensions(width, height, depth);

    if (dispatchThreadID.x >= width ||
        dispatchThreadID.y >= height ||
        dispatchThreadID.z >= depth)
    {
        return;
    }
    
    uint3 texDim = uint3(width, height, depth);

    // Convert dispatch index -> UV at voxel center
    float3 sampleUV = VoxelIndexToNvdfUV(dispatchThreadID, texDim);
    float3 sampleWS = UVToWorld(sampleUV);

    float tau = GetOpticalDepthToSun(sampleWS, DIR_SUN);
    gCache[dispatchThreadID] = float4(tau, 0.0f, 0.0f, 0.0f);
}
