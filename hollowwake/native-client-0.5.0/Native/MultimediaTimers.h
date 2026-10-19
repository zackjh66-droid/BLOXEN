#pragma once
#include <windows.h>
using HWTimeCallback=void(CALLBACK*)(UINT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR);
UINT WINAPI HWTimeSetEvent(UINT delay,UINT resolution,HWTimeCallback callback,DWORD_PTR user,UINT flags);
UINT WINAPI HWTimeKillEvent(UINT id);
void* ResolveMultimediaTimerImport(const char* dll,const char* name);
