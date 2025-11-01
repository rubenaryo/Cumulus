#include "Factories.h"

#include "hash_util.h"

// MeshFactory
#include "Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// ShaderFactory
#include "Shader.h"

// TextureFactory
#include "Material.h"
#include <filesystem>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>

#include <Core/DXCore.h>
#include <Utils/Utils.h>
#include <unordered_map>

namespace Muon
{

MeshID MeshFactory::CreateMesh(const char* fileName, const VertexBufferDescription* vertAttr, Mesh& out_mesh)
{
    Assimp::Importer Importer;
    MeshID meshId = fnv1a(fileName);

    // Load assimpScene with proper flags
    const aiScene* pScene = Importer.ReadFile(
        GetModelPathFromFile(fileName),
        aiProcess_Triangulate           |
        aiProcess_JoinIdenticalVertices |   // Remove unnecessary duplicate information
        aiProcess_GenNormals            |   // Ensure normals are generated
        aiProcess_CalcTangentSpace          // Needed for normal mapping
    );

    const VertexBufferDescription vertDesc = *vertAttr;

    if (pScene)
    {
        using namespace DirectX;

        // aiScenes may be composed of multiple submeshes,
        // we want to coagulate this into a single vertex/index buffer
        for (unsigned int i = 0; i != pScene->mNumMeshes; ++i)
        {
            const aiMesh* pMesh = pScene->mMeshes[i];
            const aiVector3D c_Zero(0.0f, 0.0f, 0.0f);

            const UINT numVertices = pMesh->mNumVertices;
            BYTE* vertices = (BYTE*)malloc(vertDesc.ByteSize * numVertices);

            const unsigned int numIndices = pMesh->mNumFaces * 3;
            uint32_t* indices  = (uint32_t*)malloc(sizeof(uint32_t) * numIndices);

            // Process Vertices for this mesh
            for (unsigned int j = 0; j != pMesh->mNumVertices; ++j)
            {
                // Assign needed vertex attributes
                for (unsigned int k = 0; k != vertDesc.AttrCount; ++k)
                {
                    const unsigned int currByteOffset = vertDesc.ByteOffsets[k];
                    const unsigned int nextByteOffset = (k+1) != vertDesc.AttrCount ? vertDesc.ByteOffsets[k+1] : vertDesc.ByteSize;
                    const unsigned int numComponents = (nextByteOffset - currByteOffset) / sizeof(float);
                    BYTE* copyLocation = (vertices + j*vertDesc.ByteSize) + currByteOffset;

                    switch (vertDesc.SemanticsArr[k])
                    {
                        case Semantics::POSITION:
                            assert(pMesh->HasPositions());
                            memcpy(copyLocation, &(pMesh->mVertices[j]), sizeof(float) * numComponents);
                            break;
                        case Semantics::NORMAL:
                            assert(pMesh->HasNormals());
                            memcpy(copyLocation, &(pMesh->mNormals[j]), sizeof(float) * numComponents);
                            break;
                        case Semantics::TEXCOORD:
                            assert(pMesh->HasTextureCoords(0));
                            memcpy(copyLocation, &(pMesh->mTextureCoords[0][j]), sizeof(float) * numComponents);
                            break;
                        case Semantics::TANGENT:
                            assert(pMesh->HasTangentsAndBitangents());
                            memcpy(copyLocation, &(pMesh->mTangents[j]), sizeof(float) * numComponents);
                            break;
                        case Semantics::BINORMAL:
                            assert(pMesh->HasTangentsAndBitangents());
                            memcpy(copyLocation, &(pMesh->mBitangents[j]), sizeof(float) * numComponents);
                            break;
                        case Semantics::COLOR: // Lacks testing
                            assert(pMesh->HasVertexColors(0));
                            memcpy(copyLocation, &(pMesh->mColors[0][j]), sizeof(float) * numComponents);
                            break;
                        #if defined(MN_DEBUG)
                        default:
                            OutputDebugStringA("INFO: Unhandled Vertex Shader Input Semantic when parsing Mesh vertices\n");
                        #endif
                    }
                }
            }

            // Process Indices next
            for (unsigned int j = 0, ind = 0; j < pMesh->mNumFaces; ++j)
            {
                const aiFace& face = pMesh->mFaces[j];

                #if defined(MN_DEBUG)
                    assert(face.mNumIndices == 3); // Sanity check
                #endif

                // All the indices of this face are valid, add to list
                indices[ind++] = face.mIndices[0];
                indices[ind++] = face.mIndices[1];
                indices[ind++] = face.mIndices[2];
            }
            
            bool success = out_mesh.Init(reinterpret_cast<void*>(vertices), vertDesc.ByteSize * numVertices, vertDesc.ByteSize, reinterpret_cast<void*>(indices), sizeof(unsigned int) * numIndices, numIndices, DXGI_FORMAT_R32_UINT);
            if (!success)
                Muon::Print("Failed to init mesh!\n");

            free(vertices);
            free(indices);
        }
    }
    #if defined(MN_DEBUG)
    else
    {
        char buf[256];
        sprintf_s(buf, "Error parsing '%s': '%s'\n", fileName, Importer.GetErrorString());
        throw std::exception(buf);
        return 0;
    }

    std::string vbName;
    vbName.append(fileName);
    vbName.append("_VertexBuffer");
        
    HRESULT hr = out_mesh.VertexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)vbName.size(), vbName.c_str());
    COM_EXCEPT(hr);

    if (out_mesh.IndexBuffer)
    {
        std::string ibName;
        ibName.append(fileName);
        ibName.append("_IndexBuffer");

        hr = out_mesh.IndexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)ibName.size(), ibName.c_str());
        COM_EXCEPT(hr);
    }
    #endif
    return meshId;
}

