/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2024/5
Description : Game window header
----------------------------------------------*/
#ifndef MUON_GAMEWINDOW_H
#define MUON_GAMEWINDOW_H

#include "AppWindow.h"

// A derived GameWindow class
class GameWindow : public BaseWindow<GameWindow>
{
public:
    GameWindow();
    virtual ~GameWindow();

    // Overriden pure virtual functions from BaseWindow
    PCWSTR ClassName() const final { return L"Game Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) final;
    bool InitGame(HWND hwnd, int width, int height) override final;
    void RunGame();

private:
    // States of the window (used to send messages to directx on resize, move, etc)
    bool m_ResizeMove = false;
    bool m_Suspended = false;
    bool m_Minimized = false;
    bool m_Fullscreen = false;

    Game m_Game;
};
#endif