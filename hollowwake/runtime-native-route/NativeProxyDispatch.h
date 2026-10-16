#pragma once
namespace Hollowwake {
void ConfigurePackagedProxyResolver(void*(*explicitRoute)(const char*,const char*),void(*missing)(const char*,const char*));
}
extern "C" __declspec(dllexport) void* __cdecl HWResolvePackagedImport(const char*,const char*);
