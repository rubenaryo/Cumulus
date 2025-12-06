/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/12
Description : Implementation of Game.h
----------------------------------------------*/
#include "Game.h"

#include <Core/DXCore.h>
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
    mOpaquePass(L"OpaquePass"),
    mAtmospherePass(L"AtmospherePass"),
    mSobelPass(L"SobelPass"),
    mRaymarchCachePass(L"RaymarchCachePass"),
    mRaymarchPass(L"RaymarchPass"),
    mProcNVDFPass(L"ProcNVDFPass"),
    mPostProcessPass(L"PostProcessPass")
{
    mTimer.SetFixedTimeStep(false);
}

const DirectX::XMFLOAT3 Game::jetDir = { -5.f, 0.f, 0.f };

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

    mCamera.Init(DirectX::XMFLOAT3(700.0, -25.0, 0.0), width / (float)height, 0.01f, 4000.0f);

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

    // Assemble raymarch cache pass
    {
        mRaymarchCachePass.SetComputeShader(codex.GetComputeShader(GetResourceID(L"RaymarchCache.cs")));

        if (!mRaymarchCachePass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mRaymarchCachePass.GetName());
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

    InitEntities();
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
    const Mesh* m = codex.GetMesh(GetResourceID(L"sphere.obj"));
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

    // Initialize sphere hull:
    cbConvexHull cHull = {};
    cHull.faceCount = (uint32_t)h.faces.size();
    cHull.faceOffset = 0;;

    cbHulls hulls = {};
    hulls.hulls[0] = cHull;
    hulls.hullCount = 1;

    // Create each frame resource and fill it with static data.
    for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
    {
        FrameResources& frameResource = mFrameResources.at(i);
        frameResource.Create(width, height);
        
        frameResource.UpdateAtmosphere(atmosphereParams);
        frameResource.UpdateCloudData(mCloudData);
        frameResource.UpdateCloudLighting(settings.lighting);
        frameResource.UpdateAABB(intersections);
        frameResource.UpdateHullFaces(faces);
        frameResource.UpdateHulls(hulls);
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

void Game::InitEntities()
{
    using namespace Muon;
    using namespace DirectX;

    ResourceCodex& codex = ResourceCodex::GetSingleton();

    EntityData jetEntity;

    // ---- 1. Setup initial orientation using flightDir ----------------------

    XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&jetDir));
    XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    // If forward is nearly parallel to up, pick a different up
    if (fabs(XMVectorGetX(XMVector3Dot(forward, up))) > 0.99f)
        up = XMVectorSet(1.f, 0.f, 0.f, 0.f);

    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
    up = XMVector3Normalize(XMVector3Cross(forward, right));

    // ---- 1b. Set initial world position -----------------------------------
    XMVECTOR jetStartPos = XMVectorSet(500.0, 500, 0, 1.f);//0.f - flightDir.x * 200.f, 1000.f, -flightDir.z * 200.f, 1.f); // <-- your initial position
    XMStoreFloat4(&jetTrailPos.positions[0], jetStartPos);

    // Build world matrix correctly
    XMMATRIX world = XMMatrixIdentity();

    world.r[0] = XMVectorSetW(right, 0.f);     // X basis
    world.r[1] = XMVectorSetW(up, 0.f);        // Y basis
    world.r[2] = XMVectorSetW(forward, 0.f);   // Z basis
    world.r[3] = jetStartPos;                   // position

    XMStoreFloat4x4(&jetEntity.entityMatrices.world, world);
    XMStoreFloat4x4(&jetEntity.entityMatrices.invWorld,
        XMMatrixInverse(nullptr, world));

    // ---- 2. Resource + hull ------------------------------------------------

    jetEntity.resourceID = Muon::GetResourceID(L"jet.obj");
    jetEntity.hullIdx = -1;

    Hull hull = codex.GetMesh(jetEntity.resourceID)->GetHull();
    cbConvexHull cHull = {};
    cHull.faceCount = (uint32_t)hull.faces.size();
    cHull.faceOffset = 0;


    // ---- 3. Store ----------------------------------------------------------

    jetIdx = 0;
    cpuEntityData.push_back(jetEntity);
}

