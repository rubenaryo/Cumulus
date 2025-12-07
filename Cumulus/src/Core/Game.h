/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/12
Description : Interface for central game class
This class encapsulates all app functionality
----------------------------------------------*/
#ifndef GAME_H
#define GAME_H

#include <Core/Camera.h>
#include <Core/Mesh.h>
#include <Core/PipelineState.h>
#include <Core/Pass.h>
#include <Core/StepTimer.h>
#include <Core/FrameResources.h>
#include <Input/GameInput.h>
#include "MuonImgui.h"
#include "Mesh.h"

class Game
{
public:
    Game();
    ~Game();

    bool Init(HWND window, int width, int height);
    bool InitFrameResources(UINT width, UINT height);

    // Main Game Loop
    void Frame();
    void WaitForCurrFrameResource();
    void AdvanceFence();

    // Callbacks for windows messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnMove();
    void SpawnProjectile();
    void OnResize(int newWidth, int newHeight);

    // Input Callbacks
    void OnMouseMove(short newX, short newY);

private:
    void InitEntities();
    void UpdateEntities(Muon::FrameResources& currFrameResources, const Muon::cbTime& time);

    void Update(Muon::StepTimer const& timer);
    void UpdateProceduralNVDF();
    void UpdateRaymarchCache(Muon::FrameResources& currFrameResources);
    void Render();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources(int newWidth, int newHeight);
    void AddMeshToHullBuffer(const Muon::Mesh* m, Muon::cbHulls& hullsBuffer, Muon::cbHullFaces& faces, int& facesOffset, int& hullOffset);

    // Input Management
    Input::GameInput mInput;

    //UI Settings
    Muon::SceneSettings settings;

    // Main Camera
    Muon::Camera mCamera;

    Muon::GraphicsPass mOpaquePass;
    Muon::GraphicsPass mAtmospherePass;
    Muon::ComputePass mSobelPass;
    Muon::ComputePass mRaymarchCachePass;
    Muon::ComputePass mRaymarchPass;
    Muon::ComputePass mProcNVDFPass;
    Muon::GraphicsPass mPostProcessPass;

    static const size_t NUM_FRAMES_IN_FLIGHT = 2;
    std::array<Muon::FrameResources, NUM_FRAMES_IN_FLIGHT> mFrameResources;
    size_t mCurrFrameResourceIdx = 0;

    int jetIdx = -1;
    Muon::cbJetTrailPositions jetTrailPos;

    static const DirectX::XMFLOAT3 jetDir;

    std::vector<int> projectileIndices;

    std::vector<Muon::EntityData> cpuEntityData;

    Muon::cbCloudGenData mCloudData;

    // Timer for the main game loop
    Muon::StepTimer mTimer;

    std::vector<Muon::ProjectilePrefab> projectilePrefabs;

    const float CAMERA_SPEED = 0.05f;
};
#endif