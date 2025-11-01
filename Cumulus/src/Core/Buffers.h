/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Interface for GPU  Buffers
----------------------------------------------*/
#ifndef MUON_BUFFERS_H
#define MUON_BUFFERS_H

#include <wrl/client.h>
#include <d3d12.h>
#include <stdint.h>
#include <string>

namespace Muon
{

struct Buffer
{
    Buffer() = default;
    virtual ~Buffer();

    void BaseCreate(const wchar_t* name, size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES resourceState);
    void BaseDestroy();

    size_t GetBufferSize() const { return mBufferSize; }
    ID3D12Resource* GetResource() { return mpResource.Get(); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
    {
        return mpResource->GetGPUVirtualAddress();
    }

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> mpResource;
    std::wstring mName;
    size_t mBufferSize = 0;
};

struct UploadBuffer : Buffer
{
    UploadBuffer();
    ~UploadBuffer();

    void Create(const wchar_t* name, size_t size);
    void Destroy();

    void* Map();
    void Unmap(size_t begin, size_t end);

    bool CanAllocate(UINT desiredSize, UINT alignment);
    bool Allocate(UINT desiredSize, UINT alignment, void*& out_mappedPtr, D3D12_GPU_VIRTUAL_ADDRESS& out_gpuAddr, UINT& out_offset);

private:
    UINT8* mMappedPtr = nullptr;
    size_t mOffset = 0; // The current offset into the buffer where allocations take place
};

struct DefaultBuffer : Buffer
{
    DefaultBuffer() = default;
    ~DefaultBuffer();

    void Create(const wchar_t* name, size_t size);
    bool Populate(void* data, size_t dataSize, UploadBuffer& stagingBuffer, ID3D12GraphicsCommandList* pCommandList);
    void Destroy();
};

size_t GetConstantBufferSize(size_t desiredSize);

}
#endif