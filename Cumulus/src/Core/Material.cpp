/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/4
Description : Implementation of custom material system
----------------------------------------------*/
#include "Material.h"

#include <Core/PipelineState.h>
#include <Core/ResourceCodex.h>
#include <Core/RootSignatureBuilder.h>
#include <Core/Shader.h>
#include <Core/ShaderUtils.h>

namespace Muon
{

//////////

MaterialType::MaterialType(const wchar_t* name)
	:	mName(name)
{
}

void MaterialType::Destroy()
{
    mpRootSignature.Reset();
    mpPipelineState.Reset();
    mMaterialParamsBuffer.Destroy();
}

bool MaterialType::Bind(ID3D12GraphicsCommandList* pCommandList) const
{
    if (!mpRootSignature || !mpPipelineState)
        return false;

    pCommandList->SetGraphicsRootSignature(mpRootSignature.Get());
    pCommandList->SetPipelineState(mpPipelineState.Get());

    int32_t materialParamsRootIndex = GetResourceRootIndex("PSPerMaterial");
    if (materialParamsRootIndex != ROOTIDX_INVALID)
    {
        pCommandList->SetGraphicsRootConstantBufferView(materialParamsRootIndex, mMaterialParamsBuffer.GetGPUVirtualAddress());
    }

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    for (auto texPair : mTextureParams)
    {
        TextureID texId = texPair.second;
        const Texture* pTex = codex.GetTexture(texId);
        if (!pTex || !pTex->pResource)
            continue;

        int32_t texRootParamIndex = GetResourceRootIndex(texPair.first.c_str());
        if (texRootParamIndex == ROOTIDX_INVALID)
            continue;

        pCommandList->SetDescriptorHeaps(1, codex.GetSRVDescriptorHeap().GetHeapAddr());
        pCommandList->SetGraphicsRootDescriptorTable(texRootParamIndex, pTex->GPUHandle);
    }

    return true;
}

void MaterialType::SetVertexShader(const VertexShader* vs)
{
	mpVS = vs;
}

void MaterialType::SetPixelShader(const PixelShader* ps)
{
	mpPS = ps;
}

const ParameterDesc* MaterialType::GetParameter(const char* paramName) const
{
	auto itFind = mParamNameToIndex.find(paramName);
	if (itFind == mParamNameToIndex.end() || itFind->second >= mParamNameToIndex.size())
		return nullptr;

	size_t index = itFind->second;
	return &mParameters.at(index);
}

bool MaterialType::PopulateMaterialParams(UploadBuffer& stagingBuffer, ID3D12GraphicsCommandList* pCommandList)
{
    return mMaterialParamsBuffer.Populate(&mMaterialParams, sizeof(cbMaterialParams), stagingBuffer, pCommandList);
}

bool MaterialType::SetTextureParam(const char* paramName, TextureID texId)
{
    // Validate that the param actually exists.
    if (GetResourceRootIndex(paramName) == ROOTIDX_INVALID)
        return false;

    mTextureParams[paramName] = texId;
    return true;
}

int32_t MaterialType::GetResourceRootIndex(const char* name) const
{
    auto itFind = mResourceNameToRootIndex.find(name);
    if (itFind == mResourceNameToRootIndex.end())
        return ROOTIDX_INVALID;

    return itFind->second;
}

bool MaterialType::Generate(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
{
    ID3D12Device* pDevice = Muon::GetDevice();

    if (!mpVS || !mpPS || !mpVS->Initialized || !mpPS->Initialized)
        return false;

    if (!mpVS->ReflectionData.IsReflected || !mpPS->ReflectionData.IsReflected)
        return false;

    if (!MergeShaderResources())
        return false;

    if (!GenerateRootSignature())
        return false;

    if (!GeneratePipelineState(rtvFormat, dsvFormat))
        return false;

    std::wstring bufferName = mName + L"_ParamsBuffer";
    mMaterialParamsBuffer.Create(bufferName.c_str(), sizeof(cbMaterialParams));

    mInitialized = true;
    return true;
}

bool MaterialType::MergeShaderResources()
{
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

bool MaterialType::GenerateRootSignature()
{
    ID3D12Device* pDevice = Muon::GetDevice();

    RootSignatureBuilder builder;
    mResourceNameToRootIndex.clear();

    // Organize resources by type and shader stage
    std::vector<ShaderResourceBinding> VSCBs;
    std::vector<ShaderResourceBinding> PSCBs;
    std::vector<ShaderResourceBinding> VSSRVs;
    std::vector<ShaderResourceBinding> PSSRVs;
    std::vector<ShaderResourceBinding> Samplers;

    // Categorize resources
    for (size_t i = 0; i < mResources.size(); ++i)
    {
        const auto& res = mResources[i];
        bool isVS = (i < mpVS->ReflectionData.Resources.size());

        switch (res.Type)
        {
        case ShaderResourceType::ConstantBuffer:
            if (isVS) VSCBs.push_back(res);
            else PSCBs.push_back(res);
            break;
        case ShaderResourceType::Texture:
        case ShaderResourceType::StructuredBuffer:
            if (isVS) VSSRVs.push_back(res);
            else PSSRVs.push_back(res);
            break;
        case ShaderResourceType::Sampler:
            Samplers.push_back(res);
            break;
        }
    }

    UINT rootParamIndex = 0;

    // Add VS constant buffers
    for (const auto& cb : VSCBs)
    {
        builder.AddConstantBufferView(cb.BindPoint, cb.Space, D3D12_SHADER_VISIBILITY_VERTEX);
        mResourceNameToRootIndex[cb.Name] = rootParamIndex++;
    }

    // Add PS constant buffers
    for (const auto& cb : PSCBs)
    {
        builder.AddConstantBufferView(cb.BindPoint, cb.Space, D3D12_SHADER_VISIBILITY_PIXEL);
        mResourceNameToRootIndex[cb.Name] = rootParamIndex++;
    }

    // Add PS textures as unique SRVs
    for (const auto& srv : PSSRVs)
    {
        builder.AddShaderResourceView(srv.BindPoint, srv.Space, D3D12_SHADER_VISIBILITY_PIXEL);
        mResourceNameToRootIndex[srv.Name] = rootParamIndex++;
    }

    // Add static samplers
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
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        builder.AddStaticSampler(samplerDesc);
    }

    return builder.Build(pDevice, mpRootSignature.GetAddressOf());
}

bool MaterialType::GeneratePipelineState(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
{
    ID3D12Device* pDevice = Muon::GetDevice();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mpRootSignature.Get();

    // Vertex Shader 
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(mpVS->ShaderBlob.Get());
    
    // Pixel Shader
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(mpPS->ShaderBlob.Get());

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
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;  // Enable all samples
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT hr = pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mpPipelineState));
    return SUCCEEDED(hr);
}

////////

MaterialInstance::MaterialInstance(const char* name, const MaterialType& materialType)
	:	mName(name)
	,	mType(materialType)
{
}

bool MaterialInstance::SetParamValue(const char* paramName, ParameterValue value)
{
	const ParameterDesc* pParamDesc = mType.GetParameter(paramName);
	if (!pParamDesc)
		return false;

	UINT index = pParamDesc->Index;
	mParamValues.reserve(index + 1);
	mParamValues[index] = value;
	return true;
}

}
