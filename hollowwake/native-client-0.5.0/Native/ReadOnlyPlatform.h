#pragma once
// Actual OS read-only proxy/keyboard-layout queries. No HTTP request creation,
// desktop HWND passthrough, input injection or account/login implementation.
void* ResolveReadOnlyPlatformImport(const char* dll,const char* symbol);
