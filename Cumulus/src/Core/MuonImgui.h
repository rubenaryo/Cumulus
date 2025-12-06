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
	struct AtmosphereInput
	{
		bool isSunDynamic = false;
		float timeScale = 1.f;
		DirectX::XMFLOAT3 sunDir = {0.0f, 0.9f, 0.1f};
		float sunSize = 1.f;
		float viewport_width = 1280.f;
		float viewport_height = 800.f;
		float view_zenith_angle_radians = 1.47f;
		float view_azimuth_angle_radians = -0.1f;
	};

	struct CloudInput
	{
		int numClouds = 4;
		float cloudScale = 1.0f;
		bool updateClouds = false;
		float windScale = 1.0f;
		DirectX::XMFLOAT2 windDir = { 1.0f, 0.0f };
	};

	struct SceneSettings {
		bool drawObjects = true;
		AtmosphereInput atmosphere;
		CloudInput clouds;
		cbCloudLighting lighting;
		bool updateLighting = false;
	};

	bool ImguiInit();
	bool ImguiInitWin32(HWND hwnd);
	void ImguiShutdown();

	void ImguiNewFrame(float gameTime, const Camera& cam, SceneSettings &settings);
	void ImguiRender();
}

#endif