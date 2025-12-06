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
#include <Utils/Utils.h>
#include <assert.h>
#include <cmath>

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
    mCameraBuffer.Create(L"Camera CB", sizeof(cbCamera));
    mLightBuffer.Create(L"Light Buffer", sizeof(cbLights));
    mTimeBuffer.Create(L"Time", sizeof(cbTime));
    mAtmosphereBuffer.Create(L"Atmosphere CB", sizeof(cbAtmosphere));
    mCloudGenBuffer.Create(L"CloudGen CG", sizeof(cbCloudGenData));
    mCloudLightingBuffer.Create(L"CloudLighting CB", sizeof(cbCloudLighting));
    mAABBBuffer.Create(L"AABB Buffer", sizeof(cbIntersections));
    mHullBuffer.Create(L"Hull Buffer", sizeof(cbHulls));
    mHullFaceBuffer.Create(L"Hull Faces Buffer", sizeof(cbHullFaces));
    mTimeBuffer.Create(L"Time", sizeof(cbTime));
    mJetTrailBuffer.Create(L"Jet Trail", sizeof(cbJetTrailPositions));
    mEntitiesBuffer.Create(L"Entities", sizeof(cbEntities));
	return true;
}

bool FrameResources::UpdateWorldMatrix(const cbPerEntity& data)
{
    memcpy(mWorldMatrixBuffer.GetMappedPtr(), &data, sizeof(cbPerEntity));
    return true;
}

bool FrameResources::UpdateCamera(const cbCamera& data)
{
    memcpy(mCameraBuffer.GetMappedPtr(), &data, sizeof(cbCamera));
    return true;
}

bool FrameResources::UpdateLights(const cbLights& data)
{
    memcpy(mLightBuffer.GetMappedPtr(), &data, sizeof(cbLights));
    return true;
}

bool FrameResources::UpdateTime(const cbTime& data)
{
    memcpy(mTimeBuffer.GetMappedPtr(), &data, sizeof(cbTime));
    return true;
}

bool FrameResources::UpdateAABB(const cbIntersections& data)
{
    memcpy(mAABBBuffer.GetMappedPtr(), &data, sizeof(cbIntersections));
    return true;
}

bool FrameResources::UpdateAtmosphere(const cbAtmosphere& data)
{
    memcpy(mAtmosphereBuffer.GetMappedPtr(), &data, sizeof(cbAtmosphere));
    return true;
}

bool FrameResources::UpdateCloudData(const cbCloudGenData& data)
{
    memcpy(mCloudGenBuffer.GetMappedPtr(), &data, sizeof(cbCloudGenData));
    return true;
}

bool FrameResources::UpdateCloudLighting(const cbCloudLighting& data)
{
    memcpy(mCloudLightingBuffer.GetMappedPtr(), &data, sizeof(cbCloudLighting));
    return true;
}

bool FrameResources::UpdateHulls(const cbHulls& data)
{
    memcpy(mHullBuffer.GetMappedPtr(), &data, sizeof(cbHulls));
    return true;
}

bool FrameResources::UpdateHullFaces(const cbHullFaces& data)
{
    memcpy(mHullFaceBuffer.GetMappedPtr(), &data, sizeof(cbHullFaces));
    return true;
}

bool FrameResources::UpdateJetTrail(const cbJetTrailPositions& data)
{
    memcpy(mJetTrailBuffer.GetMappedPtr(), &data, sizeof(cbJetTrailPositions));
    return false;
}

bool FrameResources::UpdateEntities(const std::vector<EntityData>& cpuEntities)
{
    cbEntities bufferEntities{};
    for (int i = 0; i < cpuEntities.size() && i < 64; ++i) {
        bufferEntities.entities[i].hullIdx = cpuEntities[i].hullIdx;
        bufferEntities.entities[i].world = cpuEntities[i].entityMatrices.world;
        bufferEntities.entities[i].invWorld = cpuEntities[i].entityMatrices.invWorld;
    }
    bufferEntities.entityCount = cpuEntities.size();

    memcpy(mEntitiesBuffer.GetMappedPtr(), &bufferEntities, sizeof(cbEntities));
    return false;
}

void FrameResources::Destroy()
{
    mCmdAllocator.Reset();
    mEntitiesBuffer.Destroy();
    mWorldMatrixBuffer.Destroy();
    mCameraBuffer.Destroy();
    mLightBuffer.Destroy();
    mTimeBuffer.Destroy();
    mAABBBuffer.Destroy();
    mAtmosphereBuffer.Destroy();
    mCloudGenBuffer.Destroy();
    mCloudLightingBuffer.Destroy();
    mHullBuffer.Destroy();
    mHullFaceBuffer.Destroy();
    mJetTrailBuffer.Destroy();
}
}