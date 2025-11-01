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

#include <ResourceUploadBatch.h>

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

struct Texture
{
    Microsoft::WRL::ComPtr<ID3D12Resource> pResource;
    D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = { 0 };

    UINT Width = 0;
    UINT Height = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

    bool IsValid() const { return pResource != nullptr && GPUHandle.ptr != 0; }
    void Destroy()
    {     
        pResource.Reset();
        CPUHandle = { 0 };
        GPUHandle = { 0 };
        Width = 0;
        Height = 0;
        Format = DXGI_FORMAT_UNKNOWN;
    }
};

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
    const MaterialType* GetMaterialType(MaterialTypeID UID) const;
    const Texture* GetTexture(TextureID UID) const;
    UploadBuffer& GetMeshStagingBuffer() { return mMeshStagingBuffer; }
    UploadBuffer& GetMatParamsStagingBuffer() { return mMaterialParamsStagingBuffer; }
    DescriptorHeap& GetSRVDescriptorHeap() { return mSRVDescriptorHeap; }
    DirectX::ResourceUploadBatch* GetUploadBatch() { return mTextureUploadBatch.get(); }

private:
    std::unordered_map<ShaderID, VertexShader>  mVertexShaders;
    std::unordered_map<ShaderID, PixelShader>   mPixelShaders;
    std::unordered_map<MeshID, Mesh>            mMeshMap;
    std::unordered_map<TextureID, Texture>      mTextureMap;
    std::unordered_map<MaterialTypeID, MaterialType> mMaterialTypeMap;

    // An intermediate upload buffer used for uploading vertex/index data to the GPU
    UploadBuffer mMeshStagingBuffer;
    UploadBuffer mMaterialParamsStagingBuffer;
    std::unique_ptr<DirectX::ResourceUploadBatch> mTextureUploadBatch;

    DescriptorHeap mSRVDescriptorHeap;

private:
    friend struct TextureFactory;
    Texture& InsertTexture(TextureID hash);
    
    friend struct MaterialFactory;
    //MaterialIndex PushMaterial(const Material& material);
    MaterialType* InsertMaterialType(const wchar_t* name);

    friend struct ShaderFactory;
    void AddVertexShader(ShaderID hash, const wchar_t* path);
    void AddPixelShader(ShaderID hash, const wchar_t* path);
};
}
#endif