void MeshFactory::LoadAllMeshes(ResourceCodex& codex)
{
    namespace fs = std::filesystem;
    std::string modelPath = MODELPATH;

#if defined(MN_DEBUG)
    if (!fs::exists(modelPath))
        throw std::exception("Models folder doesn't exist!");
#endif

    // PhongVS will be comprehensive enough for now..
    const ShaderID kPhongVSID = fnv1a(L"PhongVS.cso");
    const VertexShader* pVS = codex.GetVertexShader(kPhongVSID);
    if (!pVS)
        return;

    const VertexBufferDescription* pVertDesc = &(pVS->VertexDesc);

    // Iterate through folder and load models
    for (const auto& entry : fs::directory_iterator(modelPath))
    {
        std::string& name = entry.path().filename().generic_string();
        codex.AddMeshFromFile(name.c_str(), pVertDesc);
    }
}

void ShaderFactory::LoadAllShaders(ResourceCodex& codex)
{
    namespace fs = std::filesystem;
    std::string shaderPath = SHADERPATH;

    #if defined(MN_DEBUG)
    if(!fs::exists(shaderPath))
        throw std::exception("Shaders folder doesn't exist!");
    #endif

    // Iterate through folder and load shaders
    for (const auto& entry : fs::directory_iterator(shaderPath))
    {
        std::wstring path = entry.path();
        std::wstring name = entry.path().filename();

        ShaderID hash = fnv1a(name.c_str());

        // Parse file name to decide how to create this resource
        if (name.find(L"VS") != std::wstring::npos)
        {
            codex.AddVertexShader(hash, path.c_str());
        }
        else if (name.find(L"PS") != std::wstring::npos)
        {
            codex.AddPixelShader(hash, path.c_str());
        }
    }
}

