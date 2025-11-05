/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Implementations of Muon shader objects
----------------------------------------------*/

#include <Core/Shader.h>
#include <Core/ShaderUtils.h>
#include <Core/ThrowMacros.h>
#include <Utils/Utils.h>

namespace Muon
{

Shader::Shader(const wchar_t* path)
{
}

Shader::~Shader()
{
}

bool Shader::Init(const wchar_t* path)
{
    if (!LoadBlob(path, this->ShaderBlob.GetAddressOf()))
        return false;

    return ReflectAndParse(this->ShaderBlob.Get(), *this);
}

bool Shader::Release()
{
    return true;
}

/////////////////////////////////////////////////////////

VertexShader::VertexShader(const wchar_t* path) : Shader(path)
{
    Init(path);
}

bool VertexShader::Init(const wchar_t* path)
{
    if (Initialized)
    {
        Muon::Printf(L"Warning: Attempted to initialized [%s]. When it was already initialized!", path);
        return false;
    }

    if (!Shader::Init(path))
        return false;

    Initialized = ReflectAndBuildInputLayout(this->ShaderBlob.Get(), *this);
    return Initialized;
}

bool VertexShader::Release()
{
    bool released = false;

    if (VertexDesc.SemanticsArr)
    {
        delete[] VertexDesc.SemanticsArr;
        VertexDesc.SemanticsArr = nullptr;
        released = true;
    }

    if (VertexDesc.ByteOffsets)
    {
        delete[] VertexDesc.ByteOffsets;
        VertexDesc.ByteOffsets = nullptr;
        released = true;
    }

    for (D3D12_INPUT_ELEMENT_DESC& param : InputElements)
    {
        if (param.SemanticName)
        {
            delete[] param.SemanticName;
            param.SemanticName = nullptr;
            released = true;
        }
    }

    return released;
}



//////////////////////////////////////////////////////////////////////////////////

PixelShader::PixelShader(const wchar_t* path) : Shader(path)
{
    Init(path);
}

bool PixelShader::Init(const wchar_t* path)
{
    if (!Shader::Init(path))
        return false;

    Initialized = true;
    return Initialized;
}

bool PixelShader::Release()
{
    // This will get filled out once we're dealing with samplers and such
    return true;
}

//////////////////////////////////////////////////////////////////////////////////

ComputeShader::ComputeShader(const wchar_t* path) : Shader(path)
{
    Init(path);
}

bool ComputeShader::Init(const wchar_t* path)
{
    if (!Shader::Init(path))
        return false;

    Initialized = true;
    return Initialized;
}

bool ComputeShader::Release()
{
    return true;
}


}