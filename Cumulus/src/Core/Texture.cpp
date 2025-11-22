/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps texture resource management
----------------------------------------------*/

#include <Core/DescriptorHeap.h>
#include <Core/Texture.h>

namespace Muon
{

bool Texture::Create(const wchar_t* name, ID3D12Device* pDevice, UINT width, UINT height, UINT depth,
    DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState,
    D3D12_CLEAR_VALUE* pClearValue)
{
    mName = name;
    mWidth = width;
    mHeight = height;
    mDepth = depth;
    mFormat = format;

    bool is3D = depth > 1;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = is3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = depth;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;

    HRESULT hr = pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        pClearValue,
        IID_PPV_ARGS(&mpResource));

    if (FAILED(hr))
        return false;

    mpResource->SetName(mName.c_str());

    return true;
}

bool Texture::InitSRV(ID3D12Device* pDevice, DescriptorHeap* pSRVHeap)
{
    // Allocate descriptor
    if (!pSRVHeap || !pSRVHeap->Allocate(mViewSRV.HandleCPU, mViewSRV.HandleGPU))
        return false;

    D3D12_RESOURCE_DESC resourceDesc = mpResource->GetDesc();

    bool is3D = mDepth > 1;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mFormat;
    srvDesc.ViewDimension = is3D ? D3D12_SRV_DIMENSION_TEXTURE3D : D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    pDevice->CreateShaderResourceView(mpResource.Get(), &srvDesc, mViewSRV.HandleCPU);
    return true;
}

bool Texture::InitUAV(ID3D12Device* pDevice, DescriptorHeap* pSRVHeap)
{
    // Allocate descriptor
    if (!pSRVHeap || !pSRVHeap->Allocate(mViewUAV.HandleCPU, mViewUAV.HandleGPU))
        return false;

    bool is3D = mDepth > 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = mFormat;
    
    if (is3D)
    {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        uavDesc.Texture3D.MipSlice = 0;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.WSize = -1;
    }
    else
    {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
    }

    pDevice->CreateUnorderedAccessView(mpResource.Get(), nullptr, &uavDesc, mViewUAV.HandleCPU);
    return true;
}

bool Texture::Destroy()
{
    mpResource.Reset();
    mName = std::wstring();
    return true;
}

}