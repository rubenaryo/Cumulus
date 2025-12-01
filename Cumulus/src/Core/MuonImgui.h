/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Helpers for initializing/using ImGui
----------------------------------------------*/
#ifndef MUONIMGUI_H
#define MUONIMGUI_H

#include <Core/WinApp.h>
#include "Camera.h"

namespace Muon
{
	struct SceneSettings {
		bool isSunDynamic = false;
		bool drawObjects = true;
		int timeOfDay = 800; // stored as military time for now
		DirectX::XMFLOAT3 sunDir;
		int numClouds = 4;
		float cloudScale = 1.0;
		bool updateClouds = false;
	};

	bool ImguiInit();
	bool ImguiInitWin32(HWND hwnd);
	void ImguiShutdown();

	void ImguiNewFrame(float gameTime, const Camera& cam, SceneSettings &settings);
	void ImguiRender();
}

#endif