/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/10
Description : Master Resource Distributor
----------------------------------------------*/
#include "ResourceCodex.h"

#include <Core/PathMacros.h>
#include <Utils/Utils.h>
#include "Factories.h"
#include "Material.h"
#include "Mesh.h"
#include "Shader.h"

#include "hash_util.h"

namespace Muon
{
static ResourceCodex* gCodexInstance = nullptr;

MeshID ResourceCodex::AddMeshFromFile(const char* fileName, const VertexBufferDescription* vertAttr)
{
    ResourceCodex& codexInstance = GetSingleton();

    Mesh mesh;

    MeshID id = MeshFactory::CreateMesh(fileName, vertAttr, mesh);
    auto& hashtable = codexInstance.mMeshMap;
    
    if (hashtable.find(id) == hashtable.end())
    {
        codexInstance.mMeshMap.emplace(id, mesh);
    }
    else
    {
        #if defined(MN_DEBUG)
            Muon::Print("ERROR: Tried to insert repeat mesh\n");
        #endif
        assert(false);
    }
    return id;
}

ResourceCodex& ResourceCodex::GetSingleton()
{
    return *gCodexInstance;
}

void ResourceCodex::Init()
{
    if (gCodexInstance)
    {
        Muon::Print("ERROR: Tried to initialize already initialized ResourceCodex!\n");
        return;
    }

    gCodexInstance = new ResourceCodex();
    gCodexInstance->mMeshStagingBuffer.Create(L"Mesh Staging Buffer", 64 * 1024 * 1024);
    gCodexInstance->mMaterialParamsStagingBuffer.Create(L"material params staging buffer", sizeof(cbMaterialParams));

    ShaderFactory::LoadAllShaders(*gCodexInstance);
    TextureFactory::LoadAllTextures(GetDevice(), GetCommandList(), *gCodexInstance);
    TextureFactory::LoadAllNVDF(GetDevice(), GetCommandList(), *gCodexInstance);
    MaterialFactory::CreateAllMaterials(*gCodexInstance);
}

void ResourceCodex::Destroy()
{
    for (auto& m : gCodexInstance->mMeshMap)
    {
        Mesh& mesh = m.second;
        mesh.Release();
    }
    gCodexInstance->mMeshMap.clear();

    gCodexInstance->mMeshStagingBuffer.Destroy();

    for (auto& m : gCodexInstance->mMaterialMap)
    {
        Material& mat = m.second;
        mat.Destroy();
    }
    gCodexInstance->mMaterialMap.clear();

    gCodexInstance->mMaterialParamsStagingBuffer.Destroy();

    for (auto& s : gCodexInstance->mVertexShaders)
    {
        VertexShader& vs = s.second;
        vs.Release();
    }
    gCodexInstance->mVertexShaders.clear();

    for (auto& s : gCodexInstance->mPixelShaders)
    {
        PixelShader& ps = s.second;
        ps.Release();
    }
    gCodexInstance->mPixelShaders.clear();

    for (auto& t : gCodexInstance->mTextureMap)
    {
        Texture& tex = t.second;
        tex.Destroy();
    }
    gCodexInstance->mTextureMap.clear();

    delete gCodexInstance;
    gCodexInstance = nullptr;
}

const Mesh* ResourceCodex::GetMesh(MeshID UID) const
{
    if(mMeshMap.find(UID) != mMeshMap.end())
        return &mMeshMap.at(UID);
    else
        return nullptr;
}

const VertexShader* ResourceCodex::GetVertexShader(ShaderID UID) const
{
    if(mVertexShaders.find(UID) != mVertexShaders.end())
        return &mVertexShaders.at(UID);
    else
        return nullptr;
}

const PixelShader* ResourceCodex::GetPixelShader(ShaderID UID) const
{
    if(mPixelShaders.find(UID) != mPixelShaders.end())
        return &mPixelShaders.at(UID);
    else
        return nullptr;
}

const ComputeShader* ResourceCodex::GetComputeShader(ShaderID UID) const
{
    if (mComputeShaders.find(UID) != mComputeShaders.end())
        return &mComputeShaders.at(UID);
    else
        return nullptr;
}

const Material* ResourceCodex::GetMaterialType(MaterialID UID) const
{
    if (mMaterialMap.find(UID) != mMaterialMap.end())
        return &mMaterialMap.at(UID);
    else
        return nullptr;
}

const Texture* ResourceCodex::GetTexture(TextureID UID) const
{
    if (mTextureMap.find(UID) != mTextureMap.end())
        return &mTextureMap.at(UID);
    else
        return nullptr;
}

void ResourceCodex::AddVertexShader(ShaderID hash, const wchar_t* path)
{   
    mVertexShaders.emplace(hash, path);
}

void ResourceCodex::AddPixelShader(ShaderID hash, const wchar_t* path)
{   
    mPixelShaders.emplace(hash, path);
}

void ResourceCodex::AddComputeShader(ShaderID hash, const wchar_t* path)
{
    mComputeShaders.emplace(hash, path);
}

Texture& ResourceCodex::InsertTexture(TextureID hash)
{
    if (mTextureMap.find(hash) != mTextureMap.end())
    {
        Muon::Printf(L"Warninr: Attempted to insert duplicate textureID: 0x%08x!\n", hash);
    }

    return mTextureMap[hash];
}

Material* ResourceCodex::InsertMaterialType(const wchar_t* name)
{
    if (!name)
        return nullptr;

    MaterialID typeId = fnv1a(name);
    auto emplaceResult = mMaterialMap.emplace(typeId, name);
    if (emplaceResult.second == false)
        return nullptr;

    return &emplaceResult.first->second;
}

}