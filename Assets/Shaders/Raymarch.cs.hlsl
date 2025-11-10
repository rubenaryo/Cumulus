#include "VS_Common.hlsli"

static const int MAX_STEPS = 255;
static const float MIN_DIST = 0.001;
static const float MAX_DIST = 10.0;
static const float EPSILON = 0.001;

Texture2D gInput : register(t0);
Texture3D sdfNvdfTex : register(t1); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 
RWTexture2D<float4> gOutput : register(u0);

float sphereSDF(float3 samplePoint, float radius)
{
    return length(samplePoint) - radius;
}

float sceneSDF(float3 samplePoint)
{
    float3 spherePos = float3(0, 2, 0);
    float sphereRadius = 0.5;
    return sphereSDF(samplePoint - spherePos, sphereRadius);
}

float rayMarch(float3 eyePos, float3 dir, float start, float end)
{
    float depth = start;
    for (int i = 0; i < MAX_STEPS; ++i)
    {
        float dist = sceneSDF(eyePos + depth*dir);
        
        if (dist < EPSILON)
        {
            return depth;
        }
        
        depth += dist;
        if (depth >= end)
        {
            return end;
        }

    }
    return end;
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
    
    float dist = rayMarch(eyePos, worldDir, MIN_DIST, MAX_DIST);
    float4 returnColor = float4(0, 0, 0, 0);
    if (dist > (MAX_DIST - EPSILON))
    {
        returnColor = gInput[pixelCoord]; // Just return backbuffer if we miss
    }
    else
    {
        float normalized = 1.0 - (dist / MAX_DIST);
        returnColor = float4(normalized, normalized, normalized, 1);
    }
    
    gOutput[dispatchThreadID.xy] = returnColor;
}