#include "PhongCommon.hlsli"

struct VertexOut
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 worldPos : POSITION;
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
    float specularity;
}

Texture2D diffuseTexture : register(t0);
Texture3D test3d : register(t1);
SamplerState samplerOptions : register(s0);

float4 main(VertexOut input) : SV_TARGET
{
    return float4(1,0, 0, 1);
    // Sample diffuse texture
    float3 surfaceColor = diffuseTexture.Sample(samplerOptions, input.uv).rgb;
    
    // Normalize normal vector
    input.normal = normalize(input.normal);
    
    // Holds the total light for this pixel
    float3 totalLight = 0;
    float3 toCamera = normalize(cameraWorldPos - input.worldPos);
    
    // Diffuse Color
    float3 diffuseLighting = directionalLight.diffuseColor.rgb *
        DiffuseAmount(input.normal, normalize(directionalLight.toLight));
    
    // Add to totallight
    totalLight += diffuseLighting;
    
    // Finally, add the ambient color
    totalLight += ambientColor;
    
    totalLight *= surfaceColor;
    
    return float4(totalLight, 1);
}