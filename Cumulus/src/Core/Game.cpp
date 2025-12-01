/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/12
Description : Implementation of Game.h
----------------------------------------------*/
#include "Game.h"

#include <Core/DXCore.h>
#include <Input/GameInput.h>

#include <Core/Camera.h>
#include <Core/COMException.h>
#include <Core/Factories.h>
#include <Core/MuonImgui.h>
#include <Core/ResourceCodex.h>
#include <Core/Shader.h>
#include <Core/Texture.h>
#include <Utils/AtmosphereUtils.h>
#include <Utils/CloudGenerationUtils.h>
#include <Utils/Utils.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

Game::Game() :
    mInput(),
    mCamera(),
    mTerrainPass(L"TerrainPass"),
    mOpaquePass(L"OpaquePass"),
    mAtmospherePass(L"AtmospherePass"),
    mSobelPass(L"SobelPass"),
    mRaymarchPass(L"RaymarchPass"),
    mProcNVDFPass(L"ProcNVDFPass"),
    mPostProcessPass(L"PostProcessPass")
{
    mTimer.SetFixedTimeStep(false);
}

bool Game::Init(HWND window, int width, int height)
{
    using namespace Muon;

    bool success = Muon::InitDX12(window, width, height);

    success &= Muon::ImguiInit();
    if (!success)
    {
        Print(L"Error: ImguiInit() failed!\n");
        return false;
    }

    ResourceCodex::Init();

    Muon::ResetCommandList(nullptr);
    // TODO: Create the offscreen render target externally, but register it in the codex so it can manage its lifetime. 
    TextureFactory::CreateOffscreenRenderTarget(Muon::GetDevice(), width, height);

    ResourceCodex& codex = ResourceCodex::GetSingleton();

    mCamera.Init(DirectX::XMFLOAT3(500.0, 300.0, 100.0), width / (float)height, 0.1f, 1000.0f);

    // Assemble opaque terrain pass
    {
        mTerrainPass.SetVertexShader(codex.GetVertexShader(GetResourceID(L"Terrain.vs")));
        mTerrainPass.SetPixelShader(codex.GetPixelShader(GetResourceID(L"Terrain.ps")));
        mTerrainPass.SetEnableDepth(false);

        if (!mTerrainPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mTerrainPass.GetName());
    }

    // Assemble opaque render pass
    {
        mOpaquePass.SetVertexShader(codex.GetVertexShader(GetResourceID(L"Phong.vs")));
        mOpaquePass.SetPixelShader(codex.GetPixelShader(GetResourceID(L"Phong_NormalMap.ps")));
        mOpaquePass.SetEnableDepth(true);

        if (!mOpaquePass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mOpaquePass.GetName());
    }

    // Assemble atmosphere render pass
    {
        mAtmospherePass.SetVertexShader(codex.GetVertexShader(GetResourceID(L"atmosphere.vs")));
        mAtmospherePass.SetPixelShader(codex.GetPixelShader(GetResourceID(L"atmosphere.ps")));
        mAtmospherePass.SetEnableDepth(false);

        if (!mAtmospherePass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mAtmospherePass.GetName());
    }

    // Assemble compute filter pass
    {
        mSobelPass.SetComputeShader(codex.GetComputeShader(GetResourceID(L"Sobel.cs")));

        if (!mSobelPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mSobelPass.GetName());
    }

    // Assemble raymarch pass
    {
        mRaymarchPass.SetComputeShader(codex.GetComputeShader(GetResourceID(L"Raymarch.cs")));

        if (!mRaymarchPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mRaymarchPass.GetName());
    }

    // Assemble procedural nvdf pass
    {
        mProcNVDFPass.SetComputeShader(codex.GetComputeShader(GetResourceID(L"UpdateNVDF.cs")));

        if (!mProcNVDFPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mProcNVDFPass.GetName());
    }

    // Assemble post-process render pass
    {
        mPostProcessPass.SetVertexShader(codex.GetVertexShader(GetResourceID(L"Passthrough.vs")));
        mPostProcessPass.SetPixelShader(codex.GetPixelShader(GetResourceID(L"Passthrough.ps")));
        mPostProcessPass.SetEnableDepth(false);

        if (!mPostProcessPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mPostProcessPass.GetName());
    }

    InitFrameResources(width, height);

    Muon::CloseCommandList();
    Muon::ExecuteCommandList();
    return success;
}

