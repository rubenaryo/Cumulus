RWTexture3D<float4> gOutput : register(u0);

cbuffer cbCloudGenBuffer : register(b6)
{
    float4 seeds[32];
    
    int numSeeds;
    float pad[3];
};

// Texture output: 
// r - sdf distance - how far we are from the cloud
// g - dimensionalProfile - this is what eli's writing to? Need to merge these well
// b - detail type - aka billowy vs whispy [0, 1]
// a - density scale - where is cloud [0, 1]

#include "Raymarch_Common.hlsli"

float smooth_min(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

float SDF_Sphere(float3 query, float3 center, float radius)
{
    return length(query - center) - radius;
}

// p is query
// a is left point, b is right
// I removed thickness (w) to be dependent on length now
float SDF_VesicaSegment(float3 p,float3 a,float3 b, float w)
{
    float3 c = (a + b) * 0.5;
    float l = length(b - a);
    float3 v = (b - a) / l;
    float y = dot(p - c, v);
    float2 q = float2(length(p - c - y * v), abs(y));
    
    float r = 0.5 * l;
    float d = 0.5 * (r * r - w * w) / w;
    float3 h = (r * q.x < d * (q.y - r)) ? float3(0.0, r, 0.0) : float3(-d, 0.0, d + w);
 
    return length(q - h.xy) - h.z;
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
                             max(WorleyNoise3D(cone_a, 12) * scale, 0.3), max(WorleyNoise3D(cone_b, 16) * scale, 0.3));
    d = smooth_min(d, cone, 0.8);
    float3 midpoint = (cone_a + cone_b) * 0.5;
    for (int i = 0; i < numSeed; ++i)
    {
        float3 offset_a = hash3(float3(i * 4.12, random(numSeed), i * numSeed * 0.77) * 3.1415) * 2.4 - 1.2;
        float3 offset_b = hash3(float3(random(i * 4.12), 1.0 - random(numSeed), i * numSeed * 347.77) * 42.1415) * 2.4 - 1.2;
        //float sphere = SDF_Sphere(query, midpoint + offset * scale * 0.8, (WorleyNoise3D(midpoint + offset, 12) * 0.9 + 0.2) * scale);
        
        float3 a = midpoint + offset_a * scale;
        float3 b = midpoint + offset_b * scale;
        float w = scale * 0.5f;
        float sphere = SDF_VesicaSegment(query, a, b, w);
        d = smooth_min(d, sphere, 0.7);
    }
    return d;
}
#define TEST 1
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
    
    //------------------------------
    // CLOUD GEN
    //------------------------------
    // SDF is getting clouds around the given seeds
    float d = 999999999.f;
    //if (numSeeds != 0)
    //{
#if TEST
    float3 center = (VOLUME_MIN_WS + VOLUME_MAX_WS) * 0.5;
    float scale = 125.f;
    d = smooth_min(d, SDF_Cloud(worldPos, 5, center, scale), 0.8);  // this is our actual SDF result
    const float sdfMin = -256.0;
    const float sdfMax = 4096.0;
    // we offset d by scale and a bit, so we can add more detail around the actual sdf value
    float encodedSdf = ((d - scale) - sdfMin) / (sdfMax - sdfMin);
    encodedSdf = saturate(encodedSdf);
    gOutput[coord].r = encodedSdf;
    
    // normalize scale distance
    float norm_scale = d / scale;
    float billow = fbm_3D_BillowNoise(worldPos * 0.009, float3(6.0, 6.0, 6.0), 12);
    gOutput[coord].g = d > scale ? 0.0 : billow < norm_scale ? 0.0 : billow * (1.0 - norm_scale);
    // way too noisy
    //gOutput[coord].b = d <= -0.1 ? 0.0 : clamp(cnoise(worldPos * 0.01), 0.2, 1.0);
    float normalized_height = (worldPos.y - VOLUME_MIN_WS.y) / (VOLUME_MAX_WS.y - VOLUME_MIN_WS.y) + 0.1;
    float3 g = float3(0.0, 0.0, 0.0);
    //gOutput[coord].b = d > scale + 10 ? 0.0 : lerp(0.0, psrdnoise(worldPos * 0.01, float3(6.0, 6.0, 6.0), 1.0, g), (d + scale) * 0.01 + normalized_height);
    //gOutput[coord].b = lerp(0.0, smoothstep(0.0, 0.9, fbm_3D_BillowNoise(worldPos * 0.007, float3(6.0, 6.0, 6.0), 2)), (1.0 - (scale * 0.1 - abs(d)) / (scale * 0.1f)) * normalized_height);
    gOutput[coord].b = fbm_3D_BillowNoise(worldPos * 0.007, float3(6.0, 6.0, 6.0), 2) * normalized_height;
    
#else
    for (uint i = 0; i < numSeeds; ++i)
    {
        d = smooth_min(d, SDF_Cloud(worldPos, i % 4 + 2, seeds[i].xyz, 50.f), 0.8);
    }
    // density scale is just a worley noise for now
    // might need to edit the noise for this to be right
    gOutput[coord].g = clamp(-d, 0.0, 1.0) * WorleyNoise3D(worldPos, 64);
    // density itself needs to start a bit inside the sdf to look good
    gOutput[coord].a = d < -5.0 ? hash3(worldPos) : 0.0f;
    const float sdfMin = -256.0;
    const float sdfMax = 4096.0;
    float encodedSdf = (d - sdfMin) / (sdfMax - sdfMin);
    encodedSdf = clamp(encodedSdf, 0.0, 1.0);
    gOutput[coord].r = encodedSdf;
    // detail type for now fully depends on sdf
    // curently its a quadratic fall off to be more billowy the deeper we are
    // is 0 if we're too far away from the cloud
    float norm_height = (worldPos.z - VOLUME_MIN_WS.z) / (VOLUME_MAX_WS.z - VOLUME_MIN_WS.z);
    gOutput[coord].b = (random(gOutput[coord].a) + 0.1) * norm_height;
    //gOutput[coord].b = 1.0;
#endif
    
    //---------------------------
    // COLLISION CODE
    //---------------------------

    bool collision = false;
    for (uint i = 0; i < hullCount; ++i)
    {
        float hullEnter, hullExit;
        ConvexHull ch = hulls[i];
        float3 dir = float3(1.0, 1.0, 1.0);
        if (PointInsideConvexHull(worldPos, ch))
        {
            gOutput[coord].a = 1.0f;
            collision = true;
            break;
        }
    }

    if (!collision)
    {
        gOutput[coord].a = max(gOutput[coord].a - 0.01, 0.0);

    }
}