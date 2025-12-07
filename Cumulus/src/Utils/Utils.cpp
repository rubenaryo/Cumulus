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

	DirectX::XMFLOAT3 OkLabToSrgb3(const DirectX::XMFLOAT3 okLab)
	{
		float l_ = okLab.x + 0.3963377774f * okLab.y + 0.2158037573f * okLab.z;
		float m_ = okLab.x - 0.1055613458f * okLab.y - 0.0638541728f * okLab.z;
		float s_ = okLab.x - 0.0894841775f * okLab.y - 1.2914855480f * okLab.z;

		float l = l_ * l_ * l_;
		float m = m_ * m_ * m_;
		float s = s_ * s_ * s_;

		DirectX::XMFLOAT3 linSrgb =  {
			+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
			-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
			-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
		};

		return LinearToSrgb3(linSrgb); 
	}

	DirectX::XMFLOAT3 SrgbToOkLab3(const DirectX::XMFLOAT3 srgb)
	{
		DirectX::XMFLOAT3 c = SrgbToLinear3(srgb); 

		float l = 0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z;
		float m = 0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z;
		float s = 0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z;

		float l_ = cbrtf(l);
		float m_ = cbrtf(m);
		float s_ = cbrtf(s);

		return {
			0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
			1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
			0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_,
		};
	}

}