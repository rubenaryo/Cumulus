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
#include <Core/PipelineState.h>
#include <Core/ResourceCodex.h>
#include <Core/Shader.h>
#include <Core/hash_util.h>
#include <Utils/Utils.h>

Game::Game() :
    mInput(),
    mCamera()
{
    mTimer.SetFixedTimeStep(false);
}

bool Game::Init(HWND window, int width, int height)
{
    using namespace Muon;

    bool success = Muon::InitDX12(window, width, height);
    
    Muon::ResetCommandList(nullptr);
    ResourceCodex::Init();

    ResourceCodex& codex = ResourceCodex::GetSingleton();
    const ShaderID kSimpleVSID = fnv1a(L"SimpleVS.cso");
    const ShaderID kSimplePSID = fnv1a(L"SimplePS.cso");
    const ShaderID kPhongVSID = fnv1a(L"PhongVS.cso");
    const ShaderID kPhongPSID = fnv1a(L"PhongPS.cso");
    const VertexShader* pVS = codex.GetVertexShader(kSimpleVSID);
    const PixelShader* pPS = codex.GetPixelShader(kSimplePSID);
    
    const VertexShader* pPhongVS = codex.GetVertexShader(kPhongVSID);
    const PixelShader* pPhongPS = codex.GetPixelShader(kPhongPSID);

    mCamera.Init(DirectX::XMFLOAT3(3.0, 3.0, 3.0), width / (float)height, 0.1f, 1000.0f);

    struct Vertex
    {
        float Pos[4];
        float Col[4];
    };

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
    Vertex triangleVertices[] =
    {
        { { 0.0f, 0.25f * aspectRatio, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { 0.25f, -0.25f * aspectRatio, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.25f, -0.25f * aspectRatio, 0.0f, 1.0f}, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };

    //Muon::ResetCommandList(mPSO.GetPipelineState());
    Muon::UploadBuffer& stagingBuffer = codex.GetMeshStagingBuffer();
    stagingBuffer.Map();
    Muon::MeshFactory::LoadAllMeshes(codex);
    mTriangle.Init(triangleVertices, sizeof(triangleVertices), sizeof(Vertex), nullptr, 0, 0, DXGI_FORMAT_R32_UINT);
    mCube.Init(cubeVertices, sizeof(cubeVertices), sizeof(PhongVertex), cubeIndices, sizeof(cubeIndices), sizeof(cubeIndices) / sizeof(uint32_t), DXGI_FORMAT_R32_UINT);
    stagingBuffer.Unmap(0, stagingBuffer.GetBufferSize());

    mWorldMatrixBuffer.Create(L"world matrix buffer", sizeof(cbPerEntity));
    void* mapped = mWorldMatrixBuffer.Map();
    cbPerEntity entity;
    DirectX::XMStoreFloat4x4(&entity.world, DirectX::XMMatrixIdentity());
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
    MaterialTypeID matId = fnv1a("Phong");
    const Muon::MaterialType* pPhongMaterial = codex.GetMaterialType(matId);
    if (pPhongMaterial)
    {
        // Bind the material's PipelineState and RootSignature (Defined by Shaders)
        pPhongMaterial->Bind(GetCommandList());
        
        // Bind the Camera's Upload Buffer to the root index known by the material
        int32_t cameraRootIdx = pPhongMaterial->GetResourceRootIndex("VSCamera");
        if (cameraRootIdx != ROOTIDX_INVALID)
        {
            mCamera.Bind(cameraRootIdx, GetCommandList());
        }

        // Bind the world matrix Upload Buffer to the root index known by the material
        int32_t worldMatrixRootIdx = pPhongMaterial->GetResourceRootIndex("VSWorld");
        if (worldMatrixRootIdx != ROOTIDX_INVALID)
        {
            GetCommandList()->SetGraphicsRootConstantBufferView(worldMatrixRootIdx, mWorldMatrixBuffer.GetGPUVirtualAddress());
        }

        int32_t lightsRootIdx = pPhongMaterial->GetResourceRootIndex("PSLights");
        if (lightsRootIdx != ROOTIDX_INVALID)
        {
            GetCommandList()->SetGraphicsRootConstantBufferView(lightsRootIdx, mLightBuffer.GetGPUVirtualAddress());
        }
    }

    // Fetch the desired mesh from the codex
    const MeshID cubeID = fnv1a("cube.obj");
    const Mesh* cubeMesh = codex.GetMesh(cubeID);
    if (cubeMesh)
    {
        // Bind VBO/IBO and Draw
        mCube.Draw(Muon::GetCommandList());
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
    mTriangle.Release();
    mCube.Release();
    mWorldMatrixBuffer.Destroy();
    mLightBuffer.Destroy();
    mCamera.Destroy();
    mInput.Destroy();

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