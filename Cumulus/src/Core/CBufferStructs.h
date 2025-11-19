/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/2
Description : Declation of structs used as constant buffers by various shaders
----------------------------------------------*/
#ifndef CBUFFERSTRUCTS_H
#define CBUFFERSTRUCTS_H

#include <DirectXMath.h>
#include <DirectXColors.h>
#include <Core/CommonTypes.h>

namespace Muon
{

struct alignas(16) cbCamera
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4X4 invView;
    DirectX::XMFLOAT4X4 invProj;
};

struct alignas(16) cbPerEntity
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 invWorld;
};

struct alignas(16) DirectionalLight
{
    DirectX::XMFLOAT3 diffuseColor;
    DirectX::XMFLOAT3 dir;
};

struct alignas(16) cbLights
{
    DirectX::XMFLOAT3A ambientColor;
    DirectionalLight directionalLight;
    DirectX::XMFLOAT3A cameraWorldPos;
};

struct alignas(16) cbMaterialParams
{
    DirectX::XMFLOAT4  colorTint = DirectX::XMFLOAT4(DirectX::Colors::Black);
    float              specularExp = 0.0f;
};

struct alignas(16) cbTime
{
    float totalTime = 0;
    float deltaTime = 0;
};

struct alignas(256) cbIntersections
{
    uint32_t aabbCount;
    AABB aabbs[1];
};

struct alignas(16) cbConvexHull
{
    uint32_t pointOffset;
    uint32_t pointCount;

    uint32_t faceOffset;
    uint32_t faceCount;

    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 invWorld;
};

struct alignas(16) cbHulls
{
    uint32_t hullCount;
    cbConvexHull hulls[1];
};


struct alignas(16) cbHullFaces
{
    DirectX::XMFLOAT4 faces[1024];
};

struct alignas(16) cbHullPoints
{
    DirectX::XMFLOAT3A points[2048];
};

struct alignas(16) cbAtmosphere
{
    DirectX::XMFLOAT4X4 view_from_clip;      // Inverse projection matrix
    DirectX::XMFLOAT4X4 model_from_view;     // Inverse view matrix
    
    DirectX::XMFLOAT3 camera_position;
    float pad0;
    
    DirectX::XMFLOAT3 earth_center;
    float pad1;
    
    DirectX::XMFLOAT3 sun_direction;
    float pad2;
    
    DirectX::XMFLOAT2 sun_size;
    float exposure;
    int32_t isCamUp;
    
    DirectX::XMFLOAT3 white_point;
    float pad3;
};

}

#endif