/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Useful functions for shader reflection system
----------------------------------------------*/
#ifndef MUON_SHADERUTILS_H
#define MUON_SHADERUTILS_H

#include <Core/DXCore.h>

namespace Muon
{
    struct VertexShader;
    struct PixelShader;
    struct ShaderReflectionData;
    struct ShaderResourceBinding;
    struct ConstantBufferReflection;

    enum class ParameterType;
}

namespace Muon
{
size_t GetParamTypeSize(ParameterType type);

ParameterType D3DTypeToParameterType(const D3D12_SHADER_TYPE_DESC& typeDesc);
    
bool ParseReflectedResources(ID3D12ShaderReflection* pReflection, 
    ShaderReflectionData& outShaderReflectionData);

void PopulateInputElements(D3D12_INPUT_CLASSIFICATION slotClass,
    std::vector<D3D12_SIGNATURE_PARAMETER_DESC> paramDescs,
    UINT numInputs,
    std::vector<D3D12_INPUT_ELEMENT_DESC>& out_inputParams,
    uint16_t* out_byteOffsets,
    uint16_t& out_byteSize);

bool BuildInputLayout(ID3D12ShaderReflection* pReflection, 
    ID3DBlob* pBlob, 
    VertexShader* out_shader);

bool MergeReflectionData(
    const ShaderReflectionData& vsData,
    const ShaderReflectionData& psData,
    std::vector<ShaderResourceBinding>& outResources,
    std::vector<ConstantBufferReflection>& outCBs);

}
#endif