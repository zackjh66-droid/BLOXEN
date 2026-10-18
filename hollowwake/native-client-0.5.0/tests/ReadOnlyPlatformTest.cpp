#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <stdexcept>
#include "../Native/ReadOnlyPlatform.h"
#include "../../runtime-native-route/NativeSystem.h"
int main(){try{
 auto query=reinterpret_cast<decltype(&WinHttpGetDefaultProxyConfiguration)>(ResolveReadOnlyPlatformImport("winhttp.dll","WinHttpGetDefaultProxyConfiguration"));
 auto ie=reinterpret_cast<decltype(&WinHttpGetIEProxyConfigForCurrentUser)>(ResolveReadOnlyPlatformImport("WINHTTP.dll","WinHttpGetIEProxyConfigForCurrentUser"));
 if(!query||!ie)throw std::runtime_error("Actual system WinHTTP read-only provider unavailable");
 using Hold=BOOL(WINAPI*)(DWORD,LPCWSTR,HMODULE*);auto hold=reinterpret_cast<Hold>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetModuleHandleExW"));HMODULE module=nullptr;if(!hold||!hold(0,L"winhttp.dll",&module))throw std::runtime_error("Provider not retained");
 bool identical=(void*)query==(void*)GetProcAddress(module,"WinHttpGetDefaultProxyConfiguration")&&(void*)ie==(void*)GetProcAddress(module,"WinHttpGetIEProxyConfigForCurrentUser");FreeLibrary(module);if(!identical)throw std::runtime_error("Query does not use real OS provider");
 WINHTTP_PROXY_INFO proxy={};SetLastError(0);BOOL ok=query(&proxy);DWORD error=GetLastError();if(!ok&&!error)throw std::runtime_error("Failed default query lost OS error");if(ok&&proxy.dwAccessType!=WINHTTP_ACCESS_TYPE_DEFAULT_PROXY&&proxy.dwAccessType!=WINHTTP_ACCESS_TYPE_NO_PROXY&&proxy.dwAccessType!=WINHTTP_ACCESS_TYPE_NAMED_PROXY&&proxy.dwAccessType!=WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY)throw std::runtime_error("Invalid native proxy result");if(proxy.lpszProxy)GlobalFree(proxy.lpszProxy);if(proxy.lpszProxyBypass)GlobalFree(proxy.lpszProxyBypass);
 WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config={};SetLastError(0);ok=ie(&config);error=GetLastError();if(!ok&&!error)throw std::runtime_error("Failed user query lost OS error");if(config.lpszAutoConfigUrl)GlobalFree(config.lpszAutoConfigUrl);if(config.lpszProxy)GlobalFree(config.lpszProxy);if(config.lpszProxyBypass)GlobalFree(config.lpszProxyBypass);
 if(ResolveReadOnlyPlatformImport("winhttp.dll","WinHttpOpen")||ResolveReadOnlyPlatformImport("winhttp.dll","WinHttpSendRequest"))throw std::runtime_error("Read-only provider exposed outbound HTTP");

 auto layout=reinterpret_cast<decltype(&GetKeyboardLayout)>(ResolveReadOnlyPlatformImport("USER32.dll","GetKeyboardLayout"));auto map=reinterpret_cast<decltype(&MapVirtualKeyW)>(ResolveReadOnlyPlatformImport("user32.dll","MapVirtualKeyW"));
 if(!layout||!map||!layout(0))throw std::runtime_error("Actual host keyboard layout unavailable");
 for(UINT key: {UINT(VK_RETURN),UINT(VK_ESCAPE)}){auto scan=map(key,MAPVK_VK_TO_VSC);if(!scan||map(scan,MAPVK_VSC_TO_VK)!=key)throw std::runtime_error("Actual host virtual/scancode round trip failed");}
 if(ResolveReadOnlyPlatformImport("user32.dll","SendInput")||ResolveReadOnlyPlatformImport("user32.dll","SetWindowsHookExW"))throw std::runtime_error("Read-only adapter exposes input injection");
 std::cout<<"PASS actual host keyboard layout/scancode round trips and input-injection rejection; Windows WinHTTP configuration queries, exact native addresses/allocations/error paths and rejection of outbound request APIs. No proxy values logged; no game/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
