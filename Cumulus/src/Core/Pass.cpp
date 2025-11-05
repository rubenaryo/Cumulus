/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps compute pass functionality
----------------------------------------------*/

#include <Core/Pass.h>
#include <Core/RootSignatureBuilder.h>
#include <Core/ShaderUtils.h>

namespace Muon
{

Pass::~Pass()
{
    mpRootSignature.Reset();
    mpPipelineState.Reset();
}

bool Pass::GeneratePipelineState()
{
    return true;
}

bool Pass::GenerateRootSignature()
{
    ID3D12Device* pDevice = Muon::GetDevice();

    RootSignatureBuilder builder;
    mResourceNameToRootIndex.clear();

    std::vector<ShaderResourceBinding> CBVs;
    std::vector<ShaderResourceBinding> SRVs;
    std::vector<ShaderResourceBinding> UAVs;
    std::vector<ShaderResourceBinding> Samplers;

    for (size_t i = 0; i < mResources.size(); ++i)
    {
        const ShaderResourceBinding& res = mResources[i];

        switch (res.Type)
        {
        case ShaderResourceType::ConstantBuffer:
            CBVs.push_back(res);
            break;
        case ShaderResourceType::Texture:
        case ShaderResourceType::StructuredBuffer:
            SRVs.push_back(res);
            break;
        case ShaderResourceType::RWTexture:
        case ShaderResourceType::RWStructuredBuffer:
            UAVs.push_back(res);
            break;
        case ShaderResourceType::Sampler:
            Samplers.push_back(res);
            break;
        }
    }

    UINT rootParamIndex = 0;

    for (const auto& cb : CBVs)
    {
        builder.AddConstantBufferView(cb.BindPoint, cb.Space, cb.Visibility);
        mResourceNameToRootIndex[cb.Name] = rootParamIndex++;
    }

    for (const auto& srv : SRVs)
    {
        builder.AddShaderResourceView(srv.BindPoint, srv.Space, srv.Visibility);
        mResourceNameToRootIndex[srv.Name] = rootParamIndex++;
    }

    for (const auto& uav : UAVs)
    {
        builder.AddShaderResourceView(uav.BindPoint, uav.Space, uav.Visibility);
        mResourceNameToRootIndex[uav.Name] = rootParamIndex++;
    }

    for (const auto& sampler : Samplers)
    {
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.ShaderRegister = sampler.BindPoint;
        samplerDesc.RegisterSpace = sampler.Space;
        samplerDesc.ShaderVisibility = sampler.Visibility;

        builder.AddStaticSampler(samplerDesc);
    }

    return builder.Build(pDevice, mpRootSignature.GetAddressOf());
}

bool Pass::Generate()
{
    if (!GatherShaderResources())
        return false;

    if (!GenerateRootSignature())
        return false;

    if (!GeneratePipelineState())
        return false;

    mInitialized = true;
    return true;
}

/////////////////////////////////////////////////////////

bool GraphicsPass::GatherShaderResources()
{
    if (!mpVS || !mpPS || !mpVS->Initialized || !mpPS->Initialized)
        return false;

    // combine VS and PS reflection data
    if (!MergeReflectionData(
        mpVS->ReflectionData,
        mpPS->ReflectionData,
        mResources,
        mConstantBuffers))
    {
        return false;
    }

    mParameters.clear();
    mParamNameToIndex.clear();

    UINT paramIndex = 0;
    for (const auto& cb : mConstantBuffers)
    {
        for (const auto& var : cb.Variables)
        {
            ParameterDesc param = var;
            param.Index = paramIndex;
            mParameters.push_back(param);
            mParamNameToIndex[param.Name] = paramIndex;
            paramIndex++;
        }
    }

    return true;
}

}