/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/10
Description : Mesh stores the vertex,index buffers ready to be drawn by DirectX
----------------------------------------------*/
#ifndef EASEL_MESH_H
#define EASEL_MESH_H

#include "DXCore.h"
#include "Shader.h"

namespace Muon
{
struct Mesh
{
    bool Init(const wchar_t* name, void* vertexData, UINT vertexDataSize, UINT vertexStride, void* indexData, UINT indexDataSize, UINT indexCount, DXGI_FORMAT indexFormat);
    bool Create(const wchar_t* name, UINT vtxDataSize, UINT vtxStride, UINT idxDataSize = 0, UINT idxCount = 0, DXGI_FORMAT idxFormat = DXGI_FORMAT_UNKNOWN);
    bool Release();
    bool PopulateBuffers(void* vertexData, UINT vertexDataSize, UINT vertexStride, void* indexData, UINT indexDataSize, UINT indexCount);
    bool Draw(ID3D12GraphicsCommandList* pCommandList) const;

    ID3D12Resource* VertexBuffer = nullptr;
    ID3D12Resource* IndexBuffer = nullptr;

    std::wstring mName;
    Microsoft::WRL::ComPtr<ID3D12Resource> mpVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> mpIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {0};
    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {0};
    UINT IndexCount = 0;
    UINT Stride = 0;
};

}

#endif