/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Generator for D3D12 Root Signatures
----------------------------------------------*/

#include <Core/RootSignatureBuilder.h>

namespace Muon
{

void RootSignatureBuilder::Reset()
{
    mParameters.clear();
    mStaticSamplers.clear();
    mDescriptorRanges.clear();
}

void RootSignatureBuilder::AddConstantBufferView(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.ShaderVisibility = visibility;
    param.Descriptor.ShaderRegister = shaderRegister;
    param.Descriptor.RegisterSpace = space;
    mParameters.push_back(param);
}

void RootSignatureBuilder::AddShaderResourceView(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = shaderRegister;
    srvRange.RegisterSpace = space;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    AddDescriptorTable(&srvRange, 1, visibility);
}

void RootSignatureBuilder::AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE* ranges, UINT numRanges, D3D12_SHADER_VISIBILITY visibility)
{
    mDescriptorRanges.emplace_back(ranges, ranges + numRanges);

    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.ShaderVisibility = visibility;
    param.DescriptorTable.NumDescriptorRanges = numRanges;
    param.DescriptorTable.pDescriptorRanges = mDescriptorRanges.back().data();
    mParameters.push_back(param);
}

void RootSignatureBuilder::AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    mStaticSamplers.push_back(sampler);
}

bool RootSignatureBuilder::Build(ID3D12Device* pDevice, ID3D12RootSignature** ppRootSig)
{
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = static_cast<UINT>(mParameters.size());
    rootSigDesc.pParameters = mParameters.data();
    rootSigDesc.NumStaticSamplers = static_cast<UINT>(mStaticSamplers.size());
    rootSigDesc.pStaticSamplers = mStaticSamplers.data();
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> pSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &pSignatureBlob,
        &pErrorBlob);

    if (FAILED(hr))
    {
        if (pErrorBlob)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
        return false;
    }

    hr = pDevice->CreateRootSignature(
        0,
        pSignatureBlob->GetBufferPointer(),
        pSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(ppRootSig));

    return SUCCEEDED(hr);
}

}