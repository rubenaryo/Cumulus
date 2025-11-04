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
#include <dxcapi.h>
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
    D3D12_SHADER_VISIBILITY Visibility;
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

struct Shader
{
    Shader(const wchar_t* path);
    virtual ~Shader();

    bool Init(const wchar_t* path);
    bool Release();

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> ShaderBlob;
    ShaderReflectionData ReflectionData;
    BOOL Initialized = false;
};

struct VertexShader : public Shader
{
    VertexShader(const wchar_t* path);

    bool Init(const wchar_t* path);
    bool Release();

    std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
    VertexBufferDescription VertexDesc;
    VertexBufferDescription InstanceDesc; // Note: The allocated memory inside this one is contiguous with VertexDesc, so no additional free's are required.
    BOOL Instanced = false;
};

struct PixelShader : public Shader
{
    PixelShader(const wchar_t* path);
    
    bool Init(const wchar_t* path);
    bool Release();
};

struct ComputeShader : public Shader
{
    ComputeShader(const wchar_t* path);

    bool Init(const wchar_t* path);
    bool Release();
};

}

#endif