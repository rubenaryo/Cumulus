/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Implementation of GPU Buffers
----------------------------------------------*/

#include <d3dx12.h>
#include <Core/DXCore.h>
#include <Core/Buffers.h>
#include <Core/ThrowMacros.h>
#include <Utils/Utils.h>

namespace Muon
{

Buffer::~Buffer()
{
}

void Buffer::BaseCreate(const wchar_t* name, size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES resourceState)
{
	mBufferSize = size;

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = heapType;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC ResourceDesc = {};
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = mBufferSize;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = GetDevice()->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc,
		resourceState, nullptr, IID_PPV_ARGS(mpResource.GetAddressOf()));
	COM_EXCEPT(hr);

	mName = name;
	mpResource->SetName(mName.c_str());
}

void Buffer::BaseDestroy()
{
	mpResource.Reset();
	mBufferSize = 0;
	mName = std::wstring();
}

////////////////////////////////////////////////////////////////

UploadBuffer::UploadBuffer()
{
}

UploadBuffer::~UploadBuffer()
{
}

void UploadBuffer::Create(const wchar_t* name, size_t size)
{
	Destroy();
	BaseCreate(name, size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void UploadBuffer::Destroy()
{
	if (mMappedPtr)
		Unmap(0, mBufferSize);

	mOffset = 0;

	BaseDestroy();
}

void* UploadBuffer::Map()
{
	if (mMappedPtr)
	{
		Print("Warning: Tried to Map() an already mapped upload buffer.");
		return nullptr;
	}

	mpResource->Map(0, &CD3DX12_RANGE(0, mBufferSize), reinterpret_cast<void**>(&mMappedPtr));
	return mMappedPtr;
}

void UploadBuffer::Unmap(size_t begin, size_t end)
{
	if (mMappedPtr == nullptr)
	{
		Print("Warning: Tried to Unmap() an unmapped upload buffer.");
		return;
	}

	mpResource->Unmap(0, &CD3DX12_RANGE(begin, std::min<size_t>(end, mBufferSize)));
	mMappedPtr = nullptr;
}

bool UploadBuffer::CanAllocate(UINT desiredSize, UINT alignment)
{
	if (mMappedPtr == nullptr || mpResource == nullptr)
	{
		// If the upload buffer doesn't exist or is uninitialized, we obviously can't allocate anything.
		return false;
	}

	// Whether (after alignment) the requested size will be able to fit in the buffer
	UINT aligned = Muon::AlignToBoundary(mOffset, alignment);
	return aligned + desiredSize <= mBufferSize;
}

// All this really does is check if the desiredSize fits and then returns a pointer to where data should be inserted.
bool UploadBuffer::Allocate(UINT desiredSize, UINT alignment, void*& out_mappedPtr, D3D12_GPU_VIRTUAL_ADDRESS& out_gpuAddr, UINT& out_offset)
{
	if (mMappedPtr == nullptr || mpResource == nullptr)
	{
		Muon::Printf("Error: Attempted Allocate() on an uncreated or unmapped UploadBuffer.");
		return false;
	}

	// Puts the current offset on the next requested alignment boundary
	UINT aligned = Muon::AlignToBoundary(mOffset, alignment);

	if (aligned + desiredSize > mBufferSize)
	{
		// We don't have enough space for this allocation
		// TODO: Maybe do a re-Create() operation? Will be expensive.
		Muon::Printf("Error: Upload Buffer failed to allocate %u bytes. Only have %u / %u space remaining.\n", desiredSize, (mBufferSize - aligned), mBufferSize);
		return false;
	}

	// Return the addresses beginning at the unoccupied aligned offset
	out_mappedPtr = mMappedPtr + aligned;
	out_gpuAddr = mpResource->GetGPUVirtualAddress() + aligned;
	out_offset = aligned;

	// Adjust write head
	mOffset = aligned + desiredSize;

	return true;
}


////////////////////////////////////////////////////////////////

DefaultBuffer::~DefaultBuffer()
{
}

void DefaultBuffer::Create(const wchar_t* name, size_t size)
{
	Destroy();
	BaseCreate(name, size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
}

bool DefaultBuffer::Populate(void* data, size_t dataSize, UploadBuffer& stagingBuffer, ID3D12GraphicsCommandList* pCommandList)
{
	if (!pCommandList || !data || dataSize > mBufferSize)
		return false;

	void* mapped = stagingBuffer.Map();
	memcpy(mapped, data, mBufferSize);
	stagingBuffer.Unmap(0, 0);

	pCommandList->CopyBufferRegion(mpResource.Get(), 0, stagingBuffer.GetResource(), 0, mBufferSize);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		mpResource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);
	pCommandList->ResourceBarrier(1, &barrier);

	return true;
}

void DefaultBuffer::Destroy()
{
	BaseDestroy();
}

size_t GetConstantBufferSize(size_t desiredSize) 
{
	// TODO: AlignToBoundary(desiredSize, 256)?
	return (desiredSize + 255) & ~255;
}

}