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

    bool UpdateWorldMatrix(cbPerEntity& data);
    bool UpdateCamera(cbCamera& data);
    bool UpdateLights(cbLights& data);
    bool UpdateTime(cbTime& data);
    bool UpdateAABB(cbIntersections& data);
    bool UpdateAtmosphere(cbAtmosphere& data);
    bool UpdateCloudData(cbCloudGenData& data);
    bool UpdateCloudLighting(cbCloudLighting& data);
    bool UpdateHulls(cbHulls& data);
    bool UpdateHullFaces(cbHullFaces& data);
    bool UpdateJetTrail(cbJetTrailPositions& jetTrailPositions);

    void Destroy();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdAllocator;

    UploadBuffer mWorldMatrixBuffer;
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

    UINT64 mFence = 0;

    bool mNeedsCloudUpdate = false;
    bool mNeedsCloudLightingUpdate = false;
};

}

#endif