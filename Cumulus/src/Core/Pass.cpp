/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps compute pass functionality
----------------------------------------------*/

#include <Core/Pass.h>
#include <Core/RootSignatureBuilder.h>

namespace Muon
{

ComputePass::~ComputePass()
{

}

bool ComputePass::GeneratePipelineState()
{
    return true;
}

bool ComputePass::GenerateRootSignature()
{
    ID3D12Device* pDevice = Muon::GetDevice();

    RootSignatureBuilder builder;
    mResourceNameToRootIndex.clear();

    // Organize resources by type and shader stage
    std::vector<ShaderResourceBinding> CBVs;
    std::vector<ShaderResourceBinding> SRVs;
    std::vector<ShaderResourceBinding> UAVs;
    std::vector<ShaderResourceBinding> Samplers;

    // Categorize resources
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
        builder.AddConstantBufferView(cb.BindPoint, cb.Space, D3D12_SHADER_VISIBILITY_ALL);
        mResourceNameToRootIndex[cb.Name] = rootParamIndex++;
    }

    for (const auto& srv : SRVs)
    {
        builder.AddShaderResourceView(srv.BindPoint, srv.Space, D3D12_SHADER_VISIBILITY_ALL);
        mResourceNameToRootIndex[srv.Name] = rootParamIndex++;
    }

    for (const auto& uav : UAVs)
    {
        builder.AddShaderResourceView(uav.BindPoint, uav.Space, D3D12_SHADER_VISIBILITY_ALL);
        mResourceNameToRootIndex[uav.Name] = rootParamIndex++;
    }

    return builder.Build(pDevice, mpRootSignature.GetAddressOf());
}

bool ComputePass::Generate()
{
    if (!GenerateRootSignature())
        return false;

    if (!GeneratePipelineState())
        return false;

    mInitialized = true;
    return true;
}

}