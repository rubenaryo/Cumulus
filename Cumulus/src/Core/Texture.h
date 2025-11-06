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
    bool InitUAV(ID3D12Device* pDevice, DescriptorHeap* pSRVHeap);

    bool ValidRTV() const { return mViewRTV.HandleCPU.ptr != 0; }
    bool ValidSRV() const { return mViewSRV.HandleCPU.ptr != 0 && mViewSRV.HandleGPU.ptr != 0; }
    bool ValidUAV() const { return mViewUAV.HandleCPU.ptr != 0 && mViewUAV.HandleGPU.ptr != 0; }

    void SetRTVHandleCPU(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle) { mViewRTV.HandleCPU = cpuHandle; }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTVHandleCPU() const { return mViewRTV.HandleCPU; }
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetSRVHandleCPU() const { return mViewSRV.HandleCPU; }
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetUAVHandleCPU() const { return mViewUAV.HandleCPU; }

    CD3DX12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return mViewSRV.HandleGPU; }
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetUAVHandleGPU() const { return mViewUAV.HandleGPU; }

    UINT GetWidth() const { return mWidth; }
    UINT GetHeight() const { return mHeight; }
    UINT GetDepth() const { return mDepth; }

    DXGI_FORMAT GetFormat() const { return mFormat; }

    ID3D12Resource* GetResource() { return mpResource.Get(); }
    const ID3D12Resource* GetResource() const { return mpResource.Get(); }

    bool Destroy();

private:
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