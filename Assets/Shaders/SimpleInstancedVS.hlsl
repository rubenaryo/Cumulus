#include "VS_Common.hlsli"

struct VertexIn
{
    float3 position : POSITION;

    float4x4 world : INSTANCE_WORLDMATRIX;
};

float4 main( VertexIn vi ) : SV_POSITION
{
    float4x4 wvp = mul(viewProj, vi.world);

    return mul(wvp, float4(vi.position, 1.0f));
}
