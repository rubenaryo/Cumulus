/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Resources needed for a single frame
----------------------------------------------*/
#include <Core/FrameResources.h>
#include <Core/CBufferStructs.h>
#include <Core/ResourceCodex.h>
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
    if (mapped)
    {
        using namespace DirectX;
        cbPerEntity entity;

        const float PI = 3.14159f;
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
    UpdateAtmosphereConstants(atmosphereParams, width, height);

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

    mTimeBuffer.Create(L"Time", sizeof(cbTime));

	return true;
}

void FrameResources::Update(float totalTime, float deltaTime)
{
    Muon::cbLights lights;

    lights.ambientColor = DirectX::XMFLOAT3A(+1.0f, +0.772f, +0.56f);
    lights.directionalLight.diffuseColor = DirectX::XMFLOAT3A(1.0, 1.0, 1.0);
    lights.directionalLight.dir = DirectX::XMFLOAT3A(0, 1, 0);

    // TODO: This should just come from invView in the shader
    //DirectX::XMStoreFloat3(&lights.cameraWorldPos, mCamera.GetPosition());

    UINT8* mapped = mLightBuffer.GetMappedPtr();

    if (mapped)
        memcpy(mapped, &lights, sizeof(Muon::cbLights));


    Muon::cbTime time;
    time.totalTime = totalTime;
    time.deltaTime = deltaTime;

    UINT8* timeBuf = mTimeBuffer.GetMappedPtr();
    if (timeBuf)
        memcpy(timeBuf, &time, sizeof(Muon::cbTime));
}

void FrameResources::Destroy()
{
	mCmdAllocator.Reset();
	mWorldMatrixBuffer.Destroy();
	mLightBuffer.Destroy();
	mTimeBuffer.Destroy();
	mAABBBuffer.Destroy();
	mAtmosphereBuffer.Destroy();
}


}