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
#include <Core/StepTimer.h>

#include <Input/GameInput.h>

class Game
{
public:
    Game();
    ~Game();

    bool Init(HWND window, int width, int height);

    // Main Game Loop
    void Frame();

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
    void Render();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources(int newWidth, int newHeight);

    // Input Management
    Input::GameInput mInput;

    // Main Camera
    Muon::Camera mCamera;

    // TEMP: For testing
    Muon::Mesh mTriangle;
    Muon::Mesh mCube;

    Muon::UploadBuffer mWorldMatrixBuffer;
    Muon::UploadBuffer mLightBuffer;

    // Timer for the main game loop
    Muon::StepTimer mTimer;
};
#endif