/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Wraps compute pass functionality
----------------------------------------------*/
#ifndef MUON_COMPUTEPASS_H
#define MUON_COMPUTEPASS_H

#include <Core/DXCore.h>
#include <Core/Shader.h>

#include <unordered_map>

namespace Muon
{
class Material;
}

namespace Muon
{

class Pass
{
public:
    Pass(const wchar_t* name) : mName(name) {}
    virtual ~Pass();

    bool Generate();
    bool Destroy();

    virtual bool Bind(ID3D12GraphicsCommandList* pCommandList) const = 0;
    bool BindMaterial(const Material& material, ID3D12GraphicsCommandList* pCommandList) const;
    const wchar_t* GetName() const { return mName.c_str(); }

    int GetResourceRootIndex(const char* name) const;
    const ParameterDesc* GetParameter(const char* paramName) const;

protected:
    virtual bool GatherShaderResources() = 0;
    virtual bool GeneratePipelineState() = 0;
    bool GenerateRootSignature();

    std::vector<ShaderResourceBinding> mResources;
    std::vector<ConstantBufferReflection> mConstantBuffers;
    std::vector<ParameterDesc> mParameters;

    std::unordered_map<std::string, size_t> mParamNameToIndex;
    std::unordered_map<std::string, int32_t> mResourceNameToRootIndex;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mpRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mpPipelineState;

    std::wstring mName;

    bool mInitialized = false;
};

class GraphicsPass : public Pass
{
public:
    GraphicsPass(const wchar_t* name) : Pass(name) {}

    virtual bool Bind(ID3D12GraphicsCommandList* pCommandList) const override;

    void SetVertexShader(const VertexShader* pVS) { mpVS = pVS; }
    void SetPixelShader(const PixelShader* pPS) { mpPS = pPS; }
    void SetEnableDepth(bool b) { mEnableDepth = b; }

protected:
    virtual bool GatherShaderResources() override;
    virtual bool GeneratePipelineState() override;

    const VertexShader* mpVS = nullptr;
    const PixelShader* mpPS = nullptr;
    bool mEnableDepth = true;
};

class ComputePass : public Pass
{
public:
    ComputePass(const wchar_t* name) : Pass(name) {}

    virtual bool Bind(ID3D12GraphicsCommandList* pCommandList) const override;

    void SetComputeShader(const ComputeShader* pCS) { mpCS = pCS; }

protected:
    virtual bool GatherShaderResources() override;
    virtual bool GeneratePipelineState() override;

    const ComputeShader* mpCS = nullptr;
};

}

#endif