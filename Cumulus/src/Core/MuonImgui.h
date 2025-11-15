/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Helpers for initializing/using ImGui
----------------------------------------------*/
#ifndef MUONIMGUI_H
#define MUONIMGUI_H

#include <Core/WinApp.h>

namespace Muon
{
	bool ImguiInit();
	bool ImguiInitWin32(HWND hwnd);
	void ImguiShutdown();

	void ImguiNewFrame();
	void ImguiRender();
}

#endif