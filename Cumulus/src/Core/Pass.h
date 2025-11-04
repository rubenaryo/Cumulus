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

class ComputePass
{
public:
    ComputePass(const wchar_t* name) : mName(name) {}
    ~ComputePass();

    void SetComputeShader(const ComputeShader* pCS) { mpCS = pCS; }
    bool Generate();

protected:
    bool GeneratePipelineState();
    bool GenerateRootSignature();

    const ComputeShader* mpCS = nullptr;

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

}

#endif