RWTexture2D<float4> outTransmittanceTexture : register(u0);

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    outTransmittanceTexture.GetDimensions(width, height);
    
    uint2 pixel = dispatchThreadID.xy;
    float2 uv = float2(pixel) / float2(width, height);

    // Simple debug gradient
    float r = uv.x;
    float g = uv.y;
    float b = 1.0 - uv.x; // just to get some contrast

    outTransmittanceTexture[pixel] = float4(r, g, b, 1.0);
}
