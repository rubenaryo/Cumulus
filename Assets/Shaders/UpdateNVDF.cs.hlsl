RWTexture3D<float4> gOutput : register(u0);
#include "Raymarch_Common.hlsli"

[numthreads(16, 16, 2)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    int3 coord = dispatchThreadID.xyz;

    uint width, height, depth;
    gOutput.GetDimensions(width, height, depth);
    
    if (coord.x >= width || coord.y >= height || coord.z >= depth)
        return;

    float3 uvw = float3(coord) / float3(width, height, depth);
    float3 worldPos = NvdfUVToWorld(uvw);

    // if(hullCount == 0)
    // {
    //     gOutput[coord] = float4(1.0, 1.0, 1.0, 1.0);
    //     return;
    // }

    bool collision = false;
    for(uint i = 0; i < hullCount; ++i)
    {
        float hullEnter, hullExit;
        ConvexHull ch = hulls[i];
        float3 dir = float3(1.0, 1.0, 1.0);
        if (PointInsideConvexHull(worldPos, ch))
        {
            gOutput[coord] = float4(0.0, 1.0, 0.0, 0.0);
            collision = true;
            break;
        }
    }

    if(!collision)
    {
        gOutput[coord] = float4(0.0, max(gOutput[coord].g - 0.01, 0.0), 1.0, 1.0);
    }
}