/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/10
Description : This file contains the main function (entry point) for the application
----------------------------------------------*/

#include <Core/AppWindow.h>
#include <Core/GameWindow.h>

#if defined(MN_DEBUG)
#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h> 
#include <dxgidebug.h>
#endif

// Entry point for the application
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // On Debug Builds: Enable simple runtime memory check
#if defined(MN_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
#endif 

    {
        // Create the window
        GameWindow window;
        DWORD style = CS_HREDRAW | CS_VREDRAW;
        DWORD ExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        if (!window.Create(L"Application", hInstance, style, ExStyle, 0L, 0L, 1280L, 800L, 0, 0))
            exit(EXIT_FAILURE);
    
        // Show the window
        ShowWindow(window.Window(), nCmdShow);
    
        // Run the game
        window.RunGame();
    }

    // Dump any found memory leaks
#if defined(MN_DEBUG)
    _CrtDumpMemoryLeaks();
#endif

    exit(EXIT_SUCCESS);
}