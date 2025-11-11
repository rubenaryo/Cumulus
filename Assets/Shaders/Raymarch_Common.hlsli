/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/11
Description : Common Functions and Constant Buffers shared across ALL vertex shaders
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
 //   float padding[3]; // Pad to 16 bytes
	AABB aabbs[1];
};
#endif