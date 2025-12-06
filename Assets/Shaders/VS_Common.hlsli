/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/11
Description : Common Functions and Constant Buffers shared across ALL vertex shaders
----------------------------------------------*/
#ifndef VS_COMMON_HLSLI
#define VS_COMMON_HLSLI

// Basic camera matrix passed in every frame
cbuffer VSCamera : register(b10)
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4x4 invView;
    float4x4 invProj;
}

struct cbPerEntity
{
    float4x4 world;
    float4x4 invWorld;
    int hullIdx;
    int padding[3];
};

cbuffer VSWorld : register(b11)
{
    float4x4 world;
    float4x4 invWorld;
    int hullIdx;
    int padding[3];
}

cbuffer entities : register(b12)
{
    cbPerEntity entities[64];

    int entityCount;
    int paddingEntities[3];
}

#endif