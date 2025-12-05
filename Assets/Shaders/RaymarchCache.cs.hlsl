#include "Raymarch_Common.hlsli"

SamplerState linearWrap : register(s2);
SamplerState linearClamp : register(s3);

Texture3D sdfTex : register(t1); // Cached sdf for accelerating sdf 
Texture3D nvdfTex : register(t2); // Model textures combined [sdf.r, model.r, model.g, model.b] 
Texture3D noiseTex : register(t3); // Low frequency, high frequency noises for wispy and billowy clouds 
RWTexture3D<float4> gCache : register(u0);

// Returns tau; transmittance is exp(-tau)
// Returns tau along 'dir' starting at samplePos
float GetOpticalDepthAlongDirection(float3 samplePos, float3 dir, float extinctionScale)
{
#if GPU_CLOUD
    return 0.0;
#else
    float tEnter, tExit;
    if (!RayBoxIntersect(samplePos, dir, VOLUME_MIN_WS, VOLUME_MAX_WS, tEnter, tExit))
    {
        return 0.0;
    }

    float t = 0.0;
    float tau = 0.0;
    const float minStepSize = AUTHORING_TO_WORLD_SCALE;
    const float depthThreshold = 5.0;

    [loop]
    for (int i = 0; i < 32; ++i)
    {
        float3 pos = samplePos + dir * t;

        if (t > tExit)
            break;

        float sdfEncoded = sdfTex.SampleLevel(linearClamp, WorldToNvdfUV(pos), 0.0f).r;
        float sdfDistance = DecodeSdf(sdfEncoded) * AUTHORING_TO_WORLD_SCALE;

        if (sdfDistance < 0.0)
        {
            float4 nvdfSample = nvdfTex.SampleLevel(linearClamp, WorldToNvdfUV(pos), 0.0f);
            float dimensionalProfile = nvdfSample.g;
            float detailType = nvdfSample.b;
            float densityScale = nvdfSample.a;

            float density = GetUprezzedVoxelCloudDensity(
                (RayMarchInfo) 0,
                pos,
                dimensionalProfile,
                detailType,
                densityScale,
                noiseTex,
                linearWrap
            );

            float sigma = dimensionalProfile * extinctionScale;

            float stepSizeInside = minStepSize;
            tau += sigma * stepSizeInside;
            t += stepSizeInside;

            if (tau >= depthThreshold)
                return tau;
        }
        else
        {
            float stepSize = max(sdfDistance, minStepSize);
            t += stepSize;
        }
    }

    return tau;
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
    
    CloudLightingParams lightingParams = gCloudLighting; // Cache cbuffer locally
    float3 dirSunN = normalize(lightingParams.dirSun);
    uint3 texDim = uint3(width, height, depth);

    // Convert dispatch index -> UV at voxel center
    float3 sampleUV = VoxelIndexToNvdfUV(dispatchThreadID, texDim);
    float3 sampleWS = UVToWorld(sampleUV);

    float tauSun = GetOpticalDepthAlongDirection(sampleWS, dirSunN, lightingParams.directExtinctionScale);
    float tauVert = GetOpticalDepthAlongDirection(sampleWS, normalize(float3(0, 1, 0)), lightingParams.ambientExtinctionScale);

    gCache[dispatchThreadID] = float4(tauSun, tauVert, 0.0f, 0.0f);
}
