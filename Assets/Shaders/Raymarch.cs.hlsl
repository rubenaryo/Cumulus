#include "VS_Common.hlsli"

static const int MAX_STEPS = 255;
static const float MIN_DIST = 0.0;
static const float MAX_DIST = 100.0;
static const float EPSILON = 0.001;

Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

float sphereSDF(float3 samplePoint)
{
    return length(samplePoint) - 1.0;
}

float sceneSDF(float3 samplePoint)
{
    return sphereSDF(samplePoint);
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
    return normalize(float3(xy, z));
}

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pixelCoord = dispatchThreadID.xy;
    float4 thisPixel = gInput[pixelCoord];
    
    float3 dir = rayDir(45.0, float2(1280, 800), pixelCoord);
    float3 eyePos = float3(0, 0, 5);
    float dist = rayMarch(eyePos, dir, MIN_DIST, MAX_DIST);
    float4 returnColor = float4(0,0,0,0);
    if (dist > MAX_DIST - EPSILON)
    {
        returnColor = float4(0,0,0,1); // Missed
    }
    else
    {
        returnColor = float4(1,1,1,1); // Hit
    }
    
    gOutput[dispatchThreadID.xy] = returnColor;
}
