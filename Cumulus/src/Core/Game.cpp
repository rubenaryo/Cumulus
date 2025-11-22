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
#include <Utils/Utils.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

Game::Game() :
    mInput(),
    mCamera(),
    mOpaquePass(L"OpaquePass"),
    mAtmospherePass(L"AtmospherePass"),
    mSobelPass(L"SobelPass"),
    mRaymarchPass(L"RaymarchPass"),
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

    // Assemble post-process render pass
    {
        mPostProcessPass.SetVertexShader(codex.GetVertexShader(GetResourceID(L"Passthrough.vs")));
        mPostProcessPass.SetPixelShader(codex.GetPixelShader(GetResourceID(L"Passthrough.ps")));
        mPostProcessPass.SetEnableDepth(false);

        if (!mPostProcessPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mPostProcessPass.GetName());
    }

    


#if 0
    mWorldMatrixBuffer.Create(L"world matrix buffer", sizeof(cbPerEntity));
    UINT8* mapped = mWorldMatrixBuffer.GetMappedPtr();
    assert(mapped);


    const float PI = 3.14159f;
    DirectX::XMMATRIX debugEntityWorld = DirectX::XMMatrixIdentity();
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixRotationRollPitchYaw(0, 0, PI / 2.0f));
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixRotationRollPitchYaw(-PI / 2.0f, 0, 0));
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixScaling(0.12f, 0.12f, 0.12f));
    debugEntityWorld = XMMatrixMultiply(debugEntityWorld, DirectX::XMMatrixTranslation(0, 1, 0));

    if (mapped)
    {
        using namespace DirectX;
        cbPerEntity entity;
        XMStoreFloat4x4(&entity.world, debugEntityWorld);
        XMStoreFloat4x4(&entity.invWorld, DirectX::XMMatrixInverse(nullptr, debugEntityWorld));

        const float PI = 3.14159f;
        XMMATRIX entityWorld = DirectX::XMMatrixIdentity();
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixRotationRollPitchYaw(0, 0, PI / 2.0f));
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixRotationRollPitchYaw(-PI / 2.0f, 0, 0));
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixScaling(0.12f, 0.12f, 0.12f));
        entityWorld = XMMatrixMultiply(entityWorld, DirectX::XMMatrixTranslation(0, 1, 0));
        XMStoreFloat4x4(&entity.world, entityWorld);
        XMStoreFloat4x4(&entity.invWorld, DirectX::XMMatrixInverse(nullptr, entityWorld));
        memcpy(mapped, &entity, sizeof(entity));
    }

    mLightBuffer.Create(L"Light Buffer", sizeof(cbLights));
    mTimeBuffer.Create(L"Time", sizeof(cbTime));
    mAtmosphereBuffer.Create(L"Atmosphere CB", sizeof(cbAtmosphere));

    cbAtmosphere atmosphereParams;
    InitializeAtmosphereConstants(atmosphereParams, width, height);

    mapped = mAtmosphereBuffer.GetMappedPtr();
    if (mapped)
    {
        memcpy(mapped, &atmosphereParams, sizeof(cbAtmosphere));
    }

    mAABBBuffer.Create(L"AABB Buffer", sizeof(cbIntersections));

    const Mesh* m = codex.GetMesh(Muon::GetResourceID(L"teapot.obj"));

    cbIntersections intersections = {};
    intersections.aabbCount = 1;
    intersections.aabbs[0] = m->GetAABB();
    if (m) {
        UINT8* aabbPtr = mAABBBuffer.GetMappedPtr();
        if (aabbPtr) {
            memcpy(aabbPtr, &intersections, sizeof(intersections));
        }
    }

    //HULL:
    //todo: concat all hulls
    Hull h = m->GetHull();
    mHullBuffer.Create(L"Hull Buffer", sizeof(cbHulls));

    if (m && mHullBuffer.GetMappedPtr())
    {
        cbHulls hulls = {};
        cbConvexHull cHull = {};
        cHull.faceCount = (uint32_t)h.faces.size();
        cHull.faceOffset = 0;


        XMStoreFloat4x4(&cHull.world, debugEntityWorld);
        XMStoreFloat4x4(&cHull.invWorld, DirectX::XMMatrixInverse(nullptr, debugEntityWorld));

        hulls.hulls[0] = cHull;
        hulls.hullCount = 1;
        memcpy(mHullBuffer.GetMappedPtr(), &hulls, sizeof(hulls));
    }

    mHullFaceBuffer.Create(L"Hull Faces Buffer", sizeof(cbHullFaces));
    if (m && h.faces.size() > 0) {
        UINT8* facePtr = mHullFaceBuffer.GetMappedPtr();
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

        if (facePtr) {
            memcpy(facePtr, &faces, sizeof(faces));
        }
    }

    mTimeBuffer.Create(L"Time", sizeof(cbTime));
