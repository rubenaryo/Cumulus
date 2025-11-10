/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Generator for D3D12 Root Signatures
----------------------------------------------*/

#include <Core/RootSignatureBuilder.h>
#include <array>

namespace Muon
{

static const size_t NUM_STATIC_SAMPLERS = 6;
std::array<const CD3DX12_STATIC_SAMPLER_DESC, NUM_STATIC_SAMPLERS> InitStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

void RootSignatureBuilder::Reset()
{
    mParameters.clear();
    mStaticSamplers.clear();
    mDescriptorRanges.clear();
}

void RootSignatureBuilder::AddConstantBufferView(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_ROOT_PARAMETER cbv = {};
    cbv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    cbv.ShaderVisibility = visibility;
    cbv.Descriptor.ShaderRegister = shaderRegister;
    cbv.Descriptor.RegisterSpace = space;
    mParameters.push_back(cbv);
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

void RootSignatureBuilder::AddUnorderedAccessView(UINT shaderRegister, UINT space, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_DESCRIPTOR_RANGE uavRange = {};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 1;
    uavRange.BaseShaderRegister = shaderRegister;
    uavRange.RegisterSpace = space;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    AddDescriptorTable(&uavRange, 1, visibility);
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

/// We define a fixed set of 6 sampler types as static. They will always be available should the shader ask for them.
/// If shaderRegister >= 6, all following samplers will be copies of the last one.
void RootSignatureBuilder::AddStaticSampler(UINT shaderRegister, UINT registerSpace)
{
    static std::array<const CD3DX12_STATIC_SAMPLER_DESC, NUM_STATIC_SAMPLERS> sStaticSamplerTemplates = InitStaticSamplers();

    UINT index = min(shaderRegister, NUM_STATIC_SAMPLERS - 1);
    CD3DX12_STATIC_SAMPLER_DESC desc = sStaticSamplerTemplates.at(index);
    desc.ShaderRegister = shaderRegister;
    desc.RegisterSpace = registerSpace;
    desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // Assumed visible to all shaders.

    mStaticSamplers.push_back(desc);
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