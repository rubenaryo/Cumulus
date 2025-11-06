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
#include <DirectXTex.h>

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
    const ShaderID kPhongVSID = fnv1a(L"Phong.vs");
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
        std::wstring stem = entry.path().stem().wstring();
        size_t pos = stem.find(L'.') + 1;
        std::wstring ext = stem.substr(pos);

        ShaderID hash = fnv1a(stem.c_str());

        // Parse file name to decide how to create this resource
        // TODO: This is stupid, maybe just put them all in the same map?
        if (ext == L"vs")
        {
            codex.AddVertexShader(hash, path.c_str());
        }
        else if (ext == L"ps")
        {
            codex.AddPixelShader(hash, path.c_str());
        }
        else if (ext == L"cs")
        {
            codex.AddComputeShader(hash, path.c_str());
        }
    }
}

bool TextureFactory::Upload3DTextureFromData(const wchar_t* textureName, void* data, size_t width, size_t height, size_t depth, DXGI_FORMAT fmt, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex)
{
    const size_t bitsPerPixel = DirectX::BitsPerPixel(fmt);
    const size_t bytesPerPixel = bitsPerPixel / 8;
    const size_t floatsPerPixel = bytesPerPixel / sizeof(float);
    assert(floatsPerPixel == 4); // any size is fine as long as it's 4

    const size_t dataSize = width * height * depth * bytesPerPixel;

    UploadBuffer& stagingBuffer = codex.Get3DTextureStagingBuffer();
    assert(dataSize <= stagingBuffer.GetBufferSize());

    TextureID hash = fnv1a(textureName);
    Texture& tex = codex.InsertTexture(hash);

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    texDesc.Alignment = 0;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = depth;
    texDesc.MipLevels = 1;
    texDesc.Format = fmt;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Create default heap resource
    HRESULT hr = pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&tex.pResource));
    COM_EXCEPT(hr);

    if (FAILED(hr))
    {
        Muon::Printf(L"Error: Failed to create default heap resource for 3d texture %s!\n", textureName);
        return false;
    }

    // schedule a copy through the staging buffer into the main resource
    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = data;
    subresourceData.RowPitch = width * floatsPerPixel * sizeof(float); // bytes per row
    subresourceData.SlicePitch = width * height * floatsPerPixel * sizeof(float); // bytes per slice

    UpdateSubresources<1>(pCommandList, tex.pResource.Get(), stagingBuffer.GetResource(), 0, 0, 1, &subresourceData);
    
    // This barrier transitions the resource state to be srv-ready
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        tex.pResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    pCommandList->ResourceBarrier(1, &barrier);

    return CreateSRV(pDevice, tex.pResource.Get(), D3D12_SRV_DIMENSION_TEXTURE3D, tex);
}

// Loads all the textures from the directory and returns them as out params to the ResourceCodex
void TextureFactory::LoadAllTextures(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex)
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

        if (!CreateSRV(pDevice, tex.pResource.Get(), D3D12_SRV_DIMENSION_TEXTURE2D, tex))
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

// Extracts index from tga file name. ie: field_data.#.tga
size_t extractNumber(const std::filesystem::path& p)
{
    std::string stem = p.stem().string();
    auto dotPos = stem.find_last_of('.');
    if (dotPos == std::string::npos)
        throw std::runtime_error("Invalid filename format: " + stem);
    return std::stoul(stem.substr(dotPos + 1));
}

