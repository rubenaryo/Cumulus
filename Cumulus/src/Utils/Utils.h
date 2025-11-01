/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Utility functions
----------------------------------------------*/
#ifndef MUON_UTILS_H
#define MUON_UTILS_H

#include <Core/Core.h>

namespace Muon
{
	void Print(const char* str);
	void Print(const wchar_t* str);
	void Printf(const char* format, ...);
	void Printf(const wchar_t* format, ...);

	// Aligns a size or memory offset to be a multiple of alignment
	UINT AlignToBoundary(UINT size, UINT alignment);
}

#endif