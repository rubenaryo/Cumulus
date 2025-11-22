/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Resources needed for a single frame
----------------------------------------------*/
#ifndef MUON_FRAMERESOURCES_H
#define MUON_FRAMERESOURCES_H

#include <Core/DXCore.h>
#include <Core/Buffers.h>

namespace Muon
{

struct FrameResources
{
    FrameResources();
    
    bool Create(UINT width, UINT height);
    void Update(float totalTime, float deltaTime);
    void Destroy();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdAllocator;

    UploadBuffer mWorldMatrixBuffer;
    UploadBuffer mLightBuffer;
    UploadBuffer mTimeBuffer;
    UploadBuffer mAABBBuffer;
    UploadBuffer mAtmosphereBuffer;

    UINT64 mFence = 0;
};

}

#endif