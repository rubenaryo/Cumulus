/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/10
Description : Implementation for structures defined in InputBinding.h
----------------------------------------------*/
#include "InputBinding.h"
#include <vector>
#include <string>

namespace Input {
    // Default Binding Constructor
    Binding::Binding() :
        mKeyCode(0),
        mKeyState(KeyState::JustReleased)
    {};

    // Create Binding from keycode and keystate
    Binding::Binding(const unsigned int keyCode, const KeyState keyState) :
        mKeyCode(keyCode),
        mKeyState(keyState)
    {};

    // Default Chord Constructor
    Chord::Chord() :
        mName(L""),
        mChord(0)
    {};

    // Create Chord from Name, create binding from keycode and keystate and add it to Chord structure
    Chord::Chord(const std::wstring& name, const unsigned int keyCode, const KeyState keyState) :
        mName(name)
    {
        mChord.push_back(Binding(keyCode, keyState));
    }

    // Create Chord from Name and Binding&
    Chord::Chord(const std::wstring& name, const Binding& binding) :
        mName(name)
    {
        mChord.push_back(binding);
    }

    // Create Chord from Name and vector of Bindings
    Chord::Chord(const std::wstring& name, const std::vector<Binding>& bindingList) :
        mName(name),
        mChord(bindingList)
    {};
}
