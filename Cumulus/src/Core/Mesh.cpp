/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/4
Description : Mesh initialization logic, for DX12
----------------------------------------------*/
#include "Mesh.h"

#include <Core/DXCore.h>
#include <Core/ThrowMacros.h>
#include <Core/ResourceCodex.h>
#include <Utils/Utils.h>
#include <d3dx12.h>

namespace Muon
{

bool Mesh::Create(const wchar_t* name, UINT vtxDataSize, UINT vtxStride, UINT vtxCount, UINT idxDataSize, UINT idxCount, DXGI_FORMAT idxFormat, AABB aabb)
{
    if (!Muon::GetDevice() || vtxDataSize == 0)
        return false;

    mName = name;

    {
        this->aabb = aabb;

        // Creates as with default heap type. This is fast to read from the GPU but inaccessible from the CPU. 
        // We need to go through a staging buffer (UploadBuffer) to get the data to it.
        HRESULT hr = Muon::GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vtxDataSize),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&mpVertexBuffer)
        );
        COM_EXCEPT(hr);
        if (FAILED(hr) || mpVertexBuffer.Get() == nullptr)
        {
            Muon::Printf(L"Error: Failed to create vertex buffer for mesh: %s\n", mName.c_str());
            return false;
        }

        std::wstring vtxName = mName + L"_VertexBuffer";
        mpVertexBuffer->SetName(vtxName.c_str());

        // Initialize vertex buffer view
        VertexBufferView.BufferLocation = mpVertexBuffer->GetGPUVirtualAddress();
        VertexBufferView.StrideInBytes = vtxStride;
        VertexBufferView.SizeInBytes = vtxDataSize;

        VertexCount = vtxCount;
    }

    if (idxDataSize > 0)
    {
        HRESULT hr = Muon::GetDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(idxDataSize),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&mpIndexBuffer)
        );
        COM_EXCEPT(hr);
        if (FAILED(hr) || mpIndexBuffer.Get() == nullptr)
        {
            Muon::Printf(L"Error: Failed to create index buffer for mesh: %s\n", mName.c_str());
            return false;
        }

        std::wstring idxName = mName + L"_IndexBuffer";
        mpIndexBuffer->SetName(idxName.c_str());

        IndexBufferView.BufferLocation = mpIndexBuffer->GetGPUVirtualAddress();
        IndexBufferView.Format = idxFormat;
        IndexBufferView.SizeInBytes = idxDataSize;

        IndexCount = idxCount;
    }

    return true;
}

// Intentially not tied to the destructor. That way meshes can be copied around freely without being randomly released.
// The ResourceCodex owns destroying meshes.
bool Mesh::Destroy()
{
    mpVertexBuffer.Reset();
    mpIndexBuffer.Reset();

    return true;
}

bool Mesh::Draw(ID3D12GraphicsCommandList* pCommandList) const
{
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
    pCommandList->DrawInstanced(VertexCount, 1, 0, 0);
    return true;
}

bool Mesh::DrawIndexed(ID3D12GraphicsCommandList* pCommandList) const
{
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
    pCommandList->IASetIndexBuffer(&IndexBufferView);
    pCommandList->DrawIndexedInstanced(IndexCount, 1, 0, 0, 0);
    return true;
}

}