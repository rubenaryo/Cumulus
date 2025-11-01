/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Useful functions for shader reflection system
----------------------------------------------*/
#include <Core/ShaderUtils.h>
#include <Core/Shader.h>
#include <Utils/Utils.h>

#include <DirectXMath.h>
#include <unordered_map>

namespace Muon
{
    static const char* kSemanticNames[] =
    {
        "POSITION",
        "NORMAL",
        "TEXCOORD",
        "TANGENT",
        "BINORMAL",
        "COLOR",
        "BLENDINDICES",
        "BLENDWEIGHTS",
        "WORLDMATRIX"
    };

    static const char* kInstancedSemanticNames[] =
    {
        "INSTANCE_POSITION",
        "INSTANCE_NORMAL",
        "INSTANCE_TEXCOORD,",
        "INSTANCE_TANGENT",
        "INSTANCE_BINORMAL",
        "INSTANCE_COLOR",
        "INSTANCE_BLENDINDICES",
        "INSTANCE_BLENDWEIGHTS",
        "INSTANCE_WORLDMATRIX"
    };

    ParameterDesc::ParameterDesc(const char* name, ParameterType type)
        : Name(name)
        , Type(type)
    {
    }

    size_t GetParamTypeSize(ParameterType type)
    {
        static size_t sParamSizes[] =
        {
            sizeof(int),
            sizeof(float),
            sizeof(DirectX::XMFLOAT2),
            sizeof(DirectX::XMFLOAT3),
            sizeof(DirectX::XMFLOAT4),
        };
        //static_assert(std::size(sParamSizes) == (UINT)ParameterType::Count);

        return sParamSizes[(UINT)type];
    }

    // Helper to convert D3D type to ParameterType
    static ParameterType D3DTypeToParameterType(const D3D12_SHADER_TYPE_DESC& typeDesc)
    {
        if (typeDesc.Class == D3D_SVC_SCALAR)
        {
            if (typeDesc.Type == D3D_SVT_FLOAT)
                return ParameterType::Float;
            else if (typeDesc.Type == D3D_SVT_INT)
                return ParameterType::Int;
        }
        else if (typeDesc.Class == D3D_SVC_VECTOR)
        {
            if (typeDesc.Type == D3D_SVT_FLOAT)
            {
                switch (typeDesc.Columns)
                {
                case 2: return ParameterType::Float2;
                case 3: return ParameterType::Float3;
                case 4: return ParameterType::Float4;
                }
            }
        }
        else if (typeDesc.Class == D3D_SVC_MATRIX_COLUMNS || typeDesc.Class == D3D_SVC_MATRIX_ROWS)
        {
            if (typeDesc.Rows == 4 && typeDesc.Columns == 4)
                return ParameterType::Matrix4x4;
        }

        return ParameterType::Invalid;
    }

    bool ParseReflectedResources(ID3D12ShaderReflection* pReflection, ShaderReflectionData& outShaderReflectionData)
    {
        D3D12_SHADER_DESC shaderDesc;
        pReflection->GetDesc(&shaderDesc);

        // Reflect bound resources
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            pReflection->GetResourceBindingDesc(i, &bindDesc);

            ShaderResourceBinding resource;
            resource.Name = bindDesc.Name;
            resource.BindPoint = bindDesc.BindPoint;
            resource.BindCount = bindDesc.BindCount;
            resource.Space = bindDesc.Space;
            resource.Size = 0;

            switch (bindDesc.Type)
            {
            case D3D_SIT_CBUFFER:
                resource.Type = ShaderResourceType::ConstantBuffer;
                break;
            case D3D_SIT_TEXTURE:
                resource.Type = ShaderResourceType::Texture;
                break;
            case D3D_SIT_SAMPLER:
                resource.Type = ShaderResourceType::Sampler;
                break;
            case D3D_SIT_UAV_RWTYPED:
                resource.Type = ShaderResourceType::RWTexture;
                break;
            case D3D_SIT_STRUCTURED:
                resource.Type = ShaderResourceType::StructuredBuffer;
                break;
            case D3D_SIT_UAV_RWSTRUCTURED:
                resource.Type = ShaderResourceType::RWStructuredBuffer;
                break;
            default:
                continue;
            }

            // If it's a constant buffer, reflect its contents
            if (resource.Type == ShaderResourceType::ConstantBuffer)
            {
                ID3D12ShaderReflectionConstantBuffer* pCBReflection =
                    pReflection->GetConstantBufferByName(bindDesc.Name);

                D3D12_SHADER_BUFFER_DESC cbDesc;
                pCBReflection->GetDesc(&cbDesc);

                ConstantBufferReflection cbReflection;
                cbReflection.Name = bindDesc.Name;
                cbReflection.BindPoint = bindDesc.BindPoint;
                cbReflection.Space = bindDesc.Space;
                cbReflection.Size = cbDesc.Size;

                // Reflect variables
                for (UINT v = 0; v < cbDesc.Variables; ++v)
                {
                    ID3D12ShaderReflectionVariable* pVar = pCBReflection->GetVariableByIndex(v);
                    D3D12_SHADER_VARIABLE_DESC varDesc;
                    pVar->GetDesc(&varDesc);

                    ID3D12ShaderReflectionType* pType = pVar->GetType();
                    D3D12_SHADER_TYPE_DESC typeDesc;
                    pType->GetDesc(&typeDesc);

                    ParameterDesc param;
                    param.Name = varDesc.Name;
                    param.Offset = varDesc.StartOffset;
                    param.Type = D3DTypeToParameterType(typeDesc);
                    param.ConstantBufferName = bindDesc.Name;

                    if (param.Type != ParameterType::Invalid)
                        cbReflection.Variables.push_back(param);
                }

                outShaderReflectionData.ConstantBuffers.push_back(cbReflection);

                // Update resource size
                resource.Size = cbDesc.Size;
            }
            outShaderReflectionData.Resources.push_back(resource);
        }

