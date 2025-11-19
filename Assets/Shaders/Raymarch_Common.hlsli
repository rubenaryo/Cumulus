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

    float4x4 world;
	float4x4 invWorld;
};

cbuffer HullsBuffer : register(b4)
{
	uint hullCount;
	ConvexHull hulls[1];
};

cbuffer HullPointsBuffer : register(b5)
{
	float4 hullPoints[2048];
};	

cbuffer HullFacesBuffer : register(b6)
{
	float4 hullFaces[1024];
};

#endif