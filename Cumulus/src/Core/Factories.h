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
    static bool CreateOffscreenRenderTarget(ID3D12Device* pDevice, UINT width, UINT height);
    
    static bool LoadTexturesForNVDF(std::filesystem::path directoryPath, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static bool Load3DTextureFromSlices(std::filesystem::path directoryPath, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static void LoadAllNVDF(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
    static void LoadAll3DTextures(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex);
};

struct MeshFactory final
{
    static bool LoadMesh(const wchar_t* fileName, UploadBuffer& stagingBuffer, Mesh& out_meshDX12);
    static void LoadAllMeshes(ResourceCodex& codex);
};

struct MaterialFactory final
{
    static bool CreateAllMaterials(ResourceCodex& codex);
};

}
#endif