void TextureFactory::LoadTexturesForNVDF(std::filesystem::path directoryPath, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex)
{
    namespace fs = std::filesystem;

    auto ExtractIndexAndInsert = [](const std::filesystem::path& p, std::vector<fs::path>& outVector)
    {
        size_t number = extractNumber(p);
        if (outVector.size() < number)
            outVector.resize(number);
        outVector[number - 1] = p;
    };

    // Assume the following naming convention: 
    // - field_data.#.tga : red channel = dimensional_profile
    // - modeling_data.#.tga : red = detail_type, green = density_scale, blue = sdf
    // Note: # starts from 1. 
    std::vector<fs::path> fieldFiles;
    std::vector<fs::path> modelingFiles;

    static const size_t INITIAL_CAPACITY = 64;
    fieldFiles.reserve(INITIAL_CAPACITY);
    modelingFiles.reserve(INITIAL_CAPACITY);

    for (const auto& entry : fs::directory_iterator(directoryPath))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".tga") // Only accept tga files
            continue;

        std::wstring name = entry.path().filename().wstring();
        if (name.rfind(L"field_data.", 0) == 0)
        {
            ExtractIndexAndInsert(entry.path(), fieldFiles);
        }
        else if (name.rfind(L"modeling_data.", 0) == 0)
        {
            ExtractIndexAndInsert(entry.path(), modelingFiles);
        }
    }

    if (fieldFiles.empty())
        return;

    using namespace DirectX;

    size_t width = 0;
    size_t height = 0;
    size_t depth = fieldFiles.size();
    std::vector<float> outData;

    static const size_t CHANNELS_PER_VOXEL = 4;

    // Use the first image to find ideal dimensions ( Should always be 512 x 512 )
    {
        ScratchImage image;
        HRESULT hr = LoadFromTGAFile(fieldFiles.at(0).c_str(), nullptr, image);
        COM_EXCEPT(hr);
        if (FAILED(hr))
            return;
        
        const Image* pImage = image.GetImage(0,0,0);
        if (!pImage)
            return;

        width = pImage->width;
        height = pImage->height;

        outData.resize(width * height * depth * CHANNELS_PER_VOXEL);
    }

    for (size_t i = 0; i != depth; ++i)
    {
        fs::path& fieldFile = fieldFiles.at(i);
        fs::path& modelFile = modelingFiles.at(i);
        
        ScratchImage fieldImg, modelImg;

        if (FAILED(LoadFromTGAFile(fieldFile.c_str(), nullptr, fieldImg)))
        {
            Printf(L"Error: Failed to load %s\n", fieldFile.filename().wstring().c_str());
            return;
        }

        if (FAILED(LoadFromTGAFile(modelFile.c_str(), nullptr, modelImg)))
        {
            Printf(L"Error: Failed to load %s\n", modelFile.filename().wstring().c_str());
            return;
        }

        const Image* fImg = fieldImg.GetImage(0, 0, 0);
        const Image* mImg = modelImg.GetImage(0, 0, 0);

        if (fImg->width != mImg->width || fImg->height != mImg->height)
        {
            Printf(L"Error: Slice Dimensions Mismatch. %s and %s\n", 
                fieldFile.filename().wstring().c_str(), 
                modelFile.filename().wstring().c_str());
            return;
        }

        DXGI_FORMAT fFmt = fImg->format;
        DXGI_FORMAT mFmt = mImg->format;

        size_t fBits = DirectX::BitsPerPixel(fFmt);
        size_t mBits = DirectX::BitsPerPixel(mFmt);

        size_t fBytes = fBits / 8;
        size_t mBytes = mBits / 8;

        if (mBytes < 4)
        {
            Printf(L"Error: File %s has an unexpected number of bytes per pixel! %ull\n",
                modelFile.filename().wstring().c_str(), mBytes);
            return;
        }

        size_t sliceOffset = i * width * height * CHANNELS_PER_VOXEL;

        const uint8_t* fPixels = fImg->pixels;
        const uint8_t* mPixels = mImg->pixels;

        for (size_t j = 0; j < width * height; ++j)
        {
            size_t baseIndex = sliceOffset + j * CHANNELS_PER_VOXEL;

            // account for TGA padding
            size_t x = j % width;
            size_t y = j / width;
            float fR = fPixels[y * fImg->rowPitch + x * fBytes] / 255.0f;
            float mR = mPixels[y * mImg->rowPitch + x * mBytes + 0] / 255.0f;
            float mG = mPixels[y * mImg->rowPitch + x * mBytes + 1] / 255.0f;
            float mB = mPixels[y * mImg->rowPitch + x * mBytes + 2] / 255.0f;

            outData[baseIndex + 0] = fR;
            outData[baseIndex + 1] = mR;
            outData[baseIndex + 2] = mG;
            outData[baseIndex + 3] = mB;
        }
    }

    std::wstring lookupName = directoryPath.filename().c_str();
    lookupName += L"_NVDF";

    Upload3DTextureFromData(lookupName.c_str(), outData.data(),
        width, height, depth, DXGI_FORMAT_R32G32B32A32_FLOAT, 
        pDevice, pCommandList, codex);
}

