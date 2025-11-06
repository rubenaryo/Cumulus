/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps texture resource management
----------------------------------------------*/

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

bool Texture::Destroy()
{
    mpResource.Reset();
    mName = std::wstring();
    return true;
}

}