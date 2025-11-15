/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wrapper for DX12 Descriptor Heap
----------------------------------------------*/

#include <Core/DescriptorHeap.h>

#include <assert.h>

namespace Muon
{

DescriptorHeap::DescriptorHeap(ID3D12Device* pDevice, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    Init(pDevice, numDescriptors);
}

DescriptorHeap::~DescriptorHeap()
{
}

void DescriptorHeap::Destroy()
{
    mHeap.Reset();
}

bool DescriptorHeap::Init(ID3D12Device* pDevice, UINT numDescriptors)
{
    mCapacity = numDescriptors;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;

    HRESULT hr = pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeap));
    if (FAILED(hr))
        return false;

    mDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mCPUStart = mHeap->GetCPUDescriptorHandleForHeapStart();
    mGPUStart = mHeap->GetGPUDescriptorHandleForHeapStart();

    mFreeIndices.reserve(numDescriptors);
    for (UINT n = numDescriptors; n > 0; n--)
        mFreeIndices.push_back(n - 1);

    return true;
}

bool DescriptorHeap::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCPU, D3D12_GPU_DESCRIPTOR_HANDLE& outGPU)
{
    if (mFreeIndices.empty())
        return false;

    UINT idx = mFreeIndices.back();
    mFreeIndices.pop_back();

    outCPU.ptr = mCPUStart.ptr + (idx * mDescriptorSize);
    outGPU.ptr = mGPUStart.ptr + (idx * mDescriptorSize);

    return true;
}

void DescriptorHeap::Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
{
    int cpu_idx = (int)((out_cpu_desc_handle.ptr - mCPUStart.ptr) / mDescriptorSize);
    int gpu_idx = (int)((out_gpu_desc_handle.ptr - mGPUStart.ptr) / mDescriptorSize);
    assert(cpu_idx == gpu_idx);
    mFreeIndices.push_back(cpu_idx);
}



}