
Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 xy = dispatchThreadID.xy;
    float4 thisPixel = gInput[xy];
    gOutput[dispatchThreadID.xy] = float4(xy.x, xy.y, 0, 1);
}
