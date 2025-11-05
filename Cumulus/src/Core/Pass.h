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

class Pass
{
public:
    Pass(const wchar_t* name) : mName(name) {}
    virtual ~Pass();

    bool Generate();

protected:
    virtual bool GatherShaderResources() = 0;
    bool GeneratePipelineState();
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
    ~GraphicsPass();

    void SetVertexShader(const VertexShader* pVS) { mpVS = pVS; }
    void SetPixelShader(const PixelShader* pPS) { mpPS = pPS; }

protected:
    virtual bool GatherShaderResources() override;

    const VertexShader* mpVS = nullptr;
    const PixelShader* mpPS = nullptr;
};

}

#endif