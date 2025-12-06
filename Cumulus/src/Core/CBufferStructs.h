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
#include <Utils/Utils.h>

namespace Muon
{

struct alignas(16) cbCamera
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4X4 invView;
    DirectX::XMFLOAT4X4 invProj;

    float minDist = 0.01f; 
    float maxDist = 4000.0f; 
	float camPad0[2];
};

struct alignas(16) cbPerEntity
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 invWorld;
    int hullIdx;
    int padding[3];
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
    uint32_t buffer1;
    uint32_t buffer2;

    uint32_t faceOffset;
    uint32_t faceCount;

    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 invWorld;
};

struct alignas(16) cbHulls
{
    uint32_t hullCount;
    cbConvexHull hulls[12];
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

struct alignas(16) cbCloudGenData
{
    DirectX::XMFLOAT4 seeds[16];
    
    int numSeeds;

    //0 is no demo
    //1 is draw jet
    //2 is draw balls
    int demoMode = 2;
    float pad[2];
};

struct alignas(16) cbCloudLighting
{
    // === Raymarch settings ===
    int   maxSteps = 256;
    float densityScale = 1.0f;
    float minTransmittance = 0.01f;
    float pad0 = 0.0f; 

    // === Sun / primary lighting ===
    DirectX::XMFLOAT3 dirSun = { 0.0f, 1.0f, 0.0f };
    float             directExtinctionScale = 0.02f;

    float             directStrength = 1.0f; 
    DirectX::XMFLOAT3 lightSun = { 220.0f, 240.0f, 250.0f };

    // === Secondary ===
    DirectX::XMFLOAT3 secondaryColor = SrgbToLinear3( 1.0f, 0.96f, 0.9f );
    float             secondaryStrength = 2.0f;

    float secondaryExtinctionScale = 0.02f;
    float pad2[3] = { 0.0f, 0.0f, 0.0f };

    // === Ambient ===
    DirectX::XMFLOAT3 ambientColor = SrgbToLinear3( 1.0f, 0.96f, 0.9f );
    float             ambientExtinctionScale = 0.05f;

    float ambientStrength = 1.0f;
    float pad3[3] = { 0.0f, 0.0f, 0.0f };
};


struct alignas(16) cbJetTrailPositions {
    DirectX::XMFLOAT4 positions[4];
};

struct EntityData {
    cbPerEntity entityMatrices;
    int hullIdx;
    Muon::ResourceID resourceID;
    DirectX::XMFLOAT4 vel;
    DirectX::XMFLOAT4 rot;
};

const static int MAX_ENTITY_COUNT = 64;
struct alignas(256) cbEntities {
    cbPerEntity entities[MAX_ENTITY_COUNT];

    int entityCount;
    int padding[3];
};

}


#endif