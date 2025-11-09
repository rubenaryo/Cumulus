#include "PhongCommon.hlsli"
#include "TimeBuffer.hlsli"

struct VertexOut
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
    float3 worldPos : POSITION;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
};

cbuffer PSLights : register(b10)
{
    float3 ambientColor;
    DirectionalLight directionalLight;
    float3 cameraWorldPos;
}

cbuffer PSPerMaterial : register(b11)
{
    float4 colorTint;
    float  specularity;
}

Texture2D diffuseTexture    : register(t0);
Texture2D normalMap         : register(t1);
SamplerState samplerOptions : register(s0);
float4 main(VertexOut input) : SV_TARGET
{
    // Sample diffuse texture, normal map(unpacked)
    float3 surfaceColor = diffuseTexture.Sample(samplerOptions, input.uv).rgb;
    float3 sampledNormal = normalMap.Sample(samplerOptions, input.uv).rgb * 2 - 1;

    // Normalize normal vector
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent - dot(input.tangent, input.normal) * input.normal);
    input.binormal = normalize(input.binormal);

    // create transformation matrix TBN
    float3x3 TBN = float3x3(input.tangent, input.binormal, input.normal);
    input.normal = mul(TBN, sampledNormal);

    // Holds the total light for this pixel
    float3 totalLight = 0;
    float3 toCamera = normalize(cameraWorldPos - input.worldPos);

    float3 lightColor = float3(1.0, 1.0, 1.0);
    float3 ambientColor = float3(1.0, 1.0, 1.0);
    float3 toLight = float3(cos(totalTime), 0.0, sin(totalTime));
    float matSpecularity = 64.0;
    toLight = normalize(toLight);
    
    // Diffuse Color
    float3 diffuseLighting = lightColor *
        DiffuseAmount(input.normal, toLight);

    // Specular Color 
    float3 specularLighting = lightColor *
        SpecularPhong(input.normal, toLight, toCamera, matSpecularity) * any(diffuseLighting);

    // Add to totallight
    totalLight += diffuseLighting;

    // Finally, add the ambient color
    const float AMBIENT_INTENSITY = 0.4f;
    totalLight += ambientColor * AMBIENT_INTENSITY;
    
    totalLight *= surfaceColor;

    totalLight = saturate(totalLight);
    return float4(totalLight, 1);
}