#ifndef FACTORIES_H
#define FACTORIES_H

#include "DXCore.h"
#include <Core/CommonTypes.h>
#include <Core/ResourceCodex.h>
#include "Shader.h"

#include <utility>
#include <filesystem>

namespace Muon
{

struct ShaderFactory final
{
    friend class ResourceCodex;

    // Main initialization function: Takes a codex, which then calls the static functions from ShaderFactory to populate its own hashtables
    static void LoadAllShaders(ResourceCodex& codex);
};

struct TextureFactory final
{
    static void LoadAllTextures(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static bool Upload3DTextureFromData(const wchar_t* textureName, void* data, size_t width, size_t height, size_t depth, DXGI_FORMAT fmt, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static void LoadTexturesForNVDF(std::filesystem::path directoryPath, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static void LoadAllNVDF(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static bool CreateSRV(ID3D12Device* pDevice, ID3D12Resource* pResource, D3D12_SRV_DIMENSION dim, Texture& outTexture);
};

struct MeshFactory final
{
    static MeshID CreateMesh(const char* fileName, const VertexBufferDescription* vertAttr, Mesh& out_meshDX12);
    static void LoadAllMeshes(ResourceCodex& codex);
};

struct MaterialFactory final
{
    static bool CreateAllMaterials(ResourceCodex& codex);
};

}
#endif