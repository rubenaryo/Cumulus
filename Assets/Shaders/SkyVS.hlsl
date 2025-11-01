#include "VS_Common.hlsli"

struct VertexIn
{
    float3 position : POSITION;
};

struct VertexOut
{
    float4 position  : SV_POSITION;
    float3 sampleDir : DIRECTION;
};

VertexOut main( VertexIn input )
{
    VertexOut output;

    output.position = mul(viewProj, float4(input.position, 1.0f));

    // Prepare for perspective divide = 1
    output.position.z = output.position.w;

    // Sample in the direction of the vertex
    output.sampleDir = input.position;

    return output;
}