/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2020/10
Description : Loads and distributes all static resources (materials, textures, etc)
----------------------------------------------*/
#ifndef RESOURCECODEX_H
#define RESOURCECODEX_H

#include <Core/DXCore.h>

#include <Core/CommonTypes.h>
#include <Core/Material.h>
#include <Core/Mesh.h>
#include <Core/Shader.h>
#include <Core/Buffers.h>
#include <Core/DescriptorHeap.h>

#include <unordered_map>
#include <memory>

namespace Muon
{
struct MeshFactory;
struct ShaderFactory;
struct TextureFactory;
}

namespace Muon
{

class alignas(8) ResourceCodex
{
public:
    static MeshID AddMeshFromFile(const char* fileName, const VertexBufferDescription* vertAttr);
    
    // Singleton Stuff
    static void Init();
    static void Destroy();

    static ResourceCodex& GetSingleton();

    const Mesh* GetMesh(MeshID UID) const;
    
    const VertexShader* GetVertexShader(ShaderID UID) const;
    const PixelShader* GetPixelShader(ShaderID UID) const;
    const ComputeShader* GetComputeShader(ShaderID UID) const;
    const Material* GetMaterialType(MaterialID UID) const;
    const MuonTexture* GetTexture(TextureID UID) const;
    UploadBuffer& GetMeshStagingBuffer() { return mMeshStagingBuffer; }
    UploadBuffer& GetMatParamsStagingBuffer() { return mMaterialParamsStagingBuffer; }
    UploadBuffer& Get2DTextureStagingBuffer() { return m2DTextureStagingBuffer; }
    UploadBuffer& Get3DTextureStagingBuffer() { return m3DTextureStagingBuffer; }

private:
    std::unordered_map<ShaderID, VertexShader>  mVertexShaders;
    std::unordered_map<ShaderID, PixelShader>   mPixelShaders;
    std::unordered_map<ShaderID, ComputeShader> mComputeShaders;
    std::unordered_map<MeshID, Mesh>            mMeshMap;
    std::unordered_map<TextureID, MuonTexture>  mTextureMap;
    std::unordered_map<MaterialID, Material>    mMaterialMap;

    // An intermediate upload buffer used for uploading vertex/index data to the GPU
    UploadBuffer mMeshStagingBuffer;
    UploadBuffer mMaterialParamsStagingBuffer;
    UploadBuffer m2DTextureStagingBuffer;
    UploadBuffer m3DTextureStagingBuffer;

private:
    friend struct TextureFactory;
    MuonTexture& InsertTexture(TextureID hash);

    friend struct MaterialFactory;
    Material* InsertMaterialType(const wchar_t* name);

    friend struct ShaderFactory;
    void AddVertexShader(ShaderID hash, const wchar_t* path);
    void AddPixelShader(ShaderID hash, const wchar_t* path);
    void AddComputeShader(ShaderID hash, const wchar_t* path);
};
}
#endif