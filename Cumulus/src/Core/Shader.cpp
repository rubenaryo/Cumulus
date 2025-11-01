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

/////////////////////////////////////////////////////////

VertexShader::VertexShader(const wchar_t* path)
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

    HRESULT hr = D3DReadFileToBlob(path, this->ShaderBlob.GetAddressOf());
    COM_EXCEPT(hr);

    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> pReflection;
    hr = D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
        IID_ID3D12ShaderReflection, (void**)pReflection.GetAddressOf());

    if (FAILED(hr))
        return false;

    if (!BuildInputLayout(pReflection.Get(), ShaderBlob.Get(), this))
        return false;

    Initialized = ParseReflectedResources(pReflection.Get(), this->ReflectionData);
    return SUCCEEDED(hr);
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

PixelShader::PixelShader(const wchar_t* path)
{
    Init(path);
}

bool PixelShader::Init(const wchar_t* path)
{
    HRESULT hr = D3DReadFileToBlob(path, this->ShaderBlob.GetAddressOf());
    COM_EXCEPT(hr);

    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> pReflection;
    hr = D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
        IID_ID3D12ShaderReflection, (void**)pReflection.GetAddressOf());

    if (FAILED(hr))
        return false;

    Initialized = ParseReflectedResources(pReflection.Get(), this->ReflectionData);
    return SUCCEEDED(hr);
}

bool PixelShader::Release()
{
    // This will get filled out once we're dealing with samplers and such
    return true;
}


}