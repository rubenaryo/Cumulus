/*----------------------------------------------
Eli Asimow (eliasimow@gmail.com)
Date : 2025/11
Description : Common Raymarching Structures for Collision
----------------------------------------------*/
#ifndef RAYMARCH_COMMON_HLSLI
#define RAYMARCH_COMMON_HLSLI

struct AABB
{
	float3 minBounds;
	float3 maxBounds;
};

cbuffer AABBBuffer : register(b3)
{
	uint aabbCount;
	AABB aabbs[1];
};

struct ConvexHull
{
    uint pointOffset;   // offset into HullPoints
    uint pointCount;

    uint faceOffset;    // offset into HullFaces
    uint faceCount;

    float3 aabbMin;
    float3 aabbMax;
};

struct HullFace
{
    float3 normal;
    float distance;     // plane equation: dot(normal, P) + distance = 0
};

// Buffers
StructuredBuffer<ConvexHull> Hulls      : register(t2);
StructuredBuffer<float3>     HullPoints : register(t3);
StructuredBuffer<HullFace>   HullFaces  : register(t4);

#endif