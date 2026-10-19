#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <fstream>
#include "../Native/DataFiles.h"
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
 auto station=reinterpret_cast<decltype(&GetProcessWindowStation)>(ResolveReadOnlyPlatformImport("user32.dll","GetProcessWindowStation"));
 auto objectInfo=reinterpret_cast<decltype(&GetUserObjectInformationW)>(ResolveReadOnlyPlatformImport("user32.dll","GetUserObjectInformationW"));
 if(!station||!objectInfo)throw std::runtime_error("Native window-station queries unavailable");
 auto ownStation=station();if(!ownStation)throw std::runtime_error("Native process has no window station");
 DWORD needed=0;SetLastError(0);if(objectInfo(ownStation,UOI_FLAGS,nullptr,0,&needed)||GetLastError()!=ERROR_INSUFFICIENT_BUFFER||needed!=sizeof(USEROBJECTFLAGS))throw std::runtime_error("Window station buffer-size/error contract failed");
 USEROBJECTFLAGS flags={};if(!objectInfo(ownStation,UOI_FLAGS,&flags,sizeof(flags),&needed))throw std::runtime_error("Real window-station flags query failed");
 // No station name, account name, SID or other process data is requested/logged.
 if(ResolveReadOnlyPlatformImport("user32.dll","SendInput")||ResolveReadOnlyPlatformImport("user32.dll","SetWindowsHookExW"))throw std::runtime_error("Read-only adapter exposes input injection");
 auto ghosting=reinterpret_cast<void(WINAPI*)()>(ResolveReadOnlyPlatformImport("USER32.dll","DisableProcessWindowsGhosting"));
 auto nativeUser=GetModuleHandleW(L"user32.dll");if(!ghosting||!nativeUser||(void*)ghosting!=(void*)GetProcAddress(nativeUser,"DisableProcessWindowsGhosting"))throw std::runtime_error("Window initialization setting is not actual OS export");
 ghosting(); // Real own-process setting; no intentionally hung window/test process.
 // Exercise the production resolver with actual GDI resources, not dummy handles.
 auto iconCreate=reinterpret_cast<decltype(&CreateIconIndirect)>(ResolveReadOnlyPlatformImport("USER32.dll","CreateIconIndirect"));
 auto iconDestroy=reinterpret_cast<decltype(&DestroyIcon)>(ResolveReadOnlyPlatformImport("USER32.dll","DestroyIcon"));
 auto cursorDestroy=reinterpret_cast<decltype(&DestroyCursor)>(ResolveReadOnlyPlatformImport("USER32.dll","DestroyCursor"));
 auto iconLoad=reinterpret_cast<decltype(&LoadIconW)>(ResolveReadOnlyPlatformImport("USER32.dll","LoadIconW"));
 auto draw=reinterpret_cast<decltype(&DrawStateW)>(ResolveReadOnlyPlatformImport("USER32.dll","DrawStateW"));
 auto info=reinterpret_cast<decltype(&GetIconInfo)>(ResolveReadOnlyPlatformImport("USER32.dll","GetIconInfo"));
 if(!iconCreate||!iconDestroy||!cursorDestroy||!iconLoad||!draw||!info)throw std::runtime_error("Native resource provider unavailable");
 BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=16;bi.bmiHeader.biHeight=-16;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 void* pixels=nullptr;auto color=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);if(!color||!pixels)throw std::runtime_error("Actual icon bitmap allocation failed");
 std::fill_n((DWORD*)pixels,256,0xffff0000u);unsigned char maskBits[32]={};auto mask=CreateBitmap(16,16,1,1,maskBits);if(!mask)throw std::runtime_error("Actual mask allocation failed");
 ICONINFO image={};image.fIcon=TRUE;image.hbmMask=mask;image.hbmColor=color;auto icon=iconCreate(&image);if(!icon)throw std::runtime_error("Actual icon creation failed");
 auto dc=CreateCompatibleDC(nullptr);void* output=nullptr;auto target=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&output,nullptr,0);if(!dc||!target||!output)throw std::runtime_error("Actual draw target failed");auto prior=SelectObject(dc,target);std::fill_n((DWORD*)output,256,0xff00ff00u);
 if(!draw(dc,nullptr,nullptr,(LPARAM)icon,0,0,0,16,16,DST_ICON))throw std::runtime_error("Native icon draw failed");GdiFlush();if((((DWORD*)output)[8*16+8]&0x00ffffff)!=0x00ff0000)throw std::runtime_error("Native icon pixel readback differs");
 ICONINFO obtained={};if(!info(icon,&obtained)||!obtained.fIcon||!obtained.hbmColor||!obtained.hbmMask)throw std::runtime_error("Native icon metadata failed");DeleteObject(obtained.hbmColor);DeleteObject(obtained.hbmMask);if(!iconDestroy(icon))throw std::runtime_error("Native icon release failed");
 image.fIcon=FALSE;image.xHotspot=3;image.yHotspot=5;auto cursor=(HCURSOR)iconCreate(&image);if(!cursor||!info((HICON)cursor,&obtained)||obtained.fIcon||obtained.xHotspot!=3||obtained.yHotspot!=5)throw std::runtime_error("Actual custom cursor metadata failed");DeleteObject(obtained.hbmColor);DeleteObject(obtained.hbmMask);if(!cursorDestroy(cursor))throw std::runtime_error("Actual custom cursor release failed");
 if(!iconLoad(nullptr,MAKEINTRESOURCEW(32512)))throw std::runtime_error("Native shared icon resource unavailable");
 auto imageLoad=reinterpret_cast<decltype(&LoadImageW)>(ResolveReadOnlyPlatformImport("USER32.dll","LoadImageW"));
 auto cursorLoad=reinterpret_cast<decltype(&LoadCursorFromFileW)>(ResolveReadOnlyPlatformImport("USER32.dll","LoadCursorFromFileW"));
 if(!imageLoad||!cursorLoad)throw std::runtime_error("Content resource loader unavailable");
 wchar_t temp[MAX_PATH],unique[MAX_PATH];if(!GetTempPathW(MAX_PATH,temp)||!GetTempFileNameW(temp,L"hwi",0,unique))throw std::runtime_error("Owned fixture directory failed");DeleteFileW(unique);
 namespace fs=std::filesystem;auto root=fs::path(unique),package=root/L"package",local=root/L"data",content=local/L"6.21"/L"FortniteGame"/L"Content";fs::create_directories(content);fs::create_directories(package);
 if(!ConfigureDataFiles(package.c_str(),local.c_str()))throw std::runtime_error("Production content namespace setup failed");
 BITMAPFILEHEADER fileHeader={};fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(BITMAPINFOHEADER);fileHeader.bfSize=fileHeader.bfOffBits+1024;
 {std::ofstream bmp(content/L"fixture.bmp",std::ios::binary);bmp.write((char*)&fileHeader,sizeof(fileHeader));bmp.write((char*)&bi.bmiHeader,sizeof(BITMAPINFOHEADER));bmp.write((char*)pixels,1024);if(!bmp)throw std::runtime_error("Original BMP fixture write failed");}
 auto loaded=(HBITMAP)imageLoad(nullptr,L"..\\..\\Content\\fixture.bmp",IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);BITMAP bitmap={};if(!loaded||!GetObjectW(loaded,sizeof(bitmap),&bitmap)||bitmap.bmWidth!=16||bitmap.bmHeight!=16||!bitmap.bmBits||(*(DWORD*)bitmap.bmBits&0x00ffffff)!=0x00ff0000)throw std::runtime_error("Actual redirected BMP decode/pixels failed");DeleteObject(loaded);
 // A small original CUR fixture: one 32-bit icon, explicit hotspot and AND mask.
 {std::ofstream cur(content/L"fixture.cur",std::ios::binary);WORD directory[3]={0,2,1};cur.write((char*)directory,sizeof(directory));BYTE dimensions[4]={16,16,0,0};cur.write((char*)dimensions,4);WORD hotspot[2]={3,5};cur.write((char*)hotspot,4);DWORD payload=40+1024+64,offset=22;cur.write((char*)&payload,4);cur.write((char*)&offset,4);auto header=bi.bmiHeader;header.biHeight=32;cur.write((char*)&header,40);cur.write((char*)pixels,1024);char andMask[64]={};cur.write(andMask,64);if(!cur)throw std::runtime_error("Original CUR fixture write failed");}
 auto fileCursor=cursorLoad(L"..\\..\\Content\\fixture.cur");if(!fileCursor||!info((HICON)fileCursor,&obtained)||obtained.fIcon )throw std::runtime_error("Actual redirected cursor decode failed");
 BITMAP cursorBitmap={};if(!GetObjectW(obtained.hbmColor,sizeof(cursorBitmap),&cursorBitmap)||cursorBitmap.bmWidth<=0||cursorBitmap.bmHeight<=0||obtained.xHotspot!=3*cursorBitmap.bmWidth/16||obtained.yHotspot!=5*cursorBitmap.bmHeight/16)throw std::runtime_error("Default-size cursor scaling/hotspot mismatch");DeleteObject(obtained.hbmColor);DeleteObject(obtained.hbmMask);cursorDestroy(fileCursor);
 SetLastError(0);if(imageLoad(nullptr,(root/L"outside.bmp").c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE)||GetLastError()!=ERROR_ACCESS_DENIED)throw std::runtime_error("Outside image path accepted");
 SetLastError(0);if(cursorLoad(L"..\\..\\Content\\untrusted.dll")||GetLastError()!=ERROR_ACCESS_DENIED)throw std::runtime_error("Code payload passed to image decoder");
 auto fallbackCursor=LoadGameCursorWithProviders(L"..\\..\\Content\\fixture.cur",nullptr,&LoadImageW);
 if(!fallbackCursor||!info((HICON)fallbackCursor,&obtained)||obtained.fIcon)throw std::runtime_error("Documented cursor-file replacement failed actual decode");
 BITMAP fallbackBitmap={};if(!GetObjectW(obtained.hbmColor,sizeof(fallbackBitmap),&fallbackBitmap)||fallbackBitmap.bmWidth<=0||obtained.xHotspot!=3*fallbackBitmap.bmWidth/16||obtained.yHotspot!=5*fallbackBitmap.bmHeight/16)throw std::runtime_error("Replacement cursor hotspot differs");
 DeleteObject(obtained.hbmColor);DeleteObject(obtained.hbmMask);if(!cursorDestroy(fallbackCursor))throw std::runtime_error("Replacement cursor cleanup failed");
 SetLastError(0);if(LoadGameCursorWithProviders(L"..\\..\\Content\\missing.cur",nullptr,&LoadImageW)||GetLastError()!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("Replacement swallowed missing-file failure");
 SetLastError(0);if(LoadGameCursorWithProviders(L"..\\..\\Content\\fixture.cur",nullptr,nullptr)||GetLastError()!=ERROR_PROC_NOT_FOUND)throw std::runtime_error("Missing cursor providers became success");
 SetLastError(0);if(LoadGameCursorWithProviders(L"..\\..\\Content\\untrusted.dll",nullptr,&LoadImageW)||GetLastError()!=ERROR_ACCESS_DENIED)throw std::runtime_error("Replacement admitted executable input");
 std::cout<<"PASS genuine LoadImageW cursor replacement when LoadCursorFromFileW is unavailable; actual dimensions/hotspots, cleanup, missing-file/provider and content-path errors. Xbox provider availability unverified.\n";
 fs::remove_all(root);
 std::cout<<"PASS real BMP/CUR decoding through package-to-content path mapping, bitmap pixel/hotspot checks, outside-path/code rejection.\n";
 SelectObject(dc,prior);DeleteObject(target);DeleteDC(dc);DeleteObject(color);DeleteObject(mask);
 if(ResolveReadOnlyPlatformImport("USER32.dll","GetDC")||ResolveReadOnlyPlatformImport("USER32.dll","BeginPaint"))throw std::runtime_error("Virtual window/file namespaces bypassed");
 std::cout<<"PASS native icon/cursor create, metadata, release, shared icon load, actual DrawState icon pixel readback; virtual-window and content-path APIs remain blocked.\n";
 std::cout<<"PASS real native window-station object/buffer/flags query; actual host keyboard layout/scancode round trips and input-injection rejection; Windows WinHTTP configuration queries, exact native addresses/allocations/error paths and rejection of outbound request APIs. No proxy values logged; no game/Xbox execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}}
