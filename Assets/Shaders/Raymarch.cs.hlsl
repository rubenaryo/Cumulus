#include "VS_Common.hlsli"

// Raymarch settings
static const int MAX_STEPS = 512; // Max steps per ray
static const float MIN_DIST = 0.001; // Global near distance
static const float MAX_DIST = 1000.0; // Global far distance
static const float EPSILON = 0.001; // Small epsilon for safety
static const float MIN_TRANSMITTANCE = 0.01; // Early-out when mostly opaque

// Volume bounds in world space
static const float SIDE_LENGTH = 40.0;
static const float3 VOLUME_MIN_WS = float3(-SIDE_LENGTH, 0.0, -SIDE_LENGTH);
static const float3 VOLUME_MAX_WS = float3(SIDE_LENGTH, SIDE_LENGTH / 4, SIDE_LENGTH);

// Mapping from NVDF authoring space to world space
static const float REFERENCE_SIDE_LENGTH = 4096.0;
static const float VOLUME_SCALE = SIDE_LENGTH / REFERENCE_SIDE_LENGTH;

// Density -> extinction scaling
static const float DENSITY_SCALE = 1.0; // To be tuned / driven by NVDF

Texture2D gInput : register(t0);
Texture3D sdfNvdfTex : register(t1); // Sdf and model textures combined [sdf.r, model.r, model.g, model.b] 
SamplerState linearWrap : register(s0); 
RWTexture2D<float4> gOutput : register(u0);

float3 WorldToNvdfUV(float3 worldPos)
{
    float3 local = saturate((worldPos - VOLUME_MIN_WS) / (VOLUME_MAX_WS - VOLUME_MIN_WS));

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
float ComputeAdaptiveStepSize(float distanceFromCamera)
{
    return max(1.0, max(sqrt(distanceFromCamera), EPSILON) * 0.08);
}

// The larger of the adaptiveStepSize and sdfDistance
float ComputeBaseStepSize(float sdfDistance, float adaptiveStepSize)
{
    return max(sdfDistance, adaptiveStepSize);
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

    float3 accumColor = float3(0.0, 0.0, 0.0);
    float transmittance = 1.0;

    // Simple white cloud color for now
    const float3 cloudColor = float3(1.0, 1.0, 1.0);

    float t = tEnter;
    float stepSize = 0;
    // Ray march until the ray exits the volume or max steps are reached
    [loop]
    for (int i = 0; i < MAX_STEPS && t < tExit; ++i)
    {
        float3 worldPos = eyePos + t * dir;
        float3 uvw = WorldToNvdfUV(worldPos);

        // Sample NVDF: sdfNvdfTex.r = sdf, .g = dimensional profile (density), etc.
        float4 nvdfSample = sdfNvdfTex.SampleLevel(linearWrap, uvw, 0.0f);
        float sdfDistance = DecodeSdf(nvdfSample.r);
        float density = nvdfSample.g;
        
        // Step size is the minimum of sdf distance, and an adaptive value based on cam distance
        float adaptive = ComputeAdaptiveStepSize(t);
        //stepSize = ComputeBaseStepSize(sdfDistance, adaptive);
        stepSize = adaptive;

        // Map density to extinction
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

        t += stepSize;
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
    float3 finalColor = VolumeRaymarchNvdf(eyePos, worldDir, bgColor);

    gOutput[dispatchThreadID.xy] = float4(finalColor, 1.0);
}