void TextureFactory::LoadAllNVDF(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ResourceCodex& codex)
{
    namespace fs = std::filesystem;
    std::wstring nvdfPath = NVDFPATHW;

#if defined(MN_DEBUG)
    if (!fs::exists(nvdfPath))
        throw std::exception("NVDF folder doesn't exist!");
#endif

    // Each directory represents a separate NVDF, consisting of loose layers packed as tga files.
    // Assume the following naming convention: 
    // - field_data.#.tga : red channel = dimensional_profile
    // - modeling_data.#.tga : red = detail_type, green = density_scale, blue = sdf

    for (const auto& entry : fs::directory_iterator(nvdfPath))
    {
        if (!entry.is_directory())
            continue;

        LoadTexturesForNVDF(entry.path(), pDevice, pCommandList, codex);
    }
}

bool TextureFactory::CreateSRV(ID3D12Device* pDevice, ID3D12Resource* pResource, D3D12_SRV_DIMENSION dim, Texture& outTexture)
{
    if (!pResource)
        return false;

    // Allocate descriptor
    DescriptorHeap* pSRVHeap = Muon::GetSRVHeap();
    if (!pSRVHeap || !pSRVHeap->Allocate(outTexture.CPUHandle, outTexture.GPUHandle))
        return false;

    D3D12_RESOURCE_DESC resourceDesc = pResource->GetDesc();
    outTexture.Width = static_cast<UINT>(resourceDesc.Width);
    outTexture.Height = resourceDesc.Height;
    outTexture.Depth = resourceDesc.DepthOrArraySize;
    outTexture.Format = resourceDesc.Format;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = resourceDesc.Format;
    srvDesc.ViewDimension = dim;
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    pDevice->CreateShaderResourceView(pResource, &srvDesc, outTexture.CPUHandle);

    return true;
}

bool MaterialFactory::CreateAllMaterials(ResourceCodex& codex)
{
    const TextureID kRockDiffuseId = fnv1a(L"Rock_T.png");
    const TextureID kRockNormalId = fnv1a(L"Rock_N.png");
    const TextureID kTestNVDFId = fnv1a(L"StormbirdCloud_NVDF");
    {
        const wchar_t* kPhongMaterialName = L"Phong";
        Material* pPhongMaterial = codex.InsertMaterialType(kPhongMaterialName);
        if (!pPhongMaterial)
        {
            Muon::Printf(L"Warning: %s Material failed to be inserted into codex!", kPhongMaterialName);
            return false;
        }

        cbMaterialParams phongMaterialParams;
        phongMaterialParams.colorTint = DirectX::XMFLOAT4(1, 1, 1, 1);
        phongMaterialParams.specularExp = 32.0f;

        pPhongMaterial->PopulateMaterialParams(codex.GetMatParamsStagingBuffer(), Muon::GetCommandList());

        pPhongMaterial->SetTextureParam("diffuseTexture",   kRockDiffuseId);
        pPhongMaterial->SetTextureParam("normalMap",        kRockNormalId);
        pPhongMaterial->SetTextureParam("testNVDF",         kTestNVDFId);
    }


    return true;
}

}