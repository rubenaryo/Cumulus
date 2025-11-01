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

// Creates a vertex/index buffer using the default heap, but does NOT populate it with initial data.
bool CreateBuffer(void* bufferData, UINT bufferDataSize, ID3D12Resource*& out_buffer)
{
    if (!Muon::GetDevice())
        return false;

    // Create VBO in default heap type
    HRESULT hr = Muon::GetDevice()->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferDataSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&out_buffer)
    );
    COM_EXCEPT(hr);

    return out_buffer != nullptr;
}

// Intentially not tied to the destructor. That way meshes can be copied around freely without being randomly released.
// The ResourceCodex owns destroying meshes.
bool Mesh::Release()
{
    bool released = false;
    if (IndexBuffer)
    {
        IndexBuffer->Release();
        released = true;
    }

    if (VertexBuffer)
    {
        VertexBuffer->Release();
        released = true;
    }

    return released;
}

bool Mesh::Init(void* vertexData, UINT vertexDataSize, UINT vertexStride, void* indexData, UINT indexDataSize, UINT indexCount, DXGI_FORMAT indexFormat)
{
    vertexDataSize = Muon::AlignToBoundary(vertexDataSize, 16);

    if (!vertexData || !CreateBuffer(vertexData, vertexDataSize, this->VertexBuffer))
    {
        Muon::Print("Error: Initialized mesh without vertices.\n");
        return false;
    }

    if (!indexData || !CreateBuffer(indexData, indexDataSize, this->IndexBuffer))
    {
        Muon::Print("Info: Initialized mesh without indices.\n");
    }

    if (!PopulateBuffers(vertexData, vertexDataSize, vertexStride, indexData, indexDataSize, indexCount))
    {
        return false;
    }

    // Initialize vertex buffer view
    VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = vertexStride;
    VertexBufferView.SizeInBytes = vertexDataSize;

    // Create IBO in default heap type
    if (IndexBuffer)
    {
        IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
        IndexBufferView.Format = indexFormat;
        IndexBufferView.SizeInBytes = indexDataSize;
    }

    IndexCount = indexCount;

    return true;
}

bool Mesh::PopulateBuffers(void* vertexData, UINT vertexDataSize, UINT vertexStride, void* indexData, UINT indexDataSize, UINT indexCount)
{
    ResourceCodex& codex = ResourceCodex::GetSingleton();
    Muon::UploadBuffer& stagingBuffer = codex.GetMeshStagingBuffer();
    ID3D12GraphicsCommandList* pCommandList = Muon::GetCommandList();

    bool bDoIndexBuffer = indexData && indexDataSize > 0;
    UINT totalRequestedSize = bDoIndexBuffer ? vertexDataSize + indexDataSize : vertexDataSize;

    if (!pCommandList || !stagingBuffer.CanAllocate(totalRequestedSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
        return false;

    void* vboMappedPtr;
    void* iboMappedPtr;
    D3D12_GPU_VIRTUAL_ADDRESS vboGpuAddr, iboGpuAddr;
    UINT vboOffset, iboOffset;

    bool allocateSuccess = stagingBuffer.Allocate(
        vertexDataSize,
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT,
        vboMappedPtr,
        vboGpuAddr,
        vboOffset
    );

    if (!allocateSuccess)
    {
        // This should never happen since we check CanAllocate above...
        return false;
    }

    // Copy the vertex data into the upload buffer
    memcpy(vboMappedPtr, vertexData, vertexDataSize);

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = vertexData;
    subResourceData.RowPitch = vertexDataSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Schedule a copy from the staging buffer to the real vertex buffer
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        VertexBuffer,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    ));
    //pCommandList->CopyBufferRegion(VertexBuffer, 0, stagingBuffer.GetResource(), vboOffset, vertexDataSize);
    UpdateSubresources(pCommandList, VertexBuffer, stagingBuffer.GetResource(), 0, 0, 1, &subResourceData);

    if (bDoIndexBuffer)
    {
        allocateSuccess = stagingBuffer.Allocate(
            indexDataSize,
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT,
            iboMappedPtr,
            iboGpuAddr,
            iboOffset
        );

        if (!allocateSuccess)
        {
            return false;
        }

        memcpy(iboMappedPtr, indexData, indexDataSize);
        pCommandList->CopyBufferRegion(IndexBuffer, 0, stagingBuffer.GetResource(), iboOffset, indexDataSize);
    }

    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        VertexBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    ));

    if (bDoIndexBuffer)
    {
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            IndexBuffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        ));
    }

    return true;
}

bool Mesh::Draw(ID3D12GraphicsCommandList* pCommandList) const
{
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
    pCommandList->IASetIndexBuffer(&IndexBufferView);
    //pCommandList->DrawInstanced(3, 1, 0, 0);
    pCommandList->DrawIndexedInstanced(IndexCount, 1, 0, 0, 0);

    return true;
}

}