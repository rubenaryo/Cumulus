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
}

#endif