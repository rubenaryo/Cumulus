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
    void OnResize(int newWidth, int newHeight);

    // Input Callbacks
    void OnMouseMove(short newX, short newY);

private:
    void Update(Muon::StepTimer const& timer);
    void UpdateProceduralNVDF();
    void Render();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources(int newWidth, int newHeight);

    // Input Management
    Input::GameInput mInput;

    //UI Settings
    Muon::SceneSettings settings;

    // Main Camera
    Muon::Camera mCamera;

    Muon::GraphicsPass mOpaquePass;
    Muon::GraphicsPass mAtmospherePass;
    Muon::ComputePass mSobelPass;
    Muon::ComputePass mRaymarchPass;
    Muon::ComputePass mProcNVDFPass;
    Muon::GraphicsPass mPostProcessPass;

    // TEMP: For testing
    Muon::Mesh mCube;

    static const size_t NUM_FRAMES_IN_FLIGHT = 2;
    std::array<Muon::FrameResources, NUM_FRAMES_IN_FLIGHT> mFrameResources;
    size_t mCurrFrameResourceIdx = 0;

    // Timer for the main game loop
    Muon::StepTimer mTimer;
};
#endif