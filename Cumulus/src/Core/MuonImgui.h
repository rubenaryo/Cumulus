/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Helpers for initializing/using ImGui
----------------------------------------------*/
#ifndef MUONIMGUI_H
#define MUONIMGUI_H

namespace Muon
{
	void ImguiInit();
	void ImguiShutdown();

	void ImguiNewFrame();
	void ImguiRender();
}

#endif