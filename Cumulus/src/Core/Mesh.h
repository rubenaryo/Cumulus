/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/10
Description : Mesh stores the vertex,index buffers ready to be drawn by DirectX
----------------------------------------------*/
#ifndef EASEL_MESH_H
#define EASEL_MESH_H

#include "DXCore.h"
#include "Shader.h"
#include <DirectXMath.h>
#include <Core/CommonTypes.h>
#include <Core/Hull.h>

namespace Muon
{
struct Mesh
{
    bool Create(const wchar_t* name, UINT vtxDataSize, UINT vtxStride, UINT vtxCount, UINT idxDataSize = 0, UINT idxCount = 0, DXGI_FORMAT idxFormat = DXGI_FORMAT_UNKNOWN, AABB aabb = {}, Hull hull = Hull());
    bool Destroy();
    bool Draw(ID3D12GraphicsCommandList* pCommandList) const;
    bool DrawIndexed(ID3D12GraphicsCommandList* pCommandList) const;

    ID3D12Resource* GetVertexBuffer() { return mpVertexBuffer.Get(); }
    ID3D12Resource* GetIndexBuffer() { return mpIndexBuffer.Get(); }
    const wchar_t* GetName() const { return mName.c_str(); }
    AABB GetAABB() const { return aabb; }
    Hull GetHull() const { return hull; }
protected:
    std::wstring mName;
    Microsoft::WRL::ComPtr<ID3D12Resource> mpVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> mpIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {0};
    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {0};
    UINT VertexCount = 0;
    UINT IndexCount = 0;
    UINT Stride = 0;
    AABB aabb;
    Hull hull;
};

}

#endif