void Game::SpawnProjectile()
{
    using namespace DirectX;
    using namespace Muon;

    if (cpuEntityData.size() >= MAX_ENTITY_COUNT) {
        return;
    }

    ResourceCodex& codex = ResourceCodex::GetSingleton();

    EntityData newProjectile{};
    newProjectile.resourceID = GetResourceID(L"sphere.obj");
    newProjectile.hullIdx = sphereHullIdx;

    // Store world & invWorld
    XMMATRIX view = mCamera.GetView();
    XMMATRIX camWorld = XMMatrixInverse(nullptr, view);

    XMStoreFloat4x4(&newProjectile.entityMatrices.world, camWorld);
    XMStoreFloat4x4(&newProjectile.entityMatrices.invWorld, view);

    // Extract camera basis
    XMVECTOR camForward = XMVector3Normalize(camWorld.r[2]);
    XMVECTOR camUp = XMVector3Normalize(camWorld.r[1]);

    // Build projectile velocity
    float forwardSpeed = 50.f;
    float upBoost = 10.f;

    XMVECTOR vel = camForward * forwardSpeed + camUp * upBoost;

    // Store velocity
    XMStoreFloat4(&newProjectile.vel, vel);

    projectileIndices.push_back(cpuEntityData.size());
    cpuEntityData.push_back(newProjectile);
}



void Game::UpdateEntities(Muon::FrameResources& currFrameResources, const Muon::cbTime& time)
{
    using namespace Muon;
    using namespace DirectX;

    if (jetIdx >= 0 && mCloudData.demoMode == 1) {
        float jetSpeed = 55.f * time.deltaTime;

        Muon::EntityData& jet = cpuEntityData[jetIdx];   

        DirectX::XMMATRIX world = XMLoadFloat4x4(&jet.entityMatrices.world);

        DirectX::XMVECTOR forward = world.r[2];  
        forward = DirectX::XMVector3Normalize(forward);

        DirectX::XMVECTOR translation =
            DirectX::XMVectorScale(forward, jetSpeed);

        world = DirectX::XMMatrixMultiply(world,
            DirectX::XMMatrixTranslationFromVector(translation));

        DirectX::XMStoreFloat4x4(&jet.entityMatrices.world, world);
        DirectX::XMStoreFloat4x4(&jet.entityMatrices.invWorld, DirectX::XMMatrixInverse(nullptr, world));

        DirectX::XMVECTOR jetPos = world.r[3];
        DirectX::XMStoreFloat4(&jetTrailPos.positions[1], jetPos);

        float initialScale = 10.f;
        float finalScale = 80.f;
        float radGrowthTime = 100.f;
        float t = std::clamp(time.totalTime / radGrowthTime, 0.f, 1.f);
        float currScale = finalScale * t + (1.f - t) * initialScale;


        jetTrailPos.positions[0].w = currScale;
        jetTrailPos.positions[1].w = initialScale;


        currFrameResources.UpdateJetTrail(jetTrailPos);
    }

    if (!projectileIndices.empty())
    {
        for (int i = 0; i < projectileIndices.size(); ++i)
        {
            EntityData& projectile = cpuEntityData[projectileIndices[i]];

            // Convert data
            projectile.vel.y -= 9.8 * time.deltaTime;
            XMVECTOR vel = XMLoadFloat4(&projectile.vel);
            XMMATRIX world = XMLoadFloat4x4(&projectile.entityMatrices.world);

            // Extract linear velocity
            XMVECTOR linearVel = XMVectorSetW(vel, 0.0f); // remove rot rate
            float rotationRate = projectile.vel.w;        // rot per second

            // --- MOVE ---
            XMVECTOR forwardMove = linearVel * time.deltaTime;
            world.r[3] = XMVectorAdd(world.r[3], forwardMove);  // modify translation

            // --- ROTATE ---
            if (rotationRate != 0.0f)
            {
                // Rotate around the projectile’s local Y axis (example)
                XMMATRIX rot = XMMatrixRotationY(rotationRate * time.deltaTime);

                // Apply rotation BEFORE translation
                world = rot * world;
            }

            // Store results back
            DirectX::XMStoreFloat4x4(&projectile.entityMatrices.world, world);
            DirectX::XMStoreFloat4x4(&projectile.entityMatrices.invWorld, XMMatrixInverse(nullptr, world));
        }
    }

    currFrameResources.UpdateEntities(cpuEntityData);
}