bool Game::InitFrameResources(UINT width, UINT height)
{
    using namespace Muon;

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    
    // Create static data to give to all frame resources

    // Updating Atmosphere
    cbAtmosphere atmosphereParams;
    InitializeAtmosphereConstants(atmosphereParams, width, height);

    // Updating Clouds
    GenerateCloudGenConstants(mCloudData, settings.numClouds, settings.cloudScale);
    
    // Updating AABBs
    const Mesh* m = codex.GetMesh(GetResourceID(L"teapot.obj"));
    cbIntersections intersections = {};
    intersections.aabbCount = 1;
    intersections.aabbs[0] = m->GetAABB();

    // Updating Hull Faces
    //todo: concat all hulls
    Hull h = m->GetHull();
    cbHullFaces faces = {};
    for (size_t i = 0; i < h.faces.size(); i++)
    {
        faces.faces[i] = DirectX::XMFLOAT4(
            h.faces[i].normal.x,
            h.faces[i].normal.y,
            h.faces[i].normal.z,
            h.faces[i].distance
        );
    }

    // Initialize teapot's hull
    cbConvexHull cHull = {};
    cHull.faceCount = (uint32_t)h.faces.size();
    cHull.faceOffset = 0;

    mEntityCBData[0].hull = cHull;
    mEntityCBData[0].entityMatrices.world = DirectX::XMFLOAT4X4(); // These get updated every frame anyway
    mEntityCBData[0].entityMatrices.invWorld = DirectX::XMFLOAT4X4();

    // Create each frame resource and fill it with static data.
    for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
    {
        FrameResources& frameResource = mFrameResources.at(i);
        frameResource.Create(width, height);
        
        frameResource.UpdateAtmosphere(atmosphereParams);
        frameResource.UpdateCloudData(mCloudData);
        frameResource.UpdateAABB(intersections);
        frameResource.UpdateHullFaces(faces);
    }

    return true;
}

// On Timer tick, run Update() on the game, then Render()
void Game::Frame()
{
    WaitForCurrFrameResource();

    mTimer.Tick([&]()
    {
        Update(mTimer);
    });

    UpdateProceduralNVDF();

    Render();
    AdvanceFence();

    mCurrFrameResourceIdx = (mCurrFrameResourceIdx + 1) % NUM_FRAMES_IN_FLIGHT;
    Muon::UpdateTitleBar(mTimer.GetFramesPerSecond(), mTimer.GetFrameCount());
}

