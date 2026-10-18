#pragma once
// Only two read-only WinHTTP proxy-configuration queries. No HTTP sessions,
// outbound request creation, endpoint bypass or account/login implementation.
void* ResolveReadOnlyPlatformImport(const char* dll,const char* symbol);
