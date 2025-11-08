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
}