void Game::WaitForCurrFrameResource()
{
    Muon::FrameResources& currFrameResources = mFrameResources.at(mCurrFrameResourceIdx);
    ID3D12Fence* pFence = Muon::GetFence();
    if (currFrameResources.mFence != 0 && pFence->GetCompletedValue() < currFrameResources.mFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        HRESULT hr = pFence->SetEventOnCompletion(currFrameResources.mFence, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void Game::AdvanceFence()
{
    Muon::FrameResources& currFrameResources = mFrameResources.at(mCurrFrameResourceIdx);

    // Advance the fence value to mark commands up to this fence point.
    currFrameResources.mFence = Muon::AdvanceFence();
}

void Game::Update(Muon::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());
    float totalTime = float(timer.GetTotalSeconds());
    mInput.Frame(elapsedTime, &mCamera);
    mCamera.UpdateView();    

    // The UI has flagged for a cloud update
    if (settings.updateClouds)
    {
        // Mark the update on each frame resource. They will consume it when they next run. 
        for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
        {
            Muon::FrameResources& frameResource = mFrameResources.at(i);
            frameResource.mNeedsCloudUpdate = true;
        }
        
        Muon::GenerateCloudGenConstants(mCloudData, settings.numClouds, settings.cloudScale);
        settings.updateClouds = false;
    }

    Muon::FrameResources& currFrameResources = mFrameResources.at(mCurrFrameResourceIdx);
    
    // Updating Lights
    Muon::cbLights lights;
    lights.ambientColor = DirectX::XMFLOAT3A(+1.0f, +0.772f, +0.56f);
    lights.directionalLight.diffuseColor = DirectX::XMFLOAT3A(1.0, 1.0, 1.0);
    lights.directionalLight.dir = DirectX::XMFLOAT3A(0, 1, 0);
    currFrameResources.UpdateLights(lights);

    // Updating Time
    Muon::cbTime time;
    time.totalTime = totalTime;
    time.deltaTime = elapsedTime;
    currFrameResources.UpdateTime(time);

    // Updating Atmosphere
    Muon::cbAtmosphere atmosphere;
    Muon::UpdateAtmosphere(atmosphere, mCamera, settings.isSunDynamic, settings.timeOfDay, time.totalTime);
    settings.sunDir = atmosphere.sun_direction;
    currFrameResources.UpdateAtmosphere(atmosphere);

    // Updating Cloud Data
    if (currFrameResources.mNeedsCloudUpdate)
    {
        currFrameResources.UpdateCloudData(mCloudData);
        currFrameResources.mNeedsCloudUpdate = false;
    }

    // Updating Entities
    const float PI = 3.14159f;
    DirectX::XMMATRIX debugEntityWorld = DirectX::XMMatrixIdentity();
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixRotationRollPitchYaw(0, 0, PI / 2.0f));
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixRotationRollPitchYaw(-PI / 2.0f, 0, 0));
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixScaling(10.f, 10.f, 10.f));
    float yPos = 1000 * (sin(time.totalTime * .5f));
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixTranslation(0, yPos, 0));

    Muon::cbPerEntity& entity = mEntityCBData[0].entityMatrices;
    XMStoreFloat4x4(&entity.world, debugEntityWorld);
    XMStoreFloat4x4(&entity.invWorld, DirectX::XMMatrixInverse(nullptr, debugEntityWorld));
    currFrameResources.UpdateEntities(entity);

    // Updating Hulls
    Muon::cbConvexHull cHull = mEntityCBData[0].hull;
    cHull.world = entity.world;
    cHull.invWorld = entity.invWorld;

    Muon::cbHulls hulls = {};
    hulls.hulls[0] = cHull;
    hulls.hullCount = 1;
    currFrameResources.UpdateHulls(hulls);
}

void Game::UpdateProceduralNVDF()
{
    using namespace Muon;

    ID3D12GraphicsCommandList* pCommandList = Muon::GetCommandList();
    FrameResources& currFrameResources = mFrameResources.at(mCurrFrameResourceIdx);

    ResetCommandList(nullptr);
    pCommandList->SetDescriptorHeaps(1, GetSRVHeap()->GetHeapAddr());

    if (!mProcNVDFPass.Bind(pCommandList))
    {
        Muon::Printf("Failed to bind procedural nvdf pass\n");
        return;
    }

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    Texture* pProcNVDFTex = codex.GetTexture(GetResourceID(L"ProceduralNVDF"));
    if (!pProcNVDFTex)
    {
        Muon::Printf("Error: Failed to get procedural nvdf texture!\n");
        return;
    }

    int32_t outputIdx = mProcNVDFPass.GetResourceRootIndex("gOutput");
    if (outputIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootDescriptorTable(outputIdx, pProcNVDFTex->GetUAVHandleGPU());
    }

    int32_t hullIdx = mProcNVDFPass.GetResourceRootIndex("HullsBuffer");
    if (hullIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(hullIdx, currFrameResources.mHullBuffer.GetGPUVirtualAddress());
    }

    int32_t hullFaceIdx = mProcNVDFPass.GetResourceRootIndex("HullFacesBuffer");
    if (hullFaceIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(hullFaceIdx, currFrameResources.mHullFaceBuffer.GetGPUVirtualAddress());
    }

    int32_t cloudIdx = mProcNVDFPass.GetResourceRootIndex("cbCloudGenBuffer");
    if (cloudIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(cloudIdx, currFrameResources.mCloudGenBuffer.GetGPUVirtualAddress());
    }


    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pProcNVDFTex->GetResource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    UINT numGroupsX = (UINT)ceilf(pProcNVDFTex->GetWidth() / 16.0f);
    UINT numGroupsY = (UINT)ceilf(pProcNVDFTex->GetHeight() / 16.0f);
    UINT numGroupsZ = (UINT)ceilf(pProcNVDFTex->GetDepth() / 2.0f);
    pCommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);

    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pProcNVDFTex->GetResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));

    CloseCommandList();
    ExecuteCommandList();
    FlushCommandQueue(); // TODO: Give this process its own cmd allocator so we don't stall everything here
}

