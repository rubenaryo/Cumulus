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
#endif