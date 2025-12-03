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
    mLightBuffer.Create(L"Light Buffer", sizeof(cbLights));
    mTimeBuffer.Create(L"Time", sizeof(cbTime));
    mAtmosphereBuffer.Create(L"Atmosphere CB", sizeof(cbAtmosphere));
    mCloudGenBuffer.Create(L"CloudGen CG", sizeof(cbCloudGenData));
    mAABBBuffer.Create(L"AABB Buffer", sizeof(cbIntersections));
    mHullBuffer.Create(L"Hull Buffer", sizeof(cbHulls));
    mHullFaceBuffer.Create(L"Hull Faces Buffer", sizeof(cbHullFaces));

	return true;
}

bool FrameResources::UpdateEntities(cbPerEntity& data)
{
    memcpy(mWorldMatrixBuffer.GetMappedPtr(), &data, sizeof(cbPerEntity));
    return true;
}

bool FrameResources::UpdateLights(cbLights& data)
{
    memcpy(mLightBuffer.GetMappedPtr(), &data, sizeof(cbLights));
    return true;
}

bool FrameResources::UpdateTime(cbTime& data)
{
    memcpy(mTimeBuffer.GetMappedPtr(), &data, sizeof(cbTime));
    return true;
}

bool FrameResources::UpdateAABB(cbIntersections& data)
{
    memcpy(mAABBBuffer.GetMappedPtr(), &data, sizeof(cbIntersections));
    return true;
}

bool FrameResources::UpdateAtmosphere(cbAtmosphere& data)
{
    memcpy(mAtmosphereBuffer.GetMappedPtr(), &data, sizeof(cbAtmosphere));
    return true;
}

bool FrameResources::UpdateCloudData(cbCloudGenData& data)
{
    memcpy(mCloudGenBuffer.GetMappedPtr(), &data, sizeof(cbCloudGenData));
    return true;
}

bool FrameResources::UpdateHulls(cbHulls& data)
{
    memcpy(mHullBuffer.GetMappedPtr(), &data, sizeof(cbHulls));
    return true;
}

bool FrameResources::UpdateHullFaces(cbHullFaces& data)
{
    memcpy(mHullFaceBuffer.GetMappedPtr(), &data, sizeof(cbHullFaces));
    return true;
}

void FrameResources::Destroy()
{
    mCmdAllocator.Reset();
    mWorldMatrixBuffer.Destroy();
    mLightBuffer.Destroy();
    mTimeBuffer.Destroy();
    mAABBBuffer.Destroy();
    mAtmosphereBuffer.Destroy();
    mCloudGenBuffer.Destroy();
    mHullBuffer.Destroy();
    mHullFaceBuffer.Destroy();
}


}