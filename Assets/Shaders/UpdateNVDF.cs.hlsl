RWTexture3D<float4> gOutput : register(u0);

[numthreads(16, 16, 2)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    int3 coord = dispatchThreadID.xyz;

    uint width, height, depth;
    gOutput.GetDimensions(width, height, depth);
    
    gOutput[coord] = float4(1.0, 1.0, 1.0, 1.0);
}