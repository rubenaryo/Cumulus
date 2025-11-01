/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/3
Description : Wrapper for Vertex/Pixel/other shader code
----------------------------------------------*/
#ifndef SHADER_H
#define SHADER_H

#include "ThrowMacros.h"

#include <vector>
#include <wrl/client.h>
#include <Core/DXCore.h>

namespace Muon
{

enum class ShaderResourceType
{
    ConstantBuffer,
    Texture,
    Sampler,
    RWTexture,
    StructuredBuffer,
    RWStructuredBuffer
};

struct ShaderResourceBinding
{
    std::string Name;
    ShaderResourceType Type;
    UINT BindPoint;
    UINT BindCount;
    UINT Space;
    UINT Size; // For constant buffers
};

enum class ParameterType
{
    Int = 0,
    Float,
    Float2,
    Float3,
    Float4,
    Matrix4x4,
    Count,
    Invalid
};

struct ParameterDesc
{
    ParameterDesc() = default;
    ParameterDesc(const char* name, ParameterType type);
    std::string Name;
    ParameterType Type = ParameterType::Invalid;
    UINT Index = 0;
    UINT Offset = 0;
    std::string ConstantBufferName; // Which CB this belongs to
};

struct ConstantBufferReflection
{
    std::string Name;
    UINT BindPoint;
    UINT Space;
    UINT Size;
    std::vector<ParameterDesc> Variables;
};

// Base shader reflection data
struct ShaderReflectionData
{
    std::vector<ShaderResourceBinding> Resources;
    std::vector<ConstantBufferReflection> ConstantBuffers;
    bool IsReflected = false;
};

#pragma region VertexShader Stuff
typedef uint8_t semantic_t;
enum class Semantics : semantic_t
{
    POSITION,
    NORMAL,
    TEXCOORD,
    TANGENT,
    BINORMAL,
    COLOR,
    BLENDINDICES,
    BLENDWEIGHTS,
    WORLDMATRIX,
    COUNT
};

struct VertexBufferDescription
{
    Semantics* SemanticsArr = nullptr;
    uint16_t*  ByteOffsets = nullptr;
    uint16_t   AttrCount = 0;
    uint16_t   ByteSize = 0;
};
#pragma endregion


struct VertexShader
{
    VertexShader() = default;
    VertexShader(const wchar_t* path);

    bool Init(const wchar_t* path);
    bool Release();

    std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
    Microsoft::WRL::ComPtr<ID3DBlob> ShaderBlob;
    VertexBufferDescription VertexDesc;
    VertexBufferDescription InstanceDesc; // Note: The allocated memory inside this one is contiguous with VertexDesc, so no additional free's are required.

    ShaderReflectionData ReflectionData;
    BOOL Initialized = false;
    BOOL Instanced = false;
};

struct PixelShader
{
    PixelShader() = default;
    PixelShader(const wchar_t* path);
    
    bool Init(const wchar_t* path);
    bool Release();

    Microsoft::WRL::ComPtr<ID3DBlob> ShaderBlob;
    ShaderReflectionData ReflectionData;
    BOOL Initialized = false;
};

}

#endif