#include "Factories.h"

// MeshFactory
#include "Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// ShaderFactory
#include <Core/Shader.h>

// TextureFactory
#include <Core/Material.h>
#include <Core/Texture.h>
#include <filesystem>
#include <DirectXTex.h>

#include <Core/DXCore.h>
#include <Utils/Utils.h>
#include <unordered_map>

namespace Muon
{

	static unsigned int GetVertexSize(const aiMesh& mesh)
	{
		unsigned int sz = 0;
		if (mesh.HasPositions()) sz += sizeof(float) * 3;
		if (mesh.HasNormals()) sz += sizeof(float) * 3;
		if (mesh.HasTextureCoords(0)) sz += sizeof(float) * 2;  // vec2
		if (mesh.HasTangentsAndBitangents())  sz += sizeof(float) * 6;  // vec3 tangent + vec3 bitangent
		if (mesh.HasVertexColors(0)) sz += sizeof(float) * 4;  // vec4
		return sz;
	}

	bool MeshFactory::LoadMesh(const wchar_t* fileName, UploadBuffer& stagingBuffer, Mesh& outMesh)
	{
		using namespace DirectX;
		Assimp::Importer Importer;

		std::string pathStr = Muon::FromWideStr(GetModelPathFromFile_W(fileName));

		// Load assimpScene with proper flags
		const aiScene* pScene = Importer.ReadFile(
			pathStr,
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |   // Remove unnecessary duplicate information
			aiProcess_GenNormals |   // Ensure normals are generated
			aiProcess_CalcTangentSpace          // Needed for normal mapping
		);

		if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
		{
			Muon::Printf("Error parsing '%s': '%s'\n", pathStr.c_str(), Importer.GetErrorString());
			return false;
		}

		struct MeshData
		{
			std::vector<uint8_t> vertexData;
			std::vector<uint32_t> indices;
			uint32_t vertexCount = 0;
			DirectX::XMFLOAT3A min;
			DirectX::XMFLOAT3A max;
		};
		std::vector<MeshData> meshesData;
		meshesData.reserve(pScene->mNumMeshes);

		auto WriteFloat2 = [](uint8_t*& writeHead, const aiVector3D& vec)
			{
				float data[2] = { vec.x, vec.y };
				memcpy(writeHead, data, sizeof(data));
				writeHead += sizeof(data);
			};

		auto WriteFloat3 = [](uint8_t*& writeHead, const aiVector3D& vec)
			{
				float data[3] = { vec.x, vec.y, vec.z };
				memcpy(writeHead, data, sizeof(data));
				writeHead += sizeof(data);
			};

		auto WriteFloat4 = [](uint8_t*& writeHead, const aiColor4D& color)
			{
				float data[4] = { color.r, color.g, color.b, color.a };
				memcpy(writeHead, data, sizeof(data));
				writeHead += sizeof(data);
			};

		// aiScenes may be composed of multiple submeshes,
		for (unsigned int i = 0; i != pScene->mNumMeshes; ++i)
		{
			const aiMesh* pMesh = pScene->mMeshes[i];

			if (!pMesh)
			{
				Muon::Printf("Warning: Model file contained null aiMesh: %s\n", pathStr.c_str());
				continue;
			}

			const unsigned int vertexSize = GetVertexSize(*pMesh);
			const unsigned int numVertices = pMesh->mNumVertices;
			const unsigned int totalVBOSize = numVertices * vertexSize;
			MeshData& meshData = meshesData.emplace_back();
			meshData.vertexData.resize(totalVBOSize);
			meshData.vertexCount = numVertices;

			uint8_t* bufferStart = meshData.vertexData.data();
			uint8_t* writeHead = bufferStart;

			DirectX::XMFLOAT3A max = DirectX::XMFLOAT3A(FLT_MIN, FLT_MIN, FLT_MIN);
			DirectX::XMFLOAT3A min = DirectX::XMFLOAT3A(FLT_MAX, FLT_MAX, FLT_MAX);

			// Process Vertices for this mesh
			for (unsigned int j = 0; j != pMesh->mNumVertices; ++j)
			{
				if (pMesh->HasPositions()) {
					WriteFloat3(writeHead, pMesh->mVertices[j]);
					DirectX::XMFLOAT3A p = DirectX::XMFLOAT3A(pMesh->mVertices[j][0], pMesh->mVertices[j][1], pMesh->mVertices[j][2]);
					min = DirectX::XMFLOAT3A(std::fmin(min.x, p.x), std::fmin(min.y, p.y), std::fmin(min.z, p.z));
					max = DirectX::XMFLOAT3A(std::fmax(max.x, p.x), std::fmax(max.y, p.y), std::fmax(max.z, p.z));
				}
				if (pMesh->HasNormals()) WriteFloat3(writeHead, pMesh->mNormals[j]);
				if (pMesh->HasTextureCoords(0)) WriteFloat2(writeHead, pMesh->mTextureCoords[0][j]);
				if (pMesh->HasTangentsAndBitangents()) { WriteFloat3(writeHead, pMesh->mTangents[j]); WriteFloat3(writeHead, pMesh->mBitangents[j]); }
				if (pMesh->HasVertexColors(0)) WriteFloat4(writeHead, pMesh->mColors[0][j]);
			}

			meshData.min = min;
			meshData.max = max;

			meshData.indices.reserve(pMesh->mNumFaces * 3);

			// Process Indices next
			for (unsigned int j = 0, ind = 0; j < pMesh->mNumFaces; ++j)
			{
				const aiFace& face = pMesh->mFaces[j];

#if defined(MN_DEBUG)
				assert(face.mNumIndices == 3); // Sanity check
#endif

				meshData.indices.push_back(face.mIndices[0]);
				meshData.indices.push_back(face.mIndices[1]);
				meshData.indices.push_back(face.mIndices[2]);
			}
		}
		// TODO: Support scenes with multiple meshes. These meshData's need to be merged.
		MeshData& data = meshesData.at(0);

		AABB boundingBox;
		boundingBox.min = data.min;
		boundingBox.max = data.max;

		bool success = outMesh.Create(fileName, (UINT)data.vertexData.size(), (UINT)data.vertexData.size() / data.vertexCount, (UINT)data.vertexCount, (UINT)data.indices.size() * sizeof(uint32_t), (UINT)data.indices.size(), DXGI_FORMAT_R32_UINT, boundingBox);
		if (!success)
		{
			Muon::Printf(L"Error: Failed to create mesh: %s\n", fileName);
			outMesh.Destroy();
			return false;
		}

		success = stagingBuffer.UploadToMesh(Muon::GetCommandList(), outMesh, data.vertexData.data(), (UINT)data.vertexData.size(), data.indices.data(), (UINT)data.indices.size() * sizeof(uint32_t));
		if (!success)
		{
			Muon::Printf(L"Error: Failed to upload mesh: %s\n", fileName);
			outMesh.Destroy();
			return false;
		}

		return true;
	}

