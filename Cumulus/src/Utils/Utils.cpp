/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Utility functions
----------------------------------------------*/

#include <Utils/Utils.h>
#include <Utils/HashUtils.h>
#include <stdio.h>
#include <locale>
#include <codecvt>
#include <algorithm>

namespace Muon
{
	void Print(const char* str)
	{
		OutputDebugStringA(str);
	}

	void Print(const wchar_t* str)
	{
		OutputDebugString(str);
	}

	void Printf(const char* format, ...)
	{
		char buffer[256];
		va_list ap;
		va_start(ap, format);
		vsprintf_s(buffer, 256, format, ap);
		va_end(ap);
		Print(buffer);
	}

	void Printf(const wchar_t* format, ...)
	{
		wchar_t buffer[256];
		va_list ap;
		va_start(ap, format);
		vswprintf(buffer, 256, format, ap);
		va_end(ap);
		Print(buffer);
	}

	std::string FromWideStr(const std::wstring& wstr)
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
		return conv.to_bytes(wstr);
	}

	std::wstring FromStr(const std::string& str)
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
		return conv.from_bytes(str);
	}

	// Central function to generate resourceIDs given a name.
	ResourceID GetResourceID(const wchar_t* resName)
	{
		return (ResourceID)fnv1a(resName);
	}

	UINT AlignToBoundary(UINT size, UINT alignment)
	{
		return (size + (alignment - 1)) & ~(alignment - 1);
	}
	float SrgbChannelToLinear(float c)
	{
		if (c <= 0.04045f)
			return c / 12.92f;
		return std::pow((c + 0.055f) / 1.055f, 2.4f);
	}

	DirectX::XMFLOAT3 SrgbToLinear3(const DirectX::XMFLOAT3 c)
	{
		return DirectX::XMFLOAT3(
			SrgbChannelToLinear(c.x),
			SrgbChannelToLinear(c.y),
			SrgbChannelToLinear(c.z)
		);
	}

	DirectX::XMFLOAT3 SrgbToLinear3(const float c[3])
	{
		return DirectX::XMFLOAT3(
			SrgbChannelToLinear(c[0]),
			SrgbChannelToLinear(c[1]),
			SrgbChannelToLinear(c[2])
		);
	}

	DirectX::XMFLOAT3 SrgbToLinear3(float r, float g, float b)
	{
		return DirectX::XMFLOAT3(
			SrgbChannelToLinear(r),
			SrgbChannelToLinear(g),
			SrgbChannelToLinear(b)
		);
	}

	float LinearChannelToSrgb(float c)
	{
		c = std::clamp(c, 0.0f, 1.0f);
		if (c <= 0.0031308f)
			return 12.92f * c;
		return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
	}

	DirectX::XMFLOAT3 LinearToSrgb3(const DirectX::XMFLOAT3 c)
	{
		return DirectX::XMFLOAT3(
			LinearChannelToSrgb(c.x),
			LinearChannelToSrgb(c.y),
			LinearChannelToSrgb(c.z)
		);
	}

	DirectX::XMFLOAT3 LinearToSrgb3(const float c[3])
	{
		return DirectX::XMFLOAT3(
			LinearChannelToSrgb(c[0]),
			LinearChannelToSrgb(c[1]),
			LinearChannelToSrgb(c[2])
		);
	}

	DirectX::XMFLOAT3 LinearToSrgb3(float r, float g, float b)
	{
		return DirectX::XMFLOAT3(
			LinearChannelToSrgb(r),
			LinearChannelToSrgb(g),
			LinearChannelToSrgb(b)
		);
	}

}