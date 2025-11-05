/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/2
Description : Material class for shader information
----------------------------------------------*/
#ifndef MATERIAL_H
#define MATERIAL_H

#include <Core/DXCore.h>
#include "CBufferStructs.h"

#include <Core/Buffers.h>
#include <Core/CommonTypes.h>
#include <Core/PipelineState.h>
#include <Core/Shader.h>
#include <unordered_map>
#include <string>

namespace Muon
{
struct VertexShader;
struct PixelShader;
}

namespace Muon
{

class Material
{
public:
    Material(const wchar_t* name);
    void Destroy();

    bool Bind(ID3D12GraphicsCommandList* pCommandList) const;

    const std::wstring& GetName() const { return mName; }
    void SetMaterialParams(cbMaterialParams& params) { mMaterialParams = params; }
    bool PopulateMaterialParams(UploadBuffer& stagingBuffer, ID3D12GraphicsCommandList* pCommandList);

    bool SetTextureParam(const char* paramName, TextureID texId);
    const std::unordered_map<std::string, TextureID>& GetTextureParams() const { return mTextureParams; }

    const DefaultBuffer& GetParamBuffer() const { return mMaterialParamsBuffer; }

protected:
    std::unordered_map<std::string, TextureID> mTextureParams;
    std::wstring mName;

    DefaultBuffer mMaterialParamsBuffer;
    cbMaterialParams mMaterialParams;
};

}
#endif