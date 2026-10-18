#pragma once
// Actual OS proxy/keyboard-layout queries and native icon/cursor resources.
// File resources go through the existing game-content namespace. No HTTP request
// creation, virtual HWND passthrough, input injection or login implementation.
void* ResolveReadOnlyPlatformImport(const char* dll,const char* symbol);
