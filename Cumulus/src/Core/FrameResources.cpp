/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Resources needed for a single frame
----------------------------------------------*/
#include <Core/FrameResources.h>
#include <Core/CBufferStructs.h>
#include <Core/ResourceCodex.h>
#include <Core/MuonImgui.h>
#include <Core/Mesh.h>
#include <Core/Hull.h>
#include <Utils/AtmosphereUtils.h>
#include <Utils/Utils.h>
#include <assert.h>

namespace Muon
{

FrameResources::FrameResources()
{
}

bool FrameResources::Create(UINT width, UINT height)
{
	ID3D12Device* pDevice = Muon::GetDevice();
	if (!pDevice)
	{
		return false;
	}

	HRESULT hr = pDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mCmdAllocator.GetAddressOf()));

	if (FAILED(hr))
	{
		Printf(L"Error: Failed to create command allocator for frame resources\n");
		return false;
	}

    mWorldMatrixBuffer.Create(L"world matrix buffer", sizeof(cbPerEntity));
    UINT8* mapped = mWorldMatrixBuffer.GetMappedPtr();
    assert(mapped);
    const float PI = 3.14159f;
    if (mapped)
    {
        using namespace DirectX;
        cbPerEntity entity;

        XMMATRIX entityWorld = DirectX::XMMatrixIdentity();
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixRotationRollPitchYaw(0, 0, PI / 2.0f));
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixRotationRollPitchYaw(-PI / 2.0f, 0, 0));
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixScaling(0.12f, 0.12f, 0.12f));
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixTranslation(0, 1, 0));
        XMStoreFloat4x4(&entity.world, entityWorld);
        XMStoreFloat4x4(&entity.invWorld, DirectX::XMMatrixInverse(nullptr, entityWorld));
        memcpy(mapped, &entity, sizeof(entity));
    }

    mLightBuffer.Create(L"Light Buffer", sizeof(cbLights));
    mTimeBuffer.Create(L"Time", sizeof(cbTime));
    mAtmosphereBuffer.Create(L"Atmosphere CB", sizeof(cbAtmosphere));

    cbAtmosphere atmosphereParams;
    InitializeAtmosphereConstants(atmosphereParams, width, height);

    mapped = mAtmosphereBuffer.GetMappedPtr();
    if (mapped)
    {
        memcpy(mapped, &atmosphereParams, sizeof(cbAtmosphere));
    }

    mAABBBuffer.Create(L"AABB Buffer", sizeof(cbIntersections));

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    const Mesh* m = codex.GetMesh(Muon::GetResourceID(L"cube.obj"));

    cbIntersections intersections = {};
    intersections.aabbCount = 1;
    intersections.aabbs[0] = m->GetAABB();
    if (m) {
        UINT8* aabbPtr = mAABBBuffer.GetMappedPtr();
        if (aabbPtr) {
            memcpy(aabbPtr, &intersections, sizeof(intersections));
        }
    }
    //HULL:
    //todo: concat all hulls
    Hull h = m->GetHull();
    mHullBuffer.Create(L"Hull Buffer", sizeof(cbHulls));

    if (m && mHullBuffer.GetMappedPtr())
    {
        cbHulls hulls = {};
        cbConvexHull cHull = {};
        cHull.faceCount = (uint32_t)h.faces.size();
        cHull.faceOffset = 0;

        DirectX::XMMATRIX debugEntityWorld = DirectX::XMMatrixIdentity();
        debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixRotationRollPitchYaw(0, 0, PI / 2.0f));
        debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixRotationRollPitchYaw(-PI / 2.0f, 0, 0));
        debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixScaling(0.12f, 0.12f, 0.12f));
        debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixTranslation(0, 1, 0));

        XMStoreFloat4x4(&cHull.world, debugEntityWorld);
        XMStoreFloat4x4(&cHull.invWorld, DirectX::XMMatrixInverse(nullptr, debugEntityWorld));

        hulls.hulls[0] = cHull;
        hulls.hullCount = 1;
        memcpy(mHullBuffer.GetMappedPtr(), &hulls, sizeof(hulls));
    }

    mHullFaceBuffer.Create(L"Hull Faces Buffer", sizeof(cbHullFaces));
    if (m && h.faces.size() > 0) {
        UINT8* facePtr = mHullFaceBuffer.GetMappedPtr();
        cbHullFaces faces = {};

        for (size_t i = 0; i < h.faces.size(); i++)
        {
            faces.faces[i] = DirectX::XMFLOAT4(
                h.faces[i].normal.x,
                h.faces[i].normal.y,
                h.faces[i].normal.z,
                h.faces[i].distance
            );
        }

        if (facePtr) {
            memcpy(facePtr, &faces, sizeof(faces));
        }
    }

    mTimeBuffer.Create(L"Time", sizeof(cbTime));

	return true;
}

void FrameResources::Update(float totalTime, float deltaTime, Muon::SceneSettings& settings, Muon::Camera& camera)
{
    // Updating Lights
    Muon::cbLights lights;
    lights.ambientColor = DirectX::XMFLOAT3A(+1.0f, +0.772f, +0.56f);
    lights.directionalLight.diffuseColor = DirectX::XMFLOAT3A(1.0, 1.0, 1.0);
    lights.directionalLight.dir = DirectX::XMFLOAT3A(0, 1, 0);

    // TODO: This should just come from invView in the shader
    //DirectX::XMStoreFloat3(&lights.cameraWorldPos, mCamera.GetPosition());

    UINT8* mapped = mLightBuffer.GetMappedPtr();

    if (mapped)
        memcpy(mapped, &lights, sizeof(Muon::cbLights));

    // Updating Time
    Muon::cbTime time;
    time.totalTime = totalTime;
    time.deltaTime = deltaTime;

    UINT8* timeBuf = mTimeBuffer.GetMappedPtr();
    if (timeBuf)
        memcpy(timeBuf, &time, sizeof(Muon::cbTime));

    // Updating Atmosphere
    Muon::cbAtmosphere atmosphereParams;
    Muon::UpdateAtmosphere(atmosphereParams, camera, settings.isSunDynamic, settings.timeOfDay, time.totalTime);
    settings.sunDir = atmosphereParams.sun_direction;
    mapped = mAtmosphereBuffer.GetMappedPtr();
    if (mapped)
    {
        memcpy(mapped, &atmosphereParams, sizeof(Muon::cbAtmosphere));
    }

}

void FrameResources::Destroy()
{
    mCmdAllocator.Reset();
    mWorldMatrixBuffer.Destroy();
    mLightBuffer.Destroy();
    mTimeBuffer.Destroy();
    mAABBBuffer.Destroy();
    mAtmosphereBuffer.Destroy();
    mHullBuffer.Destroy();
    mHullFaceBuffer.Destroy();
}


}