void Game::Render()
{
    using namespace Muon;

    // Don't try to render anything before the first Update.
    if (mTimer.GetFrameCount() == 0)
    {
        return;
    }

    FrameResources& currFrameResources = mFrameResources.at(mCurrFrameResourceIdx);
    ResetCommandList(currFrameResources.mCmdAllocator.Get());
    PrepareForRender();

    ImguiNewFrame(mTimer.GetTotalSeconds(), mCamera, settings);

    // Fetch the desired material from the codex
    ResourceCodex& codex = ResourceCodex::GetSingleton();
    ResourceID phongMatId = GetResourceID(L"Phong");
    const Muon::Material* pPhongMaterial = codex.GetMaterialType(phongMatId);

    
    Texture* pOffscreenTarget = codex.GetTexture(GetResourceID(L"OffscreenTarget"));
    Texture* pComputeOutput = codex.GetTexture(GetResourceID(L"SobelOutput"));
    if (!pOffscreenTarget || !pComputeOutput)
    {
        Muon::Printf("Error: Game::Render Failed to fetch the offscreen target and compute output textures.\n");
        return;
    }

    ID3D12GraphicsCommandList* pCommandList = GetCommandList();
    pCommandList->SetDescriptorHeaps(1, GetSRVHeap()->GetHeapAddr());

    if (mAtmospherePass.Bind(pCommandList))
    {
        const Texture* pTransmittanceTex = codex.GetTexture(GetResourceID(L"transmittance_high.hdr"));
        const Texture* pIrradianceTex = codex.GetTexture(GetResourceID(L"irradiance_high.hdr"));
        const Texture* pScatteringTex = codex.GetTexture(GetResourceID(L"TestHDR_3D"));// L"scatter_tex_full.dds"));    // .dds seems to work the same

        int32_t cameraRootIdx = mAtmospherePass.GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            mCamera.Bind(cameraRootIdx, pCommandList);
        }

        int32_t atmosphereRootIdx = mAtmospherePass.GetResourceRootIndex("cbAtmosphere");
        if (atmosphereRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(atmosphereRootIdx, currFrameResources.mAtmosphereBuffer.GetGPUVirtualAddress());
        }

        int32_t transmittanceIdx = mAtmospherePass.GetResourceRootIndex("transmittance_texture");
        if (pTransmittanceTex && transmittanceIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootDescriptorTable(transmittanceIdx, pTransmittanceTex->GetSRVHandleGPU());
        }

        int32_t irradianceIdx = mAtmospherePass.GetResourceRootIndex("irradiance_texture");
        if (pIrradianceTex && irradianceIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootDescriptorTable(irradianceIdx, pIrradianceTex->GetSRVHandleGPU());
        }

        int32_t scatterIdx = mAtmospherePass.GetResourceRootIndex("scattering_texture");
        if (pScatteringTex && scatterIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootDescriptorTable(scatterIdx, pScatteringTex->GetSRVHandleGPU());
        }

        // Draw fullscreen quad
        pCommandList->IASetVertexBuffers(0, 1, nullptr);
        pCommandList->IASetIndexBuffer(nullptr);
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->DrawInstanced(6, 1, 0, 0);
    }

    if (mTerrainPass.Bind(pCommandList))
    {
        // Bind's the materials parameter buffer and textures.
        mTerrainPass.BindMaterial(*pPhongMaterial, pCommandList);

        // Bind the Camera's Upload Buffer to the root index known by the material
        int32_t cameraRootIdx = mTerrainPass.GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            mCamera.Bind(cameraRootIdx, pCommandList);
        }

        // Bind the world matrix Upload Buffer to the root index known by the material
        // TODO: Just uses the same world matrix as the teapot for now.
        int32_t worldMatrixRootIdx = mTerrainPass.GetResourceRootIndex("VSWorld");
        if (worldMatrixRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(worldMatrixRootIdx, currFrameResources.mWorldMatrixBuffer.GetGPUVirtualAddress());
        }

        int32_t lightsRootIdx = mTerrainPass.GetResourceRootIndex("PSLights");
        if (lightsRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(lightsRootIdx, currFrameResources.mLightBuffer.GetGPUVirtualAddress());
        }

        const Mesh* pMesh = codex.GetMesh(GetResourceID(L"TerrainPlane"));
        if (pMesh)
        {
            pMesh->DrawIndexed(pCommandList);
        }
    }

    if (mOpaquePass.Bind(pCommandList))
    {
        // Bind's the materials parameter buffer and textures.
        mOpaquePass.BindMaterial(*pPhongMaterial, pCommandList);

        // Bind the Camera's Upload Buffer to the root index known by the material
        int32_t cameraRootIdx = mOpaquePass.GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            mCamera.Bind(cameraRootIdx, pCommandList);
        }

        // Bind the world matrix Upload Buffer to the root index known by the material
        int32_t worldMatrixRootIdx = mOpaquePass.GetResourceRootIndex("VSWorld");
        if (worldMatrixRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(worldMatrixRootIdx, currFrameResources.mWorldMatrixBuffer.GetGPUVirtualAddress());
        }
     
        int32_t lightsRootIdx = mOpaquePass.GetResourceRootIndex("PSLights");
        if (lightsRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(lightsRootIdx, currFrameResources.mLightBuffer.GetGPUVirtualAddress());
        }

        int32_t timeRootIdx = mOpaquePass.GetResourceRootIndex("Time");
        if (timeRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(timeRootIdx, currFrameResources.mTimeBuffer.GetGPUVirtualAddress());
        }

        const Mesh* pMesh = codex.GetMesh(GetResourceID(L"teapot.obj"));
        if (pMesh && settings.drawObjects)
        {
            pMesh->DrawIndexed(pCommandList);
        }
    }

    // After opaque pass, transition depth buffer to be bindable as a regular texture by other passes
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetDepthStencilResource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    if (mRaymarchPass.Bind(pCommandList))
    {
        Texture* collisionTexture = codex.GetTexture(GetResourceID(L"ProceduralNVDF"));
        Texture* pSdf = codex.GetTexture(GetResourceID(L"StormbirdCloudSDF_3D"));
        Texture* pNVDF = codex.GetTexture(GetResourceID(L"StormbirdCloud_NVDF"));
        Texture* pNoise = codex.GetTexture(GetResourceID(L"Noise_3D"));

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pOffscreenTarget->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

        // Bind the Camera's Upload Buffer to the root index known by the material
        int32_t cameraRootIdx = mRaymarchPass.GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(cameraRootIdx, mCamera.GetGPUVirtualAddress());
        }

        int32_t aabbIdx = mRaymarchPass.GetResourceRootIndex("AABBBuffer");
        if (aabbIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(aabbIdx, currFrameResources.mAABBBuffer.GetGPUVirtualAddress());
        }

        int32_t hullIdx = mRaymarchPass.GetResourceRootIndex("HullsBuffer");
        if (hullIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(hullIdx, currFrameResources.mHullBuffer.GetGPUVirtualAddress());
        }

        int32_t hullFaceIdx = mRaymarchPass.GetResourceRootIndex("HullFacesBuffer");
        if (hullFaceIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(hullFaceIdx, currFrameResources.mHullFaceBuffer.GetGPUVirtualAddress());
        }

        int32_t inIdx = mRaymarchPass.GetResourceRootIndex("gInput");
        if (inIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(inIdx, pOffscreenTarget->GetSRVHandleGPU());
        }

        int32_t outIdx = mRaymarchPass.GetResourceRootIndex("gOutput");
        if (outIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(outIdx, pComputeOutput->GetUAVHandleGPU());
        }

        int32_t sdfIndex = mRaymarchPass.GetResourceRootIndex("sdfTex");
        if (sdfIndex != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(sdfIndex, pSdf->GetSRVHandleGPU());
        }

        int32_t nvdfIndex = mRaymarchPass.GetResourceRootIndex("nvdfTex");
        if (nvdfIndex != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(nvdfIndex, pNVDF->GetSRVHandleGPU());
        }

        int32_t noiseIndex = mRaymarchPass.GetResourceRootIndex("noiseTex"); 
        if (noiseIndex != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(noiseIndex, pNoise->GetSRVHandleGPU()); 
        }

        int32_t depthBufferIdx = mRaymarchPass.GetResourceRootIndex("depthStencilBuffer");
        if (depthBufferIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(depthBufferIdx, GetDepthStencilSRV().HandleGPU);
        }

        int32_t collisionIndex = mRaymarchPass.GetResourceRootIndex("proceduralNvdfTex");
        if (collisionIndex != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(collisionIndex, collisionTexture->GetSRVHandleGPU());
        }

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pComputeOutput->GetResource(),
            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        UINT numGroupsX = (UINT)ceilf(pOffscreenTarget->GetWidth() / 16.0f);
        UINT numGroupsY = (UINT)ceilf(pOffscreenTarget->GetHeight() / 16.0f);
        pCommandList->Dispatch(numGroupsX, numGroupsY, 1);

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pComputeOutput->GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));

        // Indicate a state transition on the resource usage.
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        // Specify the buffers we are going to render to.
        pCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferView(), true, nullptr);
    }

    // Get depth buffer ready to write depth again
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetDepthStencilResource(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    if (mPostProcessPass.Bind(pCommandList))
    {
        int32_t edgeMapIdx = mPostProcessPass.GetResourceRootIndex("gInput");
        if (edgeMapIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootDescriptorTable(edgeMapIdx, pComputeOutput->GetSRVHandleGPU());
        }

        // Draw fullscreen quad
        pCommandList->IASetVertexBuffers(0, 1, nullptr);
        pCommandList->IASetIndexBuffer(nullptr);
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->DrawInstanced(6, 1, 0, 0);
    }

    ImguiRender();
    FinalizeRender();
    CloseCommandList();
    ExecuteCommandList();
    Present();
    UpdateBackBufferIndex();
}

