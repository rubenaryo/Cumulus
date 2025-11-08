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
#include <Core/Texture.h>

#include <DirectXTex.h>

#include <Utils/HashUtils.h>

namespace Muon
{
static ResourceCodex* gCodexInstance = nullptr;

bool ResourceCodex::RegisterMesh(Mesh& m)
{
    ResourceID id = GetResourceID(m.GetName());   

    #if defined(MN_DEBUG)
    if (mMeshMap.find(id) != mMeshMap.end())
    {
        Muon::Printf("ERROR: Tried to insert repeat mesh: %s\n", m.GetName());
    }
    #endif
    
    mMeshMap[id] = m;
    return true;
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
    gCodexInstance->mMeshStagingBuffer.Create(L"Mesh Staging Buffer", 64 * 1024 * 1024); // 64MB of models can be staged at once
    gCodexInstance->mMaterialParamsStagingBuffer.Create(L"Material Params Staging Buffer", sizeof(cbMaterialParams)); // Exactly one set of material params can be staged at once
    gCodexInstance->m2DTextureStagingBuffer.Create(L"2D Staging Buffer", 512 * 512 * 4 * sizeof(float) * 64); // 64 512x512 2D textures at once
    gCodexInstance->m3DTextureStagingBuffer.Create(L"NVDF Staging Buffer",4 * (512 * 512 * 128 * 4 * sizeof(float))); // 4 512x512x128 3D textures at once 

    // TODO: Optimization opportunity.
    // Some of these load operations are CPU-heavy. 
    // At the moment we do all the CPU load upfront, 
    // only scheduling what to do in the GPU for later when the initialization command list is executed.
    // Idea: We should open, schedule, then execute command lists intermittently so the GPU isn't idle while we
    // do heavy CPU work (like loading models, or 3d textures, etc)
    // Of note are model loading and 3D texture loading, which can be quite expensive. 
    ShaderFactory::LoadAllShaders(*gCodexInstance);
    MeshFactory::LoadAllMeshes(*gCodexInstance);
    TextureFactory::LoadAllTextures(GetDevice(), GetCommandList(), *gCodexInstance);
    TextureFactory::LoadAllNVDF(GetDevice(), GetCommandList(), *gCodexInstance);
    MaterialFactory::CreateAllMaterials(*gCodexInstance);
}

void ResourceCodex::Destroy()
{
    for (auto& m : gCodexInstance->mMeshMap)
    {
        Mesh& mesh = m.second;
        mesh.Destroy();
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
    gCodexInstance->m2DTextureStagingBuffer.Destroy();
    gCodexInstance->m3DTextureStagingBuffer.Destroy();

    Muon::SetOffscreenTarget(nullptr); // offscreen target destruction now owned by the codex

    delete gCodexInstance;
    gCodexInstance = nullptr;
}

const Mesh* ResourceCodex::GetMesh(ResourceID UID) const
{
    if(mMeshMap.find(UID) != mMeshMap.end())
        return &mMeshMap.at(UID);
    else
        return nullptr;
}

const VertexShader* ResourceCodex::GetVertexShader(ResourceID UID) const
{
    if(mVertexShaders.find(UID) != mVertexShaders.end())
        return &mVertexShaders.at(UID);
    else
        return nullptr;
}

const PixelShader* ResourceCodex::GetPixelShader(ResourceID UID) const
{
    if(mPixelShaders.find(UID) != mPixelShaders.end())
        return &mPixelShaders.at(UID);
    else
        return nullptr;
}

const ComputeShader* ResourceCodex::GetComputeShader(ResourceID UID) const
{
    if (mComputeShaders.find(UID) != mComputeShaders.end())
        return &mComputeShaders.at(UID);
    else
        return nullptr;
}

const Material* ResourceCodex::GetMaterialType(ResourceID UID) const
{
    if (mMaterialMap.find(UID) != mMaterialMap.end())
        return &mMaterialMap.at(UID);
    else
        return nullptr;
}

const Texture* ResourceCodex::GetTexture(ResourceID UID) const
{
    if (mTextureMap.find(UID) != mTextureMap.end())
        return &mTextureMap.at(UID);
    else
        return nullptr;
}

Texture* ResourceCodex::GetTexture(ResourceID UID)
{
    if (mTextureMap.find(UID) != mTextureMap.end())
        return &mTextureMap.at(UID);
    else
        return nullptr;
}

void ResourceCodex::AddVertexShader(ResourceID hash, const wchar_t* path)
{   
    mVertexShaders.emplace(hash, path);
}

void ResourceCodex::AddPixelShader(ResourceID hash, const wchar_t* path)
{   
    mPixelShaders.emplace(hash, path);
}

void ResourceCodex::AddComputeShader(ResourceID hash, const wchar_t* path)
{
    mComputeShaders.emplace(hash, path);
}

Texture& ResourceCodex::InsertTexture(ResourceID hash)
{
    if (mTextureMap.find(hash) != mTextureMap.end())
    {
        Muon::Printf(L"Warning: Attempted to insert duplicate ResourceID: 0x%08x!\n", hash);
    }

    return mTextureMap[hash];
}

Material* ResourceCodex::InsertMaterialType(const wchar_t* name)
{
    if (!name)
        return nullptr;

    ResourceID typeId = GetResourceID(name);
    auto emplaceResult = mMaterialMap.emplace(typeId, name);
    if (emplaceResult.second == false)
        return nullptr;

    return &emplaceResult.first->second;
}

}