	void MeshFactory::LoadAllMeshes(ResourceCodex& codex)
	{
		namespace fs = std::filesystem;
		std::string modelPath = MODELPATH;

#if defined(MN_DEBUG)
		if (!fs::exists(modelPath))
			throw std::exception("Models folder doesn't exist!");
#endif

		// Iterate through folder and load models
		for (const auto& entry : fs::directory_iterator(modelPath))
		{
			std::wstring& name = entry.path().filename().wstring();
			Mesh temp;
			if (!MeshFactory::LoadMesh(name.c_str(), codex.GetMeshStagingBuffer(), temp))
				continue;

			codex.RegisterMesh(temp);
		}
	}

	void ShaderFactory::LoadAllShaders(ResourceCodex& codex)
	{
		namespace fs = std::filesystem;
		std::string shaderPath = SHADERPATH;

#if defined(MN_DEBUG)
		if (!fs::exists(shaderPath))
			throw std::exception("Shaders folder doesn't exist!");
#endif

		// Iterate through folder and load shaders
		for (const auto& entry : fs::directory_iterator(shaderPath))
		{
			std::wstring path = entry.path();
			std::wstring stem = entry.path().stem().wstring();
			size_t pos = stem.find(L'.') + 1;
			std::wstring ext = stem.substr(pos);

			ResourceID hash = GetResourceID(stem.c_str());

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
		const size_t dataSize = width * height * depth * bytesPerPixel;

		UploadBuffer& stagingBuffer = codex.Get3DTextureStagingBuffer();
		assert(dataSize <= stagingBuffer.GetBufferSize());

		Texture& tex = codex.InsertTexture(GetResourceID(textureName));

		if (!tex.Create(textureName, pDevice, (UINT)width, (UINT)height, (UINT)depth, fmt, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST))
		{
			Muon::Printf(L"Error: Failed to create default heap resource for 3d texture %s!\n", textureName);
			return false;
		}

		if (!stagingBuffer.UploadToTexture(tex, data, pCommandList))
		{
			Muon::Printf(L"Error: Failed to create default heap resource for 3d texture %s!\n", textureName);
			return false;
		}

		if (!tex.InitSRV(pDevice, Muon::GetSRVHeap()))
		{
			Muon::Printf(L"Error: Failed to create create D3D12 Resource and SRV for 3d texture %s!\n", textureName);
			return false;
		}

		return true;
	}

	bool TextureFactory::CreateOffscreenRenderTarget(ID3D12Device* pDevice, UINT width, UINT height)
	{
		DescriptorHeap* pSRVHeap = Muon::GetSRVHeap();
		ID3D12DescriptorHeap* pRTVHeap = Muon::GetRTVHeap();
		DXGI_FORMAT rtvFormat = Muon::GetRTVFormat();

		if (!pSRVHeap || !pRTVHeap)
			return false;

		bool success = true;

		D3D12_CLEAR_VALUE& clearValue = Muon::GetGlobalClearValue();
		clearValue.Format = rtvFormat;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.2f;
		clearValue.Color[2] = 0.4f;
		clearValue.Color[3] = 1.0f;

		const wchar_t* OFFSCREEN_TARGET_NAME = L"OffscreenTarget";
		const wchar_t* COMPUTE_OUTPUT_NAME = L"SobelOutput";

		ResourceCodex& codex = ResourceCodex::GetSingleton();
		Texture& offscreenTarget = codex.InsertTexture(GetResourceID(OFFSCREEN_TARGET_NAME));
		Texture& computeOutput = codex.InsertTexture(GetResourceID(COMPUTE_OUTPUT_NAME));

		success &= offscreenTarget.Create(OFFSCREEN_TARGET_NAME, pDevice, width, height, 1, rtvFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue);
		if (!success)
			return false;

		success &= offscreenTarget.InitSRV(pDevice, pSRVHeap);
		if (!success)
		{
			Printf("Error: Failed to allocate on the SRV heap for offscreen render target!\n");
			return false;
		}

		offscreenTarget.SetRTVHandleCPU(CD3DX12_CPU_DESCRIPTOR_HANDLE(pRTVHeap->GetCPUDescriptorHandleForHeapStart(), Muon::GetSwapChainBufferCount(), Muon::GetRTVSize()));
		pDevice->CreateRenderTargetView(offscreenTarget.GetResource(), nullptr, offscreenTarget.GetRTVHandleCPU());

		/// Sobel Output 
		success &= computeOutput.Create(COMPUTE_OUTPUT_NAME, pDevice, width, height, 1, rtvFormat, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		if (!success)
			return false;

		success &= computeOutput.InitSRV(pDevice, pSRVHeap);
		if (!success)
			return false;

		success &= computeOutput.InitUAV(pDevice, pSRVHeap);
		if (!success)
			return false;

		Muon::SetOffscreenTarget(&offscreenTarget); // taking addr of a reference returns the address of the original object
		return success;
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

		UploadBuffer& stagingBuffer = codex.Get2DTextureStagingBuffer();

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

			ResourceID tid = GetResourceID(name.c_str());
			Texture& tex = codex.InsertTexture(tid);

			DirectX::ScratchImage scratchImg;
			HRESULT hr = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, scratchImg, nullptr);
			if (FAILED(hr))
			{
				Muon::Printf(L"Error: Failed to load texture %s: 0x%08X\n", path.c_str(), hr);
				continue;
			}

			const DirectX::Image* pImage = scratchImg.GetImage(0, 0, 0);
			if (!pImage || !tex.Create(name.c_str(), pDevice, (UINT)pImage->width, (UINT)pImage->height, 1, pImage->format, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST))
			{
				Muon::Printf(L"Error: Failed to create texture on default heap %s: 0x%08X\n", path.c_str(), hr);
				continue;
			}

			if (!stagingBuffer.UploadToTexture(tex, pImage->pixels, pCommandList))
			{
				Muon::Printf(L"Error: Failed to upload data to texture %s: 0x%08X\n", path.c_str(), hr);
				continue;
			}

			if (!tex.InitSRV(pDevice, Muon::GetSRVHeap()))
			{
				Muon::Printf(L"Error: Failed to create D3D12 Resource and SRV for %s!\n", path.c_str());
				continue;
			}
		}
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

			const Image* pImage = image.GetImage(0, 0, 0);
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

	bool MaterialFactory::CreateAllMaterials(ResourceCodex& codex)
	{
		const ResourceID kRockDiffuseId = GetResourceID(L"Rock_T.png");
		const ResourceID kRockNormalId = GetResourceID(L"Rock_N.png");
		const ResourceID kTestNVDFId = GetResourceID(L"StormbirdCloud_NVDF");
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

			pPhongMaterial->SetTextureParam("diffuseTexture", kRockDiffuseId);
			pPhongMaterial->SetTextureParam("normalMap", kRockNormalId);
			pPhongMaterial->SetTextureParam("testNVDF", kTestNVDFId);
		}


		return true;
	}

}