void Game::CreateDeviceDependentResources()
{
}

void Game::CreateWindowSizeDependentResources(int newWidth, int newHeight)
{
    float aspectRatio = (float)newWidth / (float)newHeight;
    mCamera.UpdateProjection(aspectRatio);
}

Game::~Game()
{ 
    for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
    {
        mFrameResources.at(i).Destroy();
    }

    mCube.Destroy();
    mCamera.Destroy();
    mInput.Destroy();
    mTerrainPass.Destroy();
    mOpaquePass.Destroy();
    mAtmospherePass.Destroy();
    mSobelPass.Destroy();
    mRaymarchPass.Destroy();
    mProcNVDFPass.Destroy();
    mPostProcessPass.Destroy();

    Muon::ImguiShutdown();
    Muon::ResourceCodex::Destroy();
    Muon::DestroyDX12();
}

#pragma region Game State Callbacks
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
}

void Game::OnResuming()
{
    mTimer.ResetElapsedTime();
}

// Recreates Window size dependent resources if needed
void Game::OnMove()
{
}

// Recreates Window size dependent resources if needed
void Game::OnResize(int newWidth, int newHeight)
{
    #if defined(MN_DEBUG)
        try
        {
            CreateWindowSizeDependentResources(newWidth, newHeight);
        }
        catch (std::exception const& e)
        {
            MessageBoxA(Muon::GetHwnd(), e.what(), "Fatal Exception on resize!", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            DestroyWindow(Muon::GetHwnd());
        }
    #else
        CreateWindowSizeDependentResources(newWidth, newHeight);
    #endif
}

void Game::OnMouseMove(short newX, short newY)
{
    mInput.OnMouseMove(newX, newY);
}
#pragma endregion