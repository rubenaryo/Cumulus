#include "VS_Common.hlsli"

struct VertexIn
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
};

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

VertexOut main( VertexIn vi)
{
    VertexOut vo;

    // Construct camera matrix
    matrix wvp = mul(viewProj, world);

    // Transform position by camera matrix
    vo.position = mul(wvp, float4(vi.position, 1.0f));

    // Transform normal too
    vo.normal = mul((float3x3)world, vi.normal);

    // Pass along UVs
    vo.uv = vi.uv;

    // Pass along world position
    vo.worldPos = mul((float3x3)world, vi.position);

    // Transform tangent, binormal
    vo.tangent = mul((float3x3)world, vi.tangent);
    vo.binormal = mul((float3x3)world, vi.binormal);

    vo.color = float4(1, 1, 1, 1);
    
    return vo;
}