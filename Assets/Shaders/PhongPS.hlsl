#include "PhongCommon.hlsli"

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
SamplerState samplerOptions : register(s0);
float4 main(VertexOut input) : SV_TARGET
{
    float3 normalRGB = input.normal.xyz * 0.5 + 0.5;

    // Sample diffuse texture, normal map(unpacked)
    float3 surfaceColor = diffuseTexture.Sample(samplerOptions, input.uv).rgb;
    
    // Normalize normal vector
    input.normal = normalize(input.normal);
    
    // Holds the total light for this pixel
    float3 totalLight = 0;
    float3 toCamera = normalize(cameraWorldPos - input.worldPos);
    
    // Diffuse Color
    float3 diffuseLighting = directionalLight.diffuseColor.rgb *
        DiffuseAmount(input.normal, directionalLight.toLight);
    
    // Specular Color
    float3 specularLighting = directionalLight.diffuseColor.rgb *
        SpecularPhong(input.normal, -directionalLight.toLight, toCamera, specularity) * any(diffuseLighting);
    
    // Add to totallight
    totalLight += diffuseLighting + specularLighting;
    
    // Finally, add the ambient color
    totalLight += ambientColor;
    
    totalLight *= surfaceColor;
    
    return float4(totalLight, 1);
}