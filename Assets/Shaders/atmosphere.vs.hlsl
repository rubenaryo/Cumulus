#include "AtmosphereBuffer.hlsli"

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut
{
    float4 position : SV_POSITION;
    float3 view_ray : TEXCOORD0;
};

VertexOut main(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    float2 texc = gTexCoords[vid];
    
    // Map [0,1]^2 to NDC space
    // In DirectX, NDC space is [-1,1] for X and Y, [0,1] for Z
    vout.position = float4(2.0f * texc.x - 1.0f, 1.0f - 2.0f * texc.y, 0.0f, 1.0f);
    
    // Construct view ray:
    // 1. Transform vertex from clip space to view/camera space
    float4 view_space = mul(view_from_clip, vout.position);
    
    // 2. Convert to direction (w=0) and transform to world space
    float4 world_dir = mul(model_from_view, float4(view_space.xyz, 0.0));
    
    vout.view_ray = world_dir.xyz;
    
    return vout;
}