        outShaderReflectionData.IsReflected = true;
        return true;
    }

    // Previously called AssignDXGIFormatsAndByteOffsets
    void PopulateInputElements(D3D12_INPUT_CLASSIFICATION slotClass,
        std::vector < D3D12_SIGNATURE_PARAMETER_DESC> paramDescs,
        UINT numInputs,
        std::vector<D3D12_INPUT_ELEMENT_DESC>& out_inputParams,
        uint16_t* out_byteOffsets,
        uint16_t& out_byteSize)
    {
        uint16_t totalByteSize = 0;
        for (uint8_t i = 0; i != numInputs; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC& paramDesc = paramDescs[i];

            out_inputParams.push_back(D3D12_INPUT_ELEMENT_DESC());
            D3D12_INPUT_ELEMENT_DESC& inputParam = out_inputParams.back();

            size_t length = 1 + strnlen_s(paramDesc.SemanticName, 256);
            char* buf = new CHAR[length];
            memset(buf, 0, length);
            strncpy(buf, paramDesc.SemanticName, length);
            inputParam.SemanticName = buf;
            inputParam.SemanticIndex = paramDesc.SemanticIndex;
            inputParam.InputSlotClass = slotClass;

            if (slotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA)
            {
                inputParam.InputSlot = 0;
                inputParam.InstanceDataStepRate = 0;
            }
            else // per instance data
            {
                inputParam.InputSlot = 1;
                inputParam.InstanceDataStepRate = 1;
            }

            out_byteOffsets[i] = totalByteSize;
            inputParam.AlignedByteOffset = totalByteSize;

            // determine DXGI format ... Thanks MSDN!
            if (paramDesc.Mask == 1) // R
            {
                totalByteSize += 4;
                if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)   inputParam.Format = DXGI_FORMAT_R32_UINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)   inputParam.Format = DXGI_FORMAT_R32_SINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)   inputParam.Format = DXGI_FORMAT_R32_FLOAT;
            }
            else if (paramDesc.Mask <= 3) // RG
            {
                totalByteSize += 8;
                if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)   inputParam.Format = DXGI_FORMAT_R32G32_UINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)   inputParam.Format = DXGI_FORMAT_R32G32_SINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)   inputParam.Format = DXGI_FORMAT_R32G32_FLOAT;
            }
            else if (paramDesc.Mask <= 7) // RGB
            {
                totalByteSize += 12;
                if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)   inputParam.Format = DXGI_FORMAT_R32G32B32_UINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)   inputParam.Format = DXGI_FORMAT_R32G32B32_SINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)   inputParam.Format = DXGI_FORMAT_R32G32B32_FLOAT;
            }
            else if (paramDesc.Mask <= 15) // RGBA
            {
                totalByteSize += 16;
                if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)   inputParam.Format = DXGI_FORMAT_R32G32B32A32_UINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)   inputParam.Format = DXGI_FORMAT_R32G32B32A32_SINT;
                else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)   inputParam.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            }


            out_inputParams[i] = inputParam;
        }

        out_byteSize = totalByteSize;
    }

    bool BuildInputLayout(ID3D12ShaderReflection* pReflection, ID3DBlob* pBlob, VertexShader* out_shader)
    {
        if (!pReflection || !pBlob || !out_shader)
            return false;

        D3D12_SHADER_DESC shaderDesc;
        pReflection->GetDesc(&shaderDesc);
        const UINT numInputs = shaderDesc.InputParameters;

        Semantics* semanticsArr = new Semantics[numInputs];
        std::vector<D3D12_SIGNATURE_PARAMETER_DESC> paramDescs(numInputs);

        out_shader->Instanced = false;
        ZeroMemory(&out_shader->InstanceDesc, sizeof(VertexBufferDescription));

        uint8_t numInstanceInputs = 0;
        uint8_t instanceStartIdx = UINT8_MAX;

        // First, build semantics enum array from the strings gotten from the reflection
        // If any of the strings are marked as being for instancing, then split off into initialization of two vertex buffers
        const char** comparisonArray = kSemanticNames;
        for (uint8_t i = 0; i != numInputs; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC* pDesc = &paramDescs[i];

            HRESULT hr = pReflection->GetInputParameterDesc(i, pDesc);
            COM_EXCEPT(hr);

            for (semantic_t s = 0; s != (semantic_t)Semantics::COUNT; ++s)
            {
                // Look for the existence of each HLSL semantic
                LPCSTR& semanticName = pDesc->SemanticName;
                if (!strcmp(semanticName, comparisonArray[s]))
                {
                    semanticsArr[i] = (Semantics)s;
                    break;
                }
                else if (numInstanceInputs == 0 && strstr(semanticName, "INSTANCE_")) // This is not great, but just look for the special "INSTANCE_" prefix
                {
                    out_shader->Instanced = true; // Now we know this is an instanced shader
                    numInstanceInputs = numInputs - i;
                    instanceStartIdx = i--; // decrement i so we scan this semanticName again
                    comparisonArray = kInstancedSemanticNames; // By convention: we are done with regular vertex attributes, switch to comparing with the instanced versions
                    break;
                }
            }
        }

        // Now the temp array has all the semantics properly labeled. Use it to create the vertex buffer and the instance buffer(if applicable)
        VertexBufferDescription vbDesc;

        vbDesc.ByteOffsets = new uint16_t[numInputs];//(uint16_t*)malloc(sizeof(uint16_t) * numInputs);
        PopulateInputElements(D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, paramDescs, numInputs, out_shader->InputElements, vbDesc.ByteOffsets, vbDesc.ByteSize);
        vbDesc.SemanticsArr = semanticsArr;
        vbDesc.AttrCount = numInputs;
        out_shader->VertexDesc = vbDesc;

        return true;
    }

    bool MergeReflectionData(const ShaderReflectionData& vsData, const ShaderReflectionData& psData, std::vector<ShaderResourceBinding>& outResources, std::vector<ConstantBufferReflection>& outCBs)
    {
        outResources.clear();
        outCBs.clear();

        // Merge resources - combine from both shaders
        // VS resources come first, then PS resources
        for (const auto& res : vsData.Resources)
            outResources.push_back(res);

        for (const auto& res : psData.Resources)
            outResources.push_back(res);

        // Merge constant buffers - check for duplicates by name
        std::unordered_map<std::string, size_t> cbNameToIndex;

        for (const auto& cb : vsData.ConstantBuffers)
        {
            outCBs.push_back(cb);
            cbNameToIndex[cb.Name] = outCBs.size() - 1;
        }

        for (const auto& cb : psData.ConstantBuffers)
        {
            auto it = cbNameToIndex.find(cb.Name);
            if (it != cbNameToIndex.end())
            {
                // CB exists in both shaders - merge variables
                auto& existingCB = outCBs[it->second];

                // Verify same size and binding
                if (existingCB.Size != cb.Size ||
                    existingCB.BindPoint != cb.BindPoint ||
                    existingCB.Space != cb.Space)
                {
                    Printf("Warning: Constant buffer '%s' has different properties in VS and PS!\n", cb.Name.c_str());
                }

                // Merge variables (avoid duplicates)
                for (const auto& var : cb.Variables)
                {
                    // TODO: This feels a bit slow
                    bool found = false;
                    for (const auto& existingVar : existingCB.Variables)
                    {
                        if (existingVar.Name == var.Name)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        existingCB.Variables.push_back(var);
                }
            }
            else
            {
                // New CB only in pixel shader
                outCBs.push_back(cb);
                cbNameToIndex[cb.Name] = outCBs.size() - 1;
            }
        }

        return true;
    }

}