void Game::Update(Muon::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());
    float totalTime = float(timer.GetTotalSeconds());
    mInput.Frame(elapsedTime, &mCamera, this);

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

    if (settings.updateLighting)
    {
        // Mark the update on each frame resource. They will consume it when they next run. 
        for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
        {
            Muon::FrameResources& frameResource = mFrameResources.at(i);
            frameResource.mNeedsCloudLightingUpdate = true;
        }

        settings.updateLighting = false;
    }

    Muon::FrameResources& currFrameResources = mFrameResources.at(mCurrFrameResourceIdx);

    // Updating Camera
    mCamera.UpdateView();    
    Muon::cbCamera camera = mCamera.GetAsCB();
    currFrameResources.UpdateCamera(camera);
    
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
    Muon::UpdateAtmosphere(atmosphere, mCamera, settings.atmosphere);
    // NOTE: this is explicit to switch Y and Z due to different coordinate systems at play
    settings.atmosphere.sunDir = { atmosphere.sun_direction.x,atmosphere.sun_direction.z,atmosphere.sun_direction.y };
    currFrameResources.UpdateAtmosphere(atmosphere);

    // Updating Cloud Data
    if (currFrameResources.mNeedsCloudUpdate)
    {
        currFrameResources.UpdateCloudData(mCloudData);
        currFrameResources.mNeedsCloudUpdate = false;
    }

    // Updating Cloud Lighting
    if (currFrameResources.mNeedsCloudLightingUpdate)
    {
        currFrameResources.UpdateCloudLighting(settings.lighting);
        currFrameResources.mNeedsCloudLightingUpdate = false;
    }

    // Updating Entities
    UpdateEntities(currFrameResources, time);
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

    Texture* pProcNoise = codex.GetTexture(GetResourceID(L"ProceduralCloudNoise_3D"));
    int32_t procNoiseIndex = mProcNVDFPass.GetResourceRootIndex("proceduralNoiseTex");
    if (procNoiseIndex != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootDescriptorTable(procNoiseIndex, pProcNoise->GetSRVHandleGPU());
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

    int32_t jetTrailIdx = mProcNVDFPass.GetResourceRootIndex("cbJetBuffer");
    if (jetTrailIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(jetTrailIdx, currFrameResources.mJetTrailBuffer.GetGPUVirtualAddress());
    }


    int32_t timeRootIdx = mProcNVDFPass.GetResourceRootIndex("Time");
    if (timeRootIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(timeRootIdx, currFrameResources.mTimeBuffer.GetGPUVirtualAddress());
    }

    int32_t entitiesIdx = mProcNVDFPass.GetResourceRootIndex("entities");
    if (entitiesIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(entitiesIdx, currFrameResources.mEntitiesBuffer.GetGPUVirtualAddress());
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

void Game::UpdateRaymarchCache(Muon::FrameResources& currFrameResources)
{
    using namespace Muon;

    // Note: Assumes command list is open

    ID3D12GraphicsCommandList* pCommandList = Muon::GetCommandList();
    if (!pCommandList)
        return;

    if (!mRaymarchCachePass.Bind(pCommandList))
    {
        Muon::Print("Error: Failed to bind raymarch caching pass.\n");
        CloseCommandList();
        return;
    }

    pCommandList->SetDescriptorHeaps(1, GetSRVHeap()->GetHeapAddr());

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    Texture* pLightingCache = codex.GetTexture(GetResourceID(L"LightingCache"));
    Texture* pSdf = codex.GetTexture(GetResourceID(L"StormbirdCloudSDF_3D")); // TODO: Ensure same resource IDs are used in main raymarch pass. These MUST match.
    Texture* pNVDF = codex.GetTexture(GetResourceID(L"StormbirdCloud_NVDF"));
    Texture* pNoise = codex.GetTexture(GetResourceID(L"Noise_3D"));

    int32_t cloudLightingIdx = mRaymarchCachePass.GetResourceRootIndex("CloudLightingBuffer");
    if (cloudLightingIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootConstantBufferView(cloudLightingIdx, currFrameResources.mCloudLightingBuffer.GetGPUVirtualAddress());
    }

    int32_t sdfIndex = mRaymarchCachePass.GetResourceRootIndex("sdfTex");
    if (sdfIndex != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootDescriptorTable(sdfIndex, pSdf->GetSRVHandleGPU());
    }
    
    int32_t nvdfIndex = mRaymarchCachePass.GetResourceRootIndex("nvdfTex");
    if (nvdfIndex != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootDescriptorTable(nvdfIndex, pNVDF->GetSRVHandleGPU());
    }
    
    int32_t noiseIndex = mRaymarchCachePass.GetResourceRootIndex("noiseTex");
    if (noiseIndex != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootDescriptorTable(noiseIndex, pNoise->GetSRVHandleGPU());
    }
    
    int32_t cacheIdx = mRaymarchCachePass.GetResourceRootIndex("gCache");
    if (cacheIdx != ROOTIDX_INVALID)
    {
        pCommandList->SetComputeRootDescriptorTable(cacheIdx, pLightingCache->GetUAVHandleGPU());
    }
    
    // Prepare volume cache for writing
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pLightingCache->GetResource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    
    // TODO: Figure out workgroup size
    UINT numGroupsX = (UINT)ceilf(pLightingCache->GetWidth() / 8.0f);
    UINT numGroupsY = (UINT)ceilf(pLightingCache->GetHeight() / 8.0f);
    UINT numGroupsZ = (UINT)ceilf(pLightingCache->GetDepth() / 4.0f);
    pCommandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
    
    // Done writing, prepare for reading
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pLightingCache->GetResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
        
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
            pCommandList->SetGraphicsRootConstantBufferView(cameraRootIdx, currFrameResources.mCameraBuffer.GetGPUVirtualAddress());
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
            pCommandList->SetGraphicsRootConstantBufferView(cameraRootIdx, currFrameResources.mCameraBuffer.GetGPUVirtualAddress());
        }

        // Bind the world matrix Upload Buffer to the root index known by the material
        int32_t entityMatrix = mOpaquePass.GetResourceRootIndex("VSWorld");
        if (entityMatrix == ROOTIDX_INVALID)
        {
            Print(L"Error: Entity Matrix not initialized!\n");
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

        const Mesh* pMesh = codex.GetMesh(GetResourceID(L"jet.obj"));
        if (pMesh && settings.drawObjects && mCloudData.demoMode == 1)
        {
           pCommandList->SetGraphicsRootConstantBufferView(entityMatrix, currFrameResources.mEntitiesBuffer.GetGPUVirtualAddress() + jetIdx * Muon::AlignToBoundary(sizeof(cbPerEntity), 16));

           pMesh->DrawIndexed(pCommandList);
        }

        if (projectileIndices.size() > 0 && mCloudData.demoMode == 2) {
            for (int i = 0; i < projectileIndices.size(); ++i) {
                const EntityData& projectile = cpuEntityData[projectileIndices[i]];
                const Mesh* mesh = codex.GetMesh(projectile.resourceID);
                if (mesh && settings.drawObjects) {
                    pCommandList->SetGraphicsRootConstantBufferView(entityMatrix, currFrameResources.mEntitiesBuffer.GetGPUVirtualAddress() + projectileIndices[i] * Muon::AlignToBoundary(sizeof(cbPerEntity), 16));

                    //currFrameResources.UpdateWorldMatrix(projectile.entityMatrices);
                    mesh->DrawIndexed(pCommandList);
                }
            }
        }
    }

    if (mTimer.GetTotalTicks() % 16 == 0)
    {
        UpdateRaymarchCache(currFrameResources);
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
        Texture* pLightingCache = codex.GetTexture(GetResourceID(L"LightingCache"));

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pOffscreenTarget->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

        // Bind the Camera's Upload Buffer to the root index known by the material
        int32_t cameraRootIdx = mRaymarchPass.GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(cameraRootIdx, currFrameResources.mCameraBuffer.GetGPUVirtualAddress());
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

        int32_t cloudLightingIdx = mRaymarchPass.GetResourceRootIndex("CloudLightingBuffer");
        if (cloudLightingIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(cloudLightingIdx, currFrameResources.mCloudLightingBuffer.GetGPUVirtualAddress());
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

        int32_t lightingCacheIndex = mRaymarchPass.GetResourceRootIndex("lightCacheTex");
        if (lightingCacheIndex != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootDescriptorTable(lightingCacheIndex, pLightingCache->GetSRVHandleGPU());
        }

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pComputeOutput->GetResource(),
            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        UINT numGroupsX = (UINT)ceilf(pOffscreenTarget->GetWidth() / 8.0f);
        UINT numGroupsY = (UINT)ceilf(pOffscreenTarget->GetHeight() / 8.0f);
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
    Muon::FlushCommandQueue();

    for (size_t i = 0; i != NUM_FRAMES_IN_FLIGHT; ++i)
    {
        mFrameResources.at(i).Destroy();
    }

    mCamera.Destroy();
    mInput.Destroy();
    mOpaquePass.Destroy();
    mAtmospherePass.Destroy();
    mSobelPass.Destroy();
    mRaymarchPass.Destroy();
    mRaymarchCachePass.Destroy();
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
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    mInput.OnMouseMove(newX, newY);
}
#pragma endregion