#endif // 0

    InitFrameResources(width, height);

    Muon::CloseCommandList();
    Muon::ExecuteCommandList();
    return success;
}

bool Game::InitFrameResources(UINT width, UINT height)
{
    for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
    {
        mFrameResources.at(i).Create(width, height);
    }

    return true;
}

// On Timer tick, run Update() on the game, then Render()
void Game::Frame()
{
    mTimer.Tick([&]()
    {
        Update(mTimer);
    });

    Render();

    Muon::UpdateTitleBar(mTimer.GetFramesPerSecond(), mTimer.GetFrameCount());
}

void Game::Update(Muon::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());
    float totalTime = float(timer.GetTotalSeconds());
    mInput.Frame(elapsedTime, &mCamera);
    mCamera.UpdateView();

    Muon::cbLights lights;

    lights.ambientColor = DirectX::XMFLOAT3A(+1.0f, +0.772f, +0.56f);
    lights.directionalLight.diffuseColor = DirectX::XMFLOAT3A(1.0, 1.0, 1.0);
    lights.directionalLight.dir = DirectX::XMFLOAT3A(0,1,0);

    DirectX::XMStoreFloat3(&lights.cameraWorldPos, mCamera.GetPosition());

    UINT8* mapped = mLightBuffer.GetMappedPtr();
    
    if (mapped)
        memcpy(mapped, &lights, sizeof(Muon::cbLights));


    Muon::cbTime time;
    time.totalTime = (float)timer.GetTotalSeconds();
    time.deltaTime = elapsedTime;

    UINT8* timeBuf = mTimeBuffer.GetMappedPtr();
    if (timeBuf)
        memcpy(timeBuf, &time, sizeof(Muon::cbTime));

    // Updating Atmosphere
    Muon::cbAtmosphere atmosphereParams;
    Muon::UpdateAtmosphere(atmosphereParams, mCamera, settings.isSunDynamic, settings.timeOfDay, time.totalTime);
    //Muon::InitializeAtmosphereConstants(atmosphereParams, 1280, 800);
    settings.sunDir = atmosphereParams.sun_direction;

    mapped = mAtmosphereBuffer.GetMappedPtr();
    if (mapped)
    {
        memcpy(mapped, &atmosphereParams, sizeof(Muon::cbAtmosphere));
    }
    mFrameResources.at(0).Update(totalTime, elapsedTime);
}

void Game::Render()
{
    using namespace Muon;

    // Don't try to render anything before the first Update.
    if (mTimer.GetFrameCount() == 0)
    {
        return;
    }

    ResetCommandList(nullptr);
    PrepareForRender();

    ImguiNewFrame(mTimer.GetTotalSeconds(), mCamera, settings);

    // Fetch the desired material from the codex
    ResourceCodex& codex = ResourceCodex::GetSingleton();
    ResourceID phongMatId = GetResourceID(L"Phong");
    const Muon::Material* pPhongMaterial = codex.GetMaterialType(phongMatId);

    FrameResources& currFrameResources = mFrameResources.at(0);
    
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
        if (pMesh)
        {
            pMesh->DrawIndexed(pCommandList);
        }
    }

    // After opaque pass, transition depth buffer to be bindable as a regular texture by other passes
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetDepthStencilResource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    if (mRaymarchPass.Bind(pCommandList))
    {
        Texture* pSdfNVDF = codex.GetTexture(GetResourceID(L"StormbirdCloud_NVDF"));
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
            pCommandList->SetComputeRootConstantBufferView(hullIdx, mHullBuffer.GetGPUVirtualAddress());
        }

        int32_t hullFaceIdx = mRaymarchPass.GetResourceRootIndex("HullFacesBuffer");
        if (hullFaceIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(hullFaceIdx, mHullFaceBuffer.GetGPUVirtualAddress());
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

        int32_t sdfNVDFIndex = mRaymarchPass.GetResourceRootIndex("sdfNvdfTex");
        if (sdfNVDFIndex != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(sdfNVDFIndex, pSdfNVDF->GetSRVHandleGPU());
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
    FlushCommandQueue();
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
    mWorldMatrixBuffer.Destroy();
    mLightBuffer.Destroy();
    mTimeBuffer.Destroy();
    mAABBBuffer.Destroy();
    mAtmosphereBuffer.Destroy();
    mHullBuffer.Destroy();
    mHullFaceBuffer.Destroy();
    mCamera.Destroy();
    mInput.Destroy();
    mOpaquePass.Destroy();
    mAtmospherePass.Destroy();
    mSobelPass.Destroy();
    mRaymarchPass.Destroy();
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