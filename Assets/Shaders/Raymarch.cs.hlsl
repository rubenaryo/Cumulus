#include "VS_Common.hlsli"

static const int MAX_STEPS = 255;
static const float MIN_DIST = 0.001;
static const float MAX_DIST = 10.0;
static const float EPSILON = 0.001;
static const float3 VOLUME_MIN_WS = float3(-2.0, 0.0, -2.0);
static const float3 VOLUME_MAX_WS = float3(2.0, 4.0, 2.0);
static const float DENSITY_SCALE = 1.0; // Placeholder; to be replaced by values from NVDF
static const float MIN_TRANSMITTANCE = 0.01; // Early-out

Texture2D gInput : register(t0);
Texture3D sdfNvdfTex : register(t1); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 
SamplerState linearWrap : register(s0); 
RWTexture2D<float4> gOutput : register(u0);

float3 WorldToNvdfUV(float3 worldPos)
{
    float3 uvw = (worldPos - VOLUME_MIN_WS) / (VOLUME_MAX_WS - VOLUME_MIN_WS);
    return uvw;
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

float3 VolumeRaymarchNvdf(float3 eyePos, float3 dir, float3 bgColor)
{
    float tEnter, tExit;
    if (!RayBoxIntersect(eyePos, dir, VOLUME_MIN_WS, VOLUME_MAX_WS, tEnter, tExit))
    {
        // Ray misses the volume entirely
        return float3(1, 1, 1);
    }

    // Clamp to your global near/far
    tEnter = max(tEnter, MIN_DIST);
    tExit = min(tExit, MAX_DIST);

    if (tExit <= tEnter)
        return bgColor;

    float segmentLength = (tExit - tEnter) / MAX_STEPS;
    float3 accumColor = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    // Simple white cloud color for now
    const float3 cloudColor = float3(1.0, 1.0, 1.0);

    float t = tEnter;
    [loop]
    for (int i = 0; i < MAX_STEPS; ++i)
    {
        float3 worldPos = eyePos + t * dir;
        float3 uvw = WorldToNvdfUV(worldPos);

        // Stay inside [0,1]^3; Just in case uvw returned by WorldToNvdfUV is invalid
        if (any(uvw < 0.0) || any(uvw > 1.0))
        {
            t += segmentLength;
            
            continue;
        }

        // Sample NVDF: sdfNvdfTex.r = sdf, .g = dimensional profile (density), etc.
        float4 nvdfSample = sdfNvdfTex.SampleLevel(linearWrap, uvw, 0.0f);
        float density = nvdfSample.g; // Dimensional profile
        
        // If the result is all red, the texture is not being loaded or sampled properly
        if (density < EPSILON)
        {
            return float3(1, 0, 0); 
        }

        // Map density to extinction
        float sigma = density * DENSITY_SCALE;

        // Beer-Lambert: alpha for this step
        float alpha = 1.0 - exp(-sigma * segmentLength);

        float3 contrib = cloudColor * alpha * transmittance;

        accumColor += contrib;
        transmittance *= (1.0 - alpha);

        if (transmittance < MIN_TRANSMITTANCE)
            break;

        t += segmentLength;
    }

    // Composite over background
    float3 finalColor = accumColor + bgColor * transmittance;
    return finalColor;
}


float3 rayDir(float fov, float2 res, float2 pixelCoord)
{
    float2 xy = pixelCoord - res / 2.0;
    float z = res.y / tan(radians(fov) / 2.0);
    return normalize(float3(xy, -z));
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
    float3 finalColor = VolumeRaymarchNvdf(eyePos, worldDir, bgColor);

    gOutput[dispatchThreadID.xy] = float4(finalColor, 1.0);
}