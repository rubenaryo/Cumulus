RWTexture3D<float4> gOutput : register(u0);

cbuffer cbCloudGenBuffer : register(b6)
{
    float4 seeds[32];
    
    int numSeeds;
    int demoMode;
    float pad[2];
};

cbuffer cbJetBuffer : register(b7)
{
    float4 positions[4];
}

cbuffer Time : register(b8)
{
    float totalTime;
    float deltaTime;
}

// Texture output: 
// r - sdf distance - how far we are from the cloud
// g - dimensionalProfile - this is what eli's writing to? Need to merge these well
// b - detail type - aka billowy vs whispy [0, 1]
// a - density scale - where is cloud [0, 1]

#include "Raymarch_Common.hlsli"
#include "VS_Common.hlsli"
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
    float gMultiplier = 1.0;
    float scale = 0;
    float jetSpeed = 0;
    if (demoMode == 0)
    {
        for (uint i = 0; i < numSeeds; ++i)
        {
            float4 curr = seeds[i];
            scale += curr.a;
            d = smooth_min(d, SDF_Cloud(worldPos, i % 4 + 3, curr.xyz, curr.a), 0.8);
        }
        scale /= numSeeds;  // scale is an average of all scales
    }

    
    else if (demoMode == 1)
    {
        uint numTrails = 1;

        float3 jetStartOffset1 = float3(15.0, -5.0, 14.0);
        float3 jetStartOffset2 = float3(15.0, -5.0, -14.0);

        for (uint i = 0; i < numTrails; ++i)
        {
            float rightLine = SDF_RoundCone(worldPos, positions[2 * i].xyz + jetStartOffset1, positions[2 * i + 1].xyz + jetStartOffset1, positions[2 * i].w, positions[2 * i + 1].w);
            float leftLine = SDF_RoundCone(worldPos, positions[2 * i].xyz + jetStartOffset2, positions[2 * i + 1].xyz + jetStartOffset2, positions[2 * i].w, positions[2 * i + 1].w);
            float curr = min(rightLine, leftLine);

            d = smooth_min(d, curr, 0.8);

            float jetTravelDistance = length(positions[2 * i + 1].xyz - positions[2 * i].xyz);
            jetSpeed = jetTravelDistance / totalTime;
            float distanceFromJet = length(worldPos - positions[2 * i + 1].xyz);
            float timeAlive = distanceFromJet / max(jetSpeed, 0.0001);
            float lifeTime = 25.0;


            float currentDensityMultiplier = (1.0 - saturate(timeAlive / lifeTime));

            scale += (positions[2 * i].a + positions[2 * i + 1].a) * 0.5f;

            float disStart = length(worldPos - positions[2 * i].xyz);
            float disEnd = length(worldPos - positions[2 * i + 1].xyz);

            gMultiplier = lerp(0.15, 1.0, currentDensityMultiplier);
        }
        scale /= numTrails;
    }
    // We then encode SDF into the range [0, 1] from [-256, 4096], as this is what Nubis expects
    const float sdfMin = -256.0;
    const float sdfMax = 4096.0;
    // We offset d by scale so we can add a bit more  detail around the harsh sdf edges
    float encodedSdf = ((d - scale) - sdfMin) / (sdfMax - sdfMin);
    encodedSdf = saturate(encodedSdf);
    gOutput[coord].r = encodedSdf; // r is sdf output
    
    // g is the cloud's detail - aka its actual form and outline
    // to get it, we calculate billowy noise with 12 iterations of fbm
    // it also slowly fades out based on distance from d by scale
    float norm_scale = d / scale;
    // fade out as we get closer to the edge
    //float norm_edge_dist = DistToEdge(worldPos) / scale;
    float billow = d > scale ? 0.0 : fbm_3D_BillowNoise(worldPos * 0.009 * (scale / 250.f), float3(6.0, 6.0, 6.0), 12);
    gOutput[coord].g = billow < norm_scale ? 0.0 : billow * (1.0 - norm_scale);
    // b is detail type, which is a bit larger billows that get attenuated by height, as higher parts are more whispy
    float normalized_height = (worldPos.y - VOLUME_MIN_WS.y) / (VOLUME_MAX_WS.y - VOLUME_MIN_WS.y) + 0.3;
    gOutput[coord].b = d > scale * 1.5 ? 0.0 : fbm_3D_BillowNoise(worldPos * 0.007 * (scale / 250.f), float3(6.0, 6.0, 6.0), 3) * normalized_height;
    gOutput[coord].g *= gMultiplier;
    //---------------------------
    // COLLISION CODE
    //---------------------------
    // collision gets put into the density scale part for now, which gets calculated in raymarch for now
    bool collision = false;
    for (uint i = 0; i < entityCount; ++i)
    {
        cbPerEntity entity = entities[i];
        
        //no collision:
        if (entity.hullIdx < 0) continue;

        float hullEnter, hullExit;
        ConvexHull ch = hulls[entity.hullIdx];
        float3 dir = float3(1.0, 1.0, 1.0);
        if (PointInsideConvexHull(worldPos, ch, entity.world))
        {
            gOutput[coord].a = 1.0f;
            collision = true;
            break;
        }
    }

    if (!collision)
    {
        gOutput[coord].a = max(gOutput[coord].a - 0.01 * deltaTime, 0.0);
    }
}