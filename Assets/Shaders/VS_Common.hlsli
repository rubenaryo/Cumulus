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
    
    float minDist; 
    float maxDist; 
    float2 camPad0;
}

cbuffer VSWorld : register(b11)
{
    float4x4 world;
    float4x4 invWorld;
    int hullIdx;
    float padVS1;
    float padVS2;
    float padVS3;
}

struct cbPerEntity
{
    float4x4 world;
    float4x4 invWorld;
    int hullIdx;
    float padEntity1;
    float padEntity2;
    float padEntity3;
    float paddTrue[7];
    float padHelp1;
    float padHelp2;
    float padHelp3;
};


cbuffer EntityBuffer : register(b12)
{        
    cbPerEntity entities[64];
    int entityCount;
    float padEntityCount1;
    float padEntityCount2;
    float padEntityCount3;
}

#endif