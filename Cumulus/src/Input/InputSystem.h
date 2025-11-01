/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/10
Description : Interface for the InputSystem class
----------------------------------------------*/
#ifndef INPUTSYSTEM_H
#define INPUTSYSTEM_H

#include <Core/WinApp.h>
#include <array>
#include <unordered_map>
#include "InputBinding.h"

namespace Input {
    
    class InputSystem
    {
    private:
        // Keyboard States
        std::array<BYTE, 256> mKeyboardCurrent;
        std::array<BYTE, 256> mKeyboardPrevious;

        // Mouse States
        POINT mMouseCurrent;
        POINT mMousePrevious;

        // Uses GetAsyncKeyState to read in 256 bytes
        void GetKeyboardState();

        // returns the state of the key in enum form
        const KeyState GetKeyboardKeyState(const unsigned int keyCode) const;

        // returns true if the key is down
        inline const bool isPressed(int keyCode) const
        {
            return (GetAsyncKeyState(keyCode) & 0x8000) ? 1 : 0;
        }

        void update();

    protected:
        std::unordered_map<GameCommands, Chord*> mKeyMap;

        virtual void SetDefaultKeyMap() = 0;

    public:
        InputSystem();
        virtual ~InputSystem();

        void Destroy();

        std::unordered_map<GameCommands, Chord*> mActiveKeyMap;

        // Main "Update method" for input system
        void GetInput();

        // On WM_MOUSEMOVE message, trigger this method
        void OnMouseMove(short newX, short newY);

        // Returns the current mouse position as a POINT
        POINT GetMousePosition() const { return mMouseCurrent; }

        // Returns the difference between current and previous as a std::pair
        std::pair<float, float> GetMouseDelta() const;

    };
}
#endif