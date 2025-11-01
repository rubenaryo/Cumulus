/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/10
Description : Various class/struct definitions 
to be used by the Inputhandling InputSystem class
----------------------------------------------*/
#ifndef INPUTBINDING_H
#define INPUTBINDING_H

// Preset our windows includes
#include <Core/WinApp.h>

// std inclusions
#include <string>
#include <array>
#include <vector>

namespace Input {
    
    // Enumeration of different GameCommands
    enum class GameCommands
    {
        Quit,
        MoveForward,
        MoveBackward,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        RollLeft,
        RollRight,
        MouseRotation,
        MouseMovement
    };

    // Enum to emphasize the different states of a key
    enum class KeyState
    {
        StillReleased,
        JustPressed,
        StillPressed,
        JustReleased
    };

    // Wrapping struct keycode and above enum
    struct Binding
    {
    private:
        unsigned int mKeyCode;         // Windows Keycode
        KeyState mKeyState;            // Associated keystate

    public:
        Binding();
        Binding(const unsigned int keyCode, const KeyState keyState);
        ~Binding() {};

        friend class InputSystem;
    };

    // Maps a game command to a Binding
    struct Chord
    {
    private:
        std::wstring mName;            // Human readable command name
        std::vector<Binding> mChord;   // Sequence of keys mapped to "m_Name" command

    public:
        Chord();
        Chord(const std::wstring& name, const unsigned int keyCode, const KeyState keyState);
        Chord(const std::wstring& name, const Binding& chord);
        Chord(const std::wstring& name, const std::vector<Binding>& chords);
        ~Chord() {};

        // Accessors for member variables
        std::vector<Binding>& GetChord() { return mChord; }
        std::wstring&         GetName()  { return mName;  }
    };
}

#endif