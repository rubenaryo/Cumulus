RWTexture3D<float4> gOutput : register(u0);

cbuffer cbCloudGenBuffer : register(b6)
{
    float3 seeds[32];
    
    int numSeeds;
    float pad[3];
};

// Texture output: 
// r - sdf distance - how far we are from the cloud
// g - dimensionalProfile - this is what eli's writing to? Need to merge these well
// b - detail type - aka billowy vs whispy [0, 1]
// a - density scale - where is cloud [0, 1]

#include "Raymarch_Common.hlsli"

float dot2(in float3 v)
{
    return dot(v, v);
}

float3 random3(float3 p)
{
    return frac(sin(float3(dot(p, float3(127.1, 311.7, 191.999)),
                          dot(p, float3(269.5, 183.3, 765.54)),
                          dot(p, float3(420.69, 631.2, 109.21))))
                 * 43758.5453);
}

float WorleyNoise3D(float3 p)
{
    p *= 30;
    // Tile the space
    float3 pointInt = floor(p);
    float3 pointFract = frac(p);

    float minDist = 1.0; // Minimum distance initialized to max.

    // Search all neighboring cells and this cell for their point
    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                float3 neighbor = float3(float(x), float(y), float(z));

                // Random point inside current neighboring cell
                float3 pt = random3(pointInt + neighbor);

                // Compute the distance b/t the point and the fragment
                // Store the min dist thus far
                float3 diff = neighbor + pt - pointFract;
                float dist = length(diff);
                minDist = min(minDist, dist);
            }
        }
    }
    return minDist;
}

float hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

float3 hash3(float3 p)
{
    return float3(
        hash(p),
        hash(p + float3(1.0, 0.0, 0.0)),
        hash(p + float3(0.0, 1.0, 0.0))
    );
}

float smooth_min(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

float SDF_Sphere(float3 query, float3 center, float radius)
{
    return length(query - center) - radius;
}

float SDF_RoundCone(float3 p, float3 a, float3 b, float r1, float r2)
{
    float3 ba = b - a;
    float l2 = dot(ba, ba);
    float rr = r1 - r2;
    float a2 = l2 - rr * rr;
    float il2 = 1.0 / l2;

    float3 pa = p - a;
    float y = dot(pa, ba);
    float z = y - l2;
    float x2 = dot2(pa * l2 - ba * y);
    float y2 = y * y * l2;
    float z2 = z * z * l2;

  // single square root!
    float k = sign(rr) * rr * rr * x2;
    if (sign(z) * a2 * z2 > k)
        return sqrt(x2 + z2) * il2 - r2;
    if (sign(y) * a2 * y2 < k)
        return sqrt(x2 + y2) * il2 - r1;
    return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

float SDF_Cloud(float3 query, int numSeed, float3 origin, float scale)
{
    float d = 9999999.f;
    float3 cone_a = float3(origin.x - 2 * hash(origin) * scale, origin.y, origin.z + hash(origin) * scale);
    float3 cone_b = float3(origin.x + 3 * (1.0 - hash(origin)) * scale, origin.y, origin.z - hash(origin) * scale);
    float cone = SDF_RoundCone(query, cone_a, cone_b,
                             max(WorleyNoise3D(cone_a) * scale, 0.3), max(WorleyNoise3D(cone_b) * scale, 0.3));
    d = smooth_min(d, cone, 0.8);
    float3 midpoint = (cone_a + cone_b) * 0.5;
    for (int i = 0; i < numSeed; ++i)
    {
        float3 offset = hash3(float3(i, numSeed * 0.76734, i * numSeed * 0.2784)) * 3.6 - 1.0;
        float sphere = SDF_Sphere(query, midpoint + offset, (WorleyNoise3D(midpoint + offset) * 0.7 + 0.15) * scale);
        d = smooth_min(d, sphere, 0.7);
    }
    return d;
}

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
    
    //if (numSeeds == 0)
    //{
    //    gOutput[coord].g = 1.0;
    //    return;
    //}

    //------------------------------
    // CLOUD GEN
    //------------------------------
    // SDF is getting clouds around the given seeds
    float d = 999999999.f;
    if (numSeeds != 0)
    {
        for (uint i = 0; i < numSeeds; ++i)
        {
            d = smooth_min(d, SDF_Cloud(worldPos, i % 4 + 2, seeds[i], 1.2), 0.8);
        }
    }
    else
    {
        // if num seeds is 0 then we are bugged for now, so we need to get cloud positions here.
        // this is really jank and is just here to test stuff
        for (uint i = 0; i < 1; ++i)
        {
            float3 center = (VOLUME_MIN_WS + VOLUME_MAX_WS) * 0.5;
            float3 currPos = center;// random3(float3(i * 0.123, i * 0.456, i * 0.789)) * 5.f + center;
            d = smooth_min(d, SDF_Cloud(worldPos, 5, currPos, 100.f), 0.8);
            //d = SDF_Sphere(worldPos, center, 50.f);
        }
    }
    gOutput[coord].g = clamp(-d, 0.0, 1.0) * WorleyNoise3D(worldPos);
    const float sdfMin = -256.0;
    const float sdfMax = 4096.0;
    float encodedSdf = (d - sdfMin) / (sdfMax - sdfMin);
    encodedSdf = clamp(encodedSdf, 0.0, 1.0);
    gOutput[coord].r = encodedSdf;
    // detail type for now fully depends on sdf
    // curently its a quadratic fall off to be more billowy the deeper we are
    // is 0 if we're too far away from the cloud
    gOutput[coord].b = d < 0.5 ? clamp(abs(d * 0.5f) * abs(d * 0.5), 0.0, 1.0) : 0.0f;
    // density scale is just a worley noise for now
    // might need to edit the noise for this to be right
    gOutput[coord].a = d < 0.0001 ? hash3(worldPos) : 0.0f;
    
    //---------------------------
    // COLLISION CODE
    //---------------------------

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