// Loads all the textures from the directory and returns them as out params to the ResourceCodex
void TextureFactory::LoadAllTextures(ID3D12Device* pDevice, ID3D12CommandList* pCommandList, ResourceCodex& codex)
{
    namespace fs = std::filesystem;
    std::string texturePath = TEXTUREPATH;

#if defined(MN_DEBUG)
    if (!fs::exists(texturePath))
        throw std::exception("Textures folder doesn't exist!");
#endif

    // NOTE: This has a known memory leak in the DXTK
    DirectX::ResourceUploadBatch* pResourceUpload = new DirectX::ResourceUploadBatch(pDevice);

    pResourceUpload->Begin();

    for (const auto& entry : fs::directory_iterator(texturePath))
    {
        std::wstring path = entry.path().c_str();
        std::wstring name = entry.path().filename().c_str();

        size_t pos = name.find(L'_');
        const std::wstring TexName = name.substr(0, pos++);
        const std::wstring TexType = name.substr(pos, 1);

        pos = name.find(L'.') + 1;
        const std::wstring TexExt = name.substr(pos);

        if (TexExt != L"png")
            continue;

        TextureID tid = fnv1a(name.c_str());
        Texture& tex = codex.InsertTexture(tid);

        HRESULT hr = DirectX::CreateWICTextureFromFile(
            pDevice,
            *pResourceUpload,
            path.c_str(),
            tex.pResource.GetAddressOf()
        );

        if (FAILED(hr))
        {
            Muon::Printf(L"Error: Failed to load texture %s: 0x%08X\n", path.c_str(), hr);
            continue;
        }

        if (!CreateSRV(codex.GetSRVDescriptorHeap(), pDevice, tex.pResource.Get(), tex))
        {
            Muon::Printf(L"Error: Failed to create D3D12 Resource and SRV for %s!\n", path.c_str());
            continue;
        }
    }

    auto uploadResourcesFinished = pResourceUpload->End(Muon::GetCommandQueue());
    uploadResourcesFinished.wait();

    delete pResourceUpload;
    FlushCommandQueue();
}

bool TextureFactory::CreateSRV(DescriptorHeap& descHeap, ID3D12Device* pDevice, ID3D12Resource* pResource, Texture& outTexture)
{
    if (!pResource)
        return false;

    // Allocate descriptor
    if (!descHeap.Allocate(outTexture.CPUHandle, outTexture.GPUHandle))
        return false;

    D3D12_RESOURCE_DESC resourceDesc = pResource->GetDesc();
    outTexture.Width = static_cast<UINT>(resourceDesc.Width);
    outTexture.Height = resourceDesc.Height;
    outTexture.Format = resourceDesc.Format;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = resourceDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    pDevice->CreateShaderResourceView(pResource, &srvDesc, outTexture.CPUHandle);

    return true;
}

bool MaterialFactory::CreateAllMaterials(ResourceCodex& codex)
{
    const TextureID kRockDiffuseId = fnv1a(L"Rock_T.png");       // FNV1A of L"Lunar_T"
    const TextureID kRockNormalId = fnv1a(L"Rock_N.png");       // FNV1A of L"Lunar_T"

    const ShaderID kPhongVSID = fnv1a("PhongVS.cso");
    const ShaderID kPhongPSID = 0x4dc6e249;          // FNV1A of L"PhongPS.cso"
    const ShaderID kPhongPSNormalMapID = fnv1a(L"Phong_NormalMapPS.cso");
    const MeshID kSkyMeshID = 0x4a986f37; // cube

    const VertexShader* pPhongVS = codex.GetVertexShader(kPhongVSID);
    const PixelShader* pPhongPS = codex.GetPixelShader(kPhongPSID);

    if (!pPhongVS || !pPhongPS)
    {
        Muon::Print("Error: Failed to fetch Phong VS/PS from codex!");
        return false;
    }

    // Test MaterialType
    {
        const wchar_t* kPhongMaterialName = L"Phong";
        MaterialType* pPhongMaterial = codex.InsertMaterialType(kPhongMaterialName);
        if (!pPhongMaterial)
        {
            Muon::Printf(L"Warning: %s MaterialType failed to be inserted into codex!", kPhongMaterialName);
            return false;
        }

        //pbr->SetRootSignature(Muon::GetRootSignature());
        pPhongMaterial->SetVertexShader(pPhongVS);
        pPhongMaterial->SetPixelShader(pPhongPS);

        if (!pPhongMaterial->Generate())
        {
            Muon::Printf(L"Warning: %s MaterialType failed to Generate()!", pPhongMaterial->GetName().c_str());
            return false;
        }

        cbMaterialParams phongMaterialParams;
        phongMaterialParams.colorTint = DirectX::XMFLOAT4(1, 1, 1, 1);
        phongMaterialParams.specularExp = 32.0f;

        pPhongMaterial->PopulateMaterialParams(codex.GetMatParamsStagingBuffer(), Muon::GetCommandList());

        pPhongMaterial->SetTextureParam("diffuseTexture",   kRockDiffuseId);
        pPhongMaterial->SetTextureParam("normalMap",        kRockNormalId);
    }


    return true;
}

}