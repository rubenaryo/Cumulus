/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wrapper for DX12 Descriptor Heap 
----------------------------------------------*/
#ifndef MUON_DESCRIPTORHEAP_H
#define MUON_DESCRIPTORHEAP_H

#include <Core/DXCore.h>

namespace Muon
{

class DescriptorHeap
{
public:
    DescriptorHeap() = default;
    ~DescriptorHeap();

    void Destroy();

    bool Init(ID3D12Device* pDevice, UINT numDescriptors);

    bool Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCPU, D3D12_GPU_DESCRIPTOR_HANDLE& outGPU);

    // underlying heap
    ID3D12DescriptorHeap* GetHeap() const { return mHeap.Get(); }
    ID3D12DescriptorHeap*const* GetHeapAddr() const { return mHeap.GetAddressOf(); }

    UINT GetDescriptorSize() const { return mDescriptorSize; }
    UINT GetNumAllocated() const { return mCurrentOffset; }
    UINT GetCapacity() const { return mCapacity; }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE mCPUStart = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE mGPUStart = { 0 };
    UINT mDescriptorSize = 0;
    UINT mCurrentOffset = 0;
    UINT mCapacity = 0;
};

}
#endif