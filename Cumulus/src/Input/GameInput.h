/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/11
Description : Interface for GameInput child class
----------------------------------------------*/
#ifndef GAMEINPUT_H
#define GAMEINPUT_H

#include <Core/Camera.h>
#include "InputSystem.h"

namespace Input {
    
    class GameInput : public InputSystem
    {
    public:
        // Default constructor
        GameInput();
        ~GameInput();

        // Actions run by the input system each frame
        void Frame(float dt, Muon::Camera* pCamera);
    
    protected:
        // Override implementation for setting default key mappings
        virtual void SetDefaultKeyMap() override;
    };
}
#endif