/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Utility functions
----------------------------------------------*/
#ifndef MUON_UTILS_H
#define MUON_UTILS_H

#include <Core/Core.h>
#include <Core/CommonTypes.h>
#include <string>
#include <algorithm>

namespace Muon
{
	void Print(const char* str);
	void Print(const wchar_t* str);
	void Printf(const char* format, ...);
	void Printf(const wchar_t* format, ...);

	std::string FromWideStr(const std::wstring& wstr);
	std::wstring FromStr(const std::string& str);

	ResourceID GetResourceID(const wchar_t* resName);

	// Aligns a size or memory offset to be a multiple of alignment
	UINT AlignToBoundary(UINT size, UINT alignment);

	float SrgbChannelToLinear(float c); 
	DirectX::XMFLOAT3 SrgbToLinear3(const DirectX::XMFLOAT3 c);
	DirectX::XMFLOAT3 SrgbToLinear3(const float c[3]);
	DirectX::XMFLOAT3 SrgbToLinear3(float r, float g, float b); 

	float LinearChannelToSrgb(float c);
	DirectX::XMFLOAT3 LinearToSrgb3(const DirectX::XMFLOAT3 c);
	DirectX::XMFLOAT3 LinearToSrgb3(const float c[3]);
	DirectX::XMFLOAT3 LinearToSrgb3(float r, float g, float b);

	// Credit: https://bottosson.github.io/posts/oklab/
	DirectX::XMFLOAT3 OkLabToSrgb3(const DirectX::XMFLOAT3 okLab); 
	DirectX::XMFLOAT3 SrgbToOkLab3(const DirectX::XMFLOAT3 srgb);

	inline float SmoothStep(float edge0, float edge1, float x)
	{
		float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	inline float Lerp(float x, float y, float t)
	{
		return (1 - t) * x + t * y;
	}
}

#endif