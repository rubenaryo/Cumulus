/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Resources needed for a single frame
----------------------------------------------*/
#ifndef MUON_FRAMERESOURCES_H
#define MUON_FRAMERESOURCES_H

#include <Core/DXCore.h>
#include <Core/Buffers.h>
#include <unordered_map>
#include <Core/CBufferStructs.h>

namespace Muon
{
    struct SceneSettings;
    class Camera;
}

namespace Muon
{

struct FrameResources
{
    FrameResources();
    
    bool Create(UINT width, UINT height);

    bool UpdateWorldMatrix(const cbPerEntity& data);
    bool UpdateCamera(const cbCamera& data);
    bool UpdateLights(const cbLights& data);
    bool UpdateTime(const cbTime& data);
    bool UpdateAABB(const cbIntersections& data);
    bool UpdateAtmosphere(const cbAtmosphere& data);
    bool UpdateCloudData(const cbCloudGenData& data);
    bool UpdateCloudLighting(const cbCloudLighting& data);
    bool UpdateHulls(const cbHulls& data);
    bool UpdateHullFaces(const cbHullFaces& data);
    bool UpdateJetTrail(const cbJetTrailPositions& jetTrailPositions);
    bool UpdateEntities(const std::vector<EntityData>& entities);

    void Destroy();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdAllocator;

    UploadBuffer mEntitiesBuffer;
    UploadBuffer mCameraBuffer;
    UploadBuffer mLightBuffer;
    UploadBuffer mTimeBuffer;
    UploadBuffer mAABBBuffer;
    UploadBuffer mAtmosphereBuffer;
    UploadBuffer mCloudGenBuffer;
    UploadBuffer mCloudLightingBuffer;
    UploadBuffer mHullBuffer;
    UploadBuffer mHullFaceBuffer;
    UploadBuffer mJetTrailBuffer;
    UploadBuffer mWorldMatrixBuffer;
    UploadBuffer mGodRayBuffer;


    UINT64 mFence = 0;

    bool mNeedsCloudUpdate = false;
    bool mNeedsCloudLightingUpdate = false;
};

}

#endif