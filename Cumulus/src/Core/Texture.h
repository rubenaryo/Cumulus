/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps texture resource management
----------------------------------------------*/
#ifndef MUON_TEXTURE_H
#define MUON_TEXTURE_H

#include <Core/DXCore.h>

namespace Muon
{
class DescriptorHeap;
}

namespace Muon
{

struct TextureView
{
    TextureView()
    {
        HandleCPU.ptr = 0;
        HandleGPU.ptr = 0;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE HandleCPU;
    CD3DX12_GPU_DESCRIPTOR_HANDLE HandleGPU;
};

class Texture
{
public:
    bool Create(const wchar_t* name, ID3D12Device* pDevice, UINT width, UINT height, UINT depth,
        DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState,
        D3D12_CLEAR_VALUE* pClearValue = nullptr);

    bool InitSRV(ID3D12Device* pDevice, DescriptorHeap* pSRVHeap);

    bool Destroy();

    TextureView mViewRTV;
    TextureView mViewSRV;
    TextureView mViewUAV;

    UINT mWidth = 0;
    UINT mHeight = 0;
    UINT mDepth = 0;
    DXGI_FORMAT mFormat = DXGI_FORMAT_UNKNOWN;

    Microsoft::WRL::ComPtr<ID3D12Resource> mpResource;
    std::wstring mName;
};

}
#endif