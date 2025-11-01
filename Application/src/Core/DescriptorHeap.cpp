/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wrapper for DX12 Descriptor Heap
----------------------------------------------*/

#include <Core/DescriptorHeap.h>

namespace Muon
{

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
    mCurrentOffset = 0;

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

    return true;
}

bool DescriptorHeap::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCPU, D3D12_GPU_DESCRIPTOR_HANDLE& outGPU)
{
    if (mCurrentOffset >= mCapacity)
        return false;

    outCPU.ptr = mCPUStart.ptr + (mCurrentOffset * mDescriptorSize);
    outGPU.ptr = mGPUStart.ptr + (mCurrentOffset * mDescriptorSize);

    mCurrentOffset++;
    return true;
}


}