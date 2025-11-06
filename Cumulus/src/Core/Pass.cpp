/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps compute pass functionality
----------------------------------------------*/

#include <Core/CommonTypes.h>
#include <Core/Pass.h>
#include <Core/RootSignatureBuilder.h>
#include <Core/ResourceCodex.h>
#include <Core/ShaderUtils.h>
#include <Core/Texture.h>

namespace Muon
{

Pass::~Pass()
{
}

bool Pass::Destroy()
{
    mpRootSignature.Reset();
    mpPipelineState.Reset();
    return true;
}

int32_t Pass::GetResourceRootIndex(const char* name) const
{
    auto itFind = mResourceNameToRootIndex.find(name);
    if (itFind == mResourceNameToRootIndex.end())
        return ROOTIDX_INVALID;

    return itFind->second;
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
        builder.AddUnorderedAccessView(uav.BindPoint, uav.Space, uav.Visibility);
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

bool Pass::BindMaterial(const Material& material, ID3D12GraphicsCommandList* pCommandList) const
{
    const DefaultBuffer& paramBuffer = material.GetParamBuffer();
    const std::unordered_map<std::string, TextureID>& textureParams = material.GetTextureParams();

    int32_t materialParamsRootIndex = GetResourceRootIndex("PSPerMaterial");
    if (materialParamsRootIndex != ROOTIDX_INVALID)
    {
        pCommandList->SetGraphicsRootConstantBufferView(materialParamsRootIndex, paramBuffer.GetGPUVirtualAddress());
    }

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    for (auto texPair : textureParams)
    {
        TextureID texId = texPair.second;
        const Texture* pTex = codex.GetTexture(texId);
        if (!pTex || !pTex->GetResource())
            continue;

        int32_t texRootParamIndex = GetResourceRootIndex(texPair.first.c_str());
        if (texRootParamIndex == ROOTIDX_INVALID)
            continue;

        pCommandList->SetGraphicsRootDescriptorTable(texRootParamIndex, pTex->GetSRVHandleGPU());
    }

    return true;
}

/////////////////////////////////////////////////////////

bool GraphicsPass::Bind(ID3D12GraphicsCommandList* pCommandList) const
{
    if (!mInitialized || !mpRootSignature || !mpPipelineState)
        return false;

    pCommandList->SetGraphicsRootSignature(mpRootSignature.Get());
    pCommandList->SetPipelineState(mpPipelineState.Get());
    return true;
}

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

bool GraphicsPass::GeneratePipelineState()
{
    ID3D12Device* pDevice = Muon::GetDevice();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mpRootSignature.Get();

    // Vertex Shader 
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(mpVS->ShaderBlob->GetBufferPointer(), mpVS->ShaderBlob->GetBufferSize());

    // Pixel Shader
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(mpPS->ShaderBlob->GetBufferPointer(), mpPS->ShaderBlob->GetBufferSize());

    // Input layout
    psoDesc.InputLayout.pInputElementDescs = mpVS->InputElements.data();
    psoDesc.InputLayout.NumElements = static_cast<UINT>(mpVS->InputElements.size());

    // Rasterizer state
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // Blend state
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth stencil state
    psoDesc.DepthStencilState.DepthEnable = FALSE; // TODO: Enable this when we want to do depth testing
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    // Render target formats
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = Muon::GetRTVFormat();
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;  // Enable all samples
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpPipelineState));
    return SUCCEEDED(hr);
}

/////////////////////////////////////////////////////////

bool ComputePass::Bind(ID3D12GraphicsCommandList* pCommandList) const
{
    if (!mInitialized || !mpRootSignature || !mpPipelineState)
        return false;

    pCommandList->SetComputeRootSignature(mpRootSignature.Get());
    pCommandList->SetPipelineState(mpPipelineState.Get());
    return true;
}

bool ComputePass::GatherShaderResources()
{
    if (!mpCS || !mpCS->Initialized)
        return false;

    mResources.clear();
    mConstantBuffers.clear();

    const ShaderReflectionData& data = mpCS->ReflectionData;
    mResources.assign(data.Resources.begin(), data.Resources.end());
    mConstantBuffers.assign(data.ConstantBuffers.begin(), data.ConstantBuffers.end());

    return true;
}

bool ComputePass::GeneratePipelineState()
{
    ID3D12Device* pDevice = Muon::GetDevice();

    D3D12_COMPUTE_PIPELINE_STATE_DESC compDesc = {};
    compDesc.pRootSignature = mpRootSignature.Get();
    compDesc.CS = CD3DX12_SHADER_BYTECODE(mpCS->ShaderBlob->GetBufferPointer(), mpCS->ShaderBlob->GetBufferSize());
    compDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    HRESULT hr = pDevice->CreateComputePipelineState(&compDesc, IID_PPV_ARGS(&mpPipelineState));
    return SUCCEEDED(hr);
}

}