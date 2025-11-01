/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2019/10
Description : This header file modifies the inclusion of windows.h
by first defining macros that disable certain  features that go unused
----------------------------------------------*/
#ifndef WINAPP_H
#define WINAPP_H

// Target Windows 10 and later
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>

// Disable unused windows.h features
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
//#define NOMINMAX DEFINED IN VS CONFIG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE

// Always include Windows.h at the very end
#include <Windows.h>

#endif