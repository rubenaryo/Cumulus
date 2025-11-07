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
#include <Core/ResourceCodex.h>
#include <Core/Shader.h>
#include <Core/Texture.h>
#include <Core/hash_util.h>
#include <Utils/Utils.h>

Game::Game() :
    mInput(),
    mCamera(),
    mOpaquePass(L"OpaquePass"),
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
    
    Muon::ResetCommandList(nullptr);
    ResourceCodex::Init();
    TextureFactory::CreateOffscreenRenderTarget(Muon::GetDevice(), width, height);

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    const ShaderID kPhongVSID = fnv1a(L"Phong.vs");
    const ShaderID kPhongPSID = fnv1a(L"Phong.ps");    
    const ShaderID kSobelCSID = fnv1a(L"Sobel.cs");
    const ShaderID kRaymarchCSID = fnv1a(L"Raymarch.cs");
    const ShaderID kCompositeVSID = fnv1a(L"Composite.vs");
    const ShaderID kCompositePSID = fnv1a(L"Composite.ps");
    const ShaderID kPassthroughVSID = fnv1a(L"Passthrough.vs");
    const ShaderID kPassthroughPSID = fnv1a(L"Passthrough.ps");
    
    const VertexShader* pPhongVS = codex.GetVertexShader(kPhongVSID);
    const PixelShader* pPhongPS = codex.GetPixelShader(kPhongPSID);
    
    const ComputeShader* pSobelCS = codex.GetComputeShader(kSobelCSID);
    const ComputeShader* pRaymarchCS = codex.GetComputeShader(kRaymarchCSID);
    
    const VertexShader* pCompositeVS = codex.GetVertexShader(kCompositeVSID);
    const PixelShader*  pCompositePS = codex.GetPixelShader(kCompositePSID);
    
    const VertexShader* pPassthroughVS = codex.GetVertexShader(kPassthroughVSID);
    const PixelShader*  pPassthroughPS = codex.GetPixelShader (kPassthroughPSID);

    mCamera.Init(DirectX::XMFLOAT3(3.0, 3.0, 3.0), width / (float)height, 0.1f, 1000.0f);

    // Assemble opaque render pass
    {
        mOpaquePass.SetVertexShader(pPhongVS);
        mOpaquePass.SetPixelShader(pPhongPS);

        if (!mOpaquePass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mOpaquePass.GetName());
    }

    // Assemble compute filter pass
    {
        mSobelPass.SetComputeShader(pSobelCS);

        if (!mSobelPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mSobelPass.GetName());
    }

    // Assemble raymarch pass
    {
        mRaymarchPass.SetComputeShader(pRaymarchCS);

        if (!mRaymarchPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mRaymarchPass.GetName());
    }

    // Assemble post-process render pass
    {
        mPostProcessPass.SetVertexShader(pPassthroughVS);
        mPostProcessPass.SetPixelShader (pPassthroughPS);

        if (!mPostProcessPass.Generate())
            Printf(L"Warning: %s failed to generate!\n", mPostProcessPass.GetName());
    }

    struct PhongVertex
    {
        float position[3];  // POSITION
        float normal[3];    // NORMAL
        float uv[2];        // TEXCOORD
        float tangent[3];   // TANGENT
        float binormal[3];  // BINORMAL
    };

    // Generate a cube centered at origin with side length 1.0f
    PhongVertex cubeVertices[] =
    {
        // Front face (Z+)
        { { -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },

        // Back face (Z-)
        { {  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f } },

        // Top face (Y+)
        { { -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f, -1.0f } },
        { {  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f, -1.0f } },
        { {  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f, -1.0f } },
        { { -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f, -1.0f } },

        // Bottom face (Y-)
        { { -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f } },
        { {  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f } },
        { {  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f } },
        { { -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f } },

        // Right face (X+)
        { {  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, {  0.0f,  0.0f, -1.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, {  0.0f,  0.0f, -1.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, {  0.0f,  0.0f, -1.0f }, {  0.0f,  1.0f,  0.0f } },
        { {  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, {  0.0f,  0.0f, -1.0f }, {  0.0f,  1.0f,  0.0f } },

        // Left face (X-)
        { { -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }, {  0.0f,  0.0f,  1.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }, {  0.0f,  0.0f,  1.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }, {  0.0f,  0.0f,  1.0f }, {  0.0f,  1.0f,  0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }, {  0.0f,  0.0f,  1.0f }, {  0.0f,  1.0f,  0.0f } },
    };

    // Index buffer for the cube (36 indices = 12 triangles)
    uint32_t cubeIndices[] =
    {
        // Front face
        0, 1, 2,    0, 2, 3,
        // Back face
        4, 5, 6,    4, 6, 7,
        // Top face
        8, 9, 10,   8, 10, 11,
        // Bottom face
        12, 13, 14, 12, 14, 15,
        // Right face
        16, 17, 18, 16, 18, 19,
        // Left face
        20, 21, 22, 20, 22, 23
    };

    float aspectRatio = width / (float)height;

    Muon::UploadBuffer& stagingBuffer = codex.GetMeshStagingBuffer();
    //mCube.Create(L"TestCube", sizeof(cubeVertices), sizeof(PhongVertex), sizeof(cubeIndices), sizeof(cubeIndices) / sizeof(uint32_t), DXGI_FORMAT_R32_UINT);
    //stagingBuffer.UploadToMesh(Muon::GetCommandList(), mCube, cubeVertices, sizeof(cubeVertices), cubeIndices, sizeof(cubeIndices));

    mWorldMatrixBuffer.Create(L"world matrix buffer", sizeof(cbPerEntity));
    void* mapped = mWorldMatrixBuffer.Map();
    cbPerEntity entity;
    DirectX::XMMATRIX entityWorld = DirectX::XMMatrixTranslation(0, 1, 0);
    DirectX::XMStoreFloat4x4(&entity.world, entityWorld);
    DirectX::XMStoreFloat4x4(&entity.invWorld, DirectX::XMMatrixInverse(nullptr, entityWorld));
    memcpy(mapped, &entity, sizeof(entity));
    mWorldMatrixBuffer.Unmap(0, mWorldMatrixBuffer.GetBufferSize());

    mLightBuffer.Create(L"Light Buffer", sizeof(cbLights));

    Muon::CloseCommandList();
    Muon::ExecuteCommandList();
    return success;
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
    mInput.Frame(elapsedTime, &mCamera);
    mCamera.UpdateView();

    Muon::cbLights lights;

    lights.ambientColor = DirectX::XMFLOAT3A(+1.0f, +0.772f, +0.56f);

    lights.directionalLight.diffuseColor = DirectX::XMFLOAT3A(1, 1, 1);
    lights.directionalLight.dir = DirectX::XMFLOAT3A(cos(elapsedTime), 0.0, sin(elapsedTime));

    DirectX::XMStoreFloat3(&lights.cameraWorldPos, mCamera.GetPosition());

    void* mapped = mLightBuffer.Map();
    memcpy(mapped, &lights, sizeof(Muon::cbLights));
    mLightBuffer.Unmap(0, mLightBuffer.GetBufferSize());
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

    // Fetch the desired material from the codex
    ResourceCodex& codex = ResourceCodex::GetSingleton();
    MaterialID matId = fnv1a("Phong");
    const Muon::Material* pPhongMaterial = codex.GetMaterialType(matId);
    
    Texture* pOffscreenTarget = codex.GetTexture(fnv1a("OffscreenTarget"));
    Texture* pComputeOutput = codex.GetTexture(fnv1a("SobelOutput"));
    if (!pOffscreenTarget || !pComputeOutput)
    {
        Muon::Printf("Error: Game::Render Failed to fetch the offscreen target and compute output textures.\n");
        return;
    }

    ID3D12GraphicsCommandList* pCommandList = GetCommandList();
    pCommandList->SetDescriptorHeaps(1, GetSRVHeap()->GetHeapAddr());

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
            pCommandList->SetGraphicsRootConstantBufferView(worldMatrixRootIdx, mWorldMatrixBuffer.GetGPUVirtualAddress());
        }

        int32_t lightsRootIdx = mOpaquePass.GetResourceRootIndex("PSLights");
        if (lightsRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetGraphicsRootConstantBufferView(lightsRootIdx, mLightBuffer.GetGPUVirtualAddress());
        }

        const Mesh* pCube = codex.GetMesh(fnv1a(L"cube.obj"));
        if (pCube)
        {
            pCube->Draw(pCommandList);
        }
    }

    if (mRaymarchPass.Bind(pCommandList))
    {
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pOffscreenTarget->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

        // Bind the Camera's Upload Buffer to the root index known by the material
        int32_t cameraRootIdx = mRaymarchPass.GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            pCommandList->SetComputeRootConstantBufferView(cameraRootIdx, mCamera.GetGPUVirtualAddress());
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
        pCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferView(), true, &GetDepthStencilView());
    }

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
    mCube.Destroy();
    mWorldMatrixBuffer.Destroy();
    mLightBuffer.Destroy();
    mCamera.Destroy();
    mInput.Destroy();
    mOpaquePass.Destroy();
    mSobelPass.Destroy();
    mRaymarchPass.Destroy();
    mPostProcessPass.Destroy();

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
            //MessageBoxA(mDeviceResources.GetWindow(), e.what(), "Fatal Exception!", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            //DestroyWindow(mDeviceResources.GetWindow());
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