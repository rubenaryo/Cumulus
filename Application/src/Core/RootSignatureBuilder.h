/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Generator for D3D12 Root Signatures
----------------------------------------------*/
#ifndef MUON_ROOTSIGNATUREBUILDER_H
#define MUON_ROOTSIGNATUREBUILDER_H

#include <Core/DXCore.h>

namespace Muon
{

class RootSignatureBuilder
{
public:
    void Reset();
    void AddConstantBufferView(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility);
    void AddShaderResourceView(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility);
    void AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, UINT numRanges, D3D12_SHADER_VISIBILITY visibility);
    void AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);

    bool Build(ID3D12Device* pDevice, ID3D12RootSignature** ppRootSig);

private:
    std::vector<D3D12_ROOT_PARAMETER> mParameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> mStaticSamplers;
    std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> mDescriptorRanges;
};

}

#endif