/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/10
Description : InputSystem method definitions
----------------------------------------------*/
#include "InputSystem.h"

namespace Input {

    // Init all keyboard states to 0
    InputSystem::InputSystem()
    {
        mKeyboardCurrent.fill(0);
        mKeyboardPrevious.fill(0);
        mMousePrevious.x = 0;
        mMousePrevious.y = 0;
        mMouseCurrent.x = 0;
        mMouseCurrent.y = 0;
    }

    // Release all dynamic memory
    InputSystem::~InputSystem()
    {
    }

    void InputSystem::Destroy()
    {
        // No need to delete dynamic memory from activeKeyMap, it's already deleted in keymap*
        mActiveKeyMap.clear();

        // Release keyMap and clear
        for (auto pair : mKeyMap)
        {
            if (pair.second)
                delete pair.second;
        }
        mKeyMap.clear();
    }

    // Stores previous/current keymappings (from windows)
    // Updates active keymap by comparing all chords 
    // Updates mouse mapping
    void InputSystem::GetInput()
    {
        // Get the keyboard state from windows
        GetKeyboardState();

        // Update active/non-active keymaps
        update();

        // Update mouse state
        mMousePrevious = mMouseCurrent;
    }

    void InputSystem::OnMouseMove(short newX, short newY)
    {
        mMouseCurrent = { newX, newY };
    }

    std::pair<float,float> InputSystem::GetMouseDelta() const
    {
        std::pair<float, float> pt;
        
        pt.first  = static_cast<float>(mMouseCurrent.x - mMousePrevious.x);
        pt.second = static_cast<float>(mMouseCurrent.y - mMousePrevious.y);
        return pt;
    }

    // Clears active key map, then fills it with all values from m_keyMap with a 'fulfilled' chord
    void InputSystem::update()
    {
        // Reset active key map
        mActiveKeyMap.clear();

        // Map which keys are active into the active key map
        for (auto key : mKeyMap)
        {
            bool activeKey = true;

            // Test Chord
            for (Binding binding : key.second->GetChord())
            {
                if (GetKeyboardKeyState(binding.mKeyCode) != binding.mKeyState)
                {
                    activeKey = false;
                    break;
                }
            }

            // Passed Chord Check : move key to active key map
            if (activeKey)
                mActiveKeyMap.insert(key);
        }
    }

    // Stores the current state of the keyboard as 'previous'
    // then reads in new 'current values from windows
    void InputSystem::GetKeyboardState()
    {
        mKeyboardPrevious = mKeyboardCurrent;

        for (int i = 0; i < 256; i++)
            mKeyboardCurrent[i] = isPressed(i);
    }

    // Use logic to deduce Keystate from current and previous keyboard states
    const KeyState InputSystem::GetKeyboardKeyState(const unsigned int keyCode) const
    {
        if (mKeyboardPrevious[keyCode] == 1)
            if (mKeyboardCurrent[keyCode] == 1)
                return KeyState::StillPressed;  // true, true
            else
                return KeyState::JustReleased;  // true, false
        else
            if (mKeyboardCurrent[keyCode] == 1)
                return KeyState::JustPressed;   // false, true
            else
                return KeyState::StillReleased; // false, false
    }
}