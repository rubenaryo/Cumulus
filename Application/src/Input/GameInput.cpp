/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/11
Description : Implementation of GameInput method overrides
----------------------------------------------*/
#include <Core/WinApp.h>

#include "InputSystem.h"
#include "GameInput.h"

namespace Input {

    GameInput::GameInput()
    {
        SetDefaultKeyMap();
    }

    GameInput::~GameInput()
    {}

    void GameInput::Frame(float dt, Muon::Camera* pCamera)
    {
        using namespace DirectX;

        static const float kSpeed = 5.0f;

        // Act on user input:
        // - Iterate through all active keys
        // - Check for commands corresponding to activated chords
        // - Do something based on those commands
        for (auto const& pair : mActiveKeyMap)
        {
            switch (pair.first)
            {
            case GameCommands::Quit:
                PostQuitMessage(0);
                break;
            case GameCommands::MoveForward:
                pCamera->MoveForward(kSpeed * dt);
                break;
            case GameCommands::MoveBackward:
                pCamera->MoveForward(-kSpeed * dt);
                break;
            case GameCommands::MoveLeft:
                pCamera->MoveRight(-kSpeed * dt);
                break;
            case GameCommands::MoveRight:
                pCamera->MoveRight(kSpeed * dt);
                break;
            case GameCommands::MoveUp:
                pCamera->MoveUp(kSpeed * dt);
                break;
            case GameCommands::MoveDown:
                pCamera->MoveUp(-kSpeed * dt);
                break;
            case GameCommands::RollLeft:
                pCamera->Rotate(XMQuaternionRotationAxis(pCamera->mForward, kSpeed*dt));
                break;
            case GameCommands::RollRight:
                pCamera->Rotate(XMQuaternionRotationAxis(pCamera->mForward, -kSpeed * dt));
                break;
            case GameCommands::MouseRotation:
            {
                std::pair<float, float> delta = this->GetMouseDelta();

                const float mouseSensitivity = pCamera->GetSensitivity();

                const float yaw  = delta.first * mouseSensitivity * dt;
                const float pitch = delta.second * mouseSensitivity * dt;

                // Always yaw around the WORLD UP (not the camera’s current up)
                XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                XMVECTOR yawQuat = XMQuaternionRotationAxis(worldUp, yaw);

                // Pitch around the camera’s local right vector
                XMVECTOR pitchQuat = XMQuaternionRotationAxis(pCamera->mRight, pitch);

                // Apply yaw, then pitch
                XMVECTOR combinedQuat = XMQuaternionMultiply(pitchQuat, yawQuat);
                pCamera->Rotate(combinedQuat);

                break;
            }
            case GameCommands::MouseMovement:
            {
                std::pair<float, float> delta = this->GetMouseDelta();

                const float mouseSensitivity = pCamera->GetSensitivity();

                const float horizontal = delta.first * kSpeed * dt;
                const float vertical = delta.second * kSpeed * dt;

                pCamera->MoveRight(horizontal);
                pCamera->MoveUp(vertical);

                break;
            }
            }
        }
        
        GetInput();
    }

    void GameInput::SetDefaultKeyMap()
    {
        mKeyMap.clear();
        mKeyMap[GameCommands::Quit]         = new Chord(L"Quit", VK_ESCAPE, KeyState::JustReleased);
        mKeyMap[GameCommands::MoveForward]  = new Chord(L"Move Forward", 'W', KeyState::StillPressed);
        mKeyMap[GameCommands::MoveBackward] = new Chord(L"Move Backward", 'S', KeyState::StillPressed);
        mKeyMap[GameCommands::MoveLeft]     = new Chord(L"Move Left", 'A', KeyState::StillPressed);
        mKeyMap[GameCommands::MoveRight]    = new Chord(L"Move Right", 'D', KeyState::StillPressed);
        mKeyMap[GameCommands::MoveUp]       = new Chord(L"Move Up", 'Q', KeyState::StillPressed);
        mKeyMap[GameCommands::MoveDown]     = new Chord(L"Move Down", 'E', KeyState::StillPressed);
        //mKeyMap[GameCommands::RollLeft]     = new Chord(L"Roll Left", 'Q', KeyState::StillPressed);
        //mKeyMap[GameCommands::RollRight]    = new Chord(L"Roll Right", 'E', KeyState::StillPressed);

        mKeyMap[GameCommands::MouseRotation] = new Chord(L"Mouse Rotation", VK_LBUTTON, KeyState::StillPressed);
        mKeyMap[GameCommands::MouseMovement] = new Chord(L"Mouse Movement", VK_RBUTTON, KeyState::StillPressed);
    }
}
