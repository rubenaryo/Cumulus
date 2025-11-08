/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/4
Description : Implementation of custom material system
----------------------------------------------*/
#include <Core/Material.h>

namespace Muon
{

Material::Material(const wchar_t* name)
	:	mName(name)
{
    std::wstring bufferName = mName + L"_ParamsBuffer";
    mMaterialParamsBuffer.Create(bufferName.c_str(), sizeof(cbMaterialParams));
}

void Material::Destroy()
{
    mMaterialParamsBuffer.Destroy();
}

bool Material::PopulateMaterialParams(UploadBuffer& stagingBuffer, ID3D12GraphicsCommandList* pCommandList)
{
    return mMaterialParamsBuffer.Populate(&mMaterialParams, sizeof(cbMaterialParams), stagingBuffer, pCommandList);
}

bool Material::SetTextureParam(const char* paramName, ResourceID texId)
{
    mTextureParams[paramName] = texId;
    return true;
}

}
