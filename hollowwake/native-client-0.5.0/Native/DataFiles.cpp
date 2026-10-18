// Real file I/O, not a virtual success response. Installed executable images stay
// in the signed package. Only non-code game data maps to this app's LocalState.
#include "DataFiles.h"
#include "../../runtime-native-route/NativeSystem.h"
#include <fileapifromapp.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstring>
#include <cwchar>
#include <algorithm>
namespace {
std::recursive_mutex gate;std::wstring package,data,cwd;std::map<std::wstring,std::wstring> code;
std::wstring Fold(std::wstring value){for(auto& c:value){if(c==L'/')c=L'\\';if(c>=L'A'&&c<=L'Z')c+=32;}if(value.compare(0,4,L"\\\\?\\")==0)value=value.substr(4);return value;}
bool Under(const std::wstring& path,const std::wstring& root){return !root.empty()&&(path==root||path.compare(0,root.size()+1,root+L"\\")==0);}
std::wstring Absolute(const wchar_t* value){if(!value||wcsnlen_s(value,32768)==32768){SetLastError(ERROR_INVALID_PARAMETER);return {};}
 std::wstring input=Fold(value);if(input.empty()){SetLastError(ERROR_INVALID_NAME);return {};}
 if(input.size()>2&&input[1]==L':'&&input[2]!=L'\\'){SetLastError(ERROR_INVALID_NAME);return {};}
 if(!(input.size()>2&&input[1]==L':'&&input[2]==L'\\'))input=cwd+L"\\"+input;
 using Full=DWORD(WINAPI*)(LPCWSTR,DWORD,LPWSTR,LPWSTR*);auto full=reinterpret_cast<Full>(Hollowwake::ResolveNativeSystemImport("kernel32.dll","GetFullPathNameW"));if(!full){SetLastError(ERROR_PROC_NOT_FOUND);return {};}
 wchar_t output[32768];auto length=full(input.c_str(),32768,output,nullptr);if(!length||length>=32768){SetLastError(ERROR_FILENAME_EXCED_RANGE);return {};}return Fold(output);
}
bool Executable(const std::wstring& name){auto position=name.find_last_of(L'.');auto ext=position==std::wstring::npos?L"":name.substr(position);for(auto bad:{L".exe",L".dll",L".sys",L".com",L".bat",L".cmd",L".ps1",L".vbs",L".msi",L".appx",L".msix"})if(ext==bad)return true;return false;}
std::wstring Map(const wchar_t* name,bool writing){std::lock_guard<std::recursive_mutex> lock(gate);auto logical=Absolute(name);if(logical.empty())return {};
 auto exact=code.find(logical);if(exact!=code.end()){if(writing){SetLastError(ERROR_ACCESS_DENIED);return {};}return exact->second;}
 if(Executable(logical)){SetLastError(ERROR_ACCESS_DENIED);return {};}
 auto root=package+L"\\6.21";if(writing&&(Under(logical,root+L"\\fortnitegame\\content")||Under(logical,data+L"\\6.21\\fortnitegame\\content")||Under(logical,root+L"\\engine\\binaries\\thirdparty\\cef3\\win64")||Under(logical,data+L"\\6.21\\engine\\binaries\\thirdparty\\cef3\\win64"))){SetLastError(ERROR_ACCESS_DENIED);return {};}if(Under(logical,root)){auto target=data+L"\\6.21"+logical.substr(root.size());if(!writing){WIN32_FILE_ATTRIBUTE_DATA attrs={};if(!GetFileAttributesExFromAppW(target.c_str(),GetFileExInfoStandard,&attrs)&&GetFileAttributesExFromAppW(logical.c_str(),GetFileExInfoStandard,&attrs))return logical;}return target;}
 if(Under(logical,data+L"\\6.21"))return logical;
 SetLastError(ERROR_ACCESS_DENIED);return {};
}
HANDLE WINAPI Open(LPCWSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD attributes,HANDLE templateFile){bool writing=(access&(GENERIC_WRITE|GENERIC_ALL|DELETE|WRITE_DAC|WRITE_OWNER|FILE_WRITE_DATA|FILE_APPEND_DATA))||creation!=OPEN_EXISTING;auto path=Map(name,writing);if(path.empty())return INVALID_HANDLE_VALUE;
 CREATEFILE2_EXTENDED_PARAMETERS extra={};extra.dwSize=sizeof(extra);extra.dwFileAttributes=attributes&0xffff;extra.dwFileFlags=attributes&0xffe00000;extra.dwSecurityQosFlags=attributes&0x001f0000;extra.lpSecurityAttributes=security;extra.hTemplateFile=templateFile;
 return CreateFile2(path.c_str(),access,share,creation,&extra);
}
BOOL WINAPI AttributesEx(LPCWSTR name,GET_FILEEX_INFO_LEVELS level,LPVOID output){auto path=Map(name,false);return path.empty()?FALSE:GetFileAttributesExFromAppW(path.c_str(),level,output);}
DWORD WINAPI Attributes(LPCWSTR name){WIN32_FILE_ATTRIBUTE_DATA result={};return AttributesEx(name,GetFileExInfoStandard,&result)?result.dwFileAttributes:INVALID_FILE_ATTRIBUTES;}
HANDLE WINAPI Find(LPCWSTR name,FINDEX_INFO_LEVELS level,LPVOID output,FINDEX_SEARCH_OPS operation,LPVOID filter,DWORD flags){auto path=Map(name,false);return path.empty()?INVALID_HANDLE_VALUE:FindFirstFileExFromAppW(path.c_str(),level,output,operation,filter,flags);}
BOOL WINAPI Directory(LPCWSTR name,LPSECURITY_ATTRIBUTES security){auto path=Map(name,true);return !path.empty()&&CreateDirectoryFromAppW(path.c_str(),security);}
BOOL WINAPI Delete(LPCWSTR name){auto path=Map(name,true);return !path.empty()&&DeleteFileFromAppW(path.c_str());}
BOOL WINAPI RemoveDataDirectory(LPCWSTR name){auto path=Map(name,true);return !path.empty()&&RemoveDirectoryFromAppW(path.c_str());}
DWORD WINAPI Current(DWORD capacity,LPWSTR output){std::lock_guard<std::recursive_mutex> lock(gate);if(cwd.empty()){SetLastError(ERROR_PATH_NOT_FOUND);return 0;}DWORD needed=(DWORD)cwd.size()+1;if(capacity<needed)return needed;if(!output){SetLastError(ERROR_INVALID_PARAMETER);return 0;}std::wmemcpy(output,cwd.c_str(),needed);return needed-1;}
BOOL WINAPI SetCurrent(LPCWSTR name){std::lock_guard<std::recursive_mutex> lock(gate);auto logical=Absolute(name);if(!Under(logical,package+L"\\6.21")&&!Under(logical,data+L"\\6.21")){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}auto attr=Attributes(logical.c_str());if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY)){SetLastError(ERROR_PATH_NOT_FOUND);return FALSE;}cwd=logical;return TRUE;}
}
std::wstring MapReadOnlyImageFile(const wchar_t* name){
 std::lock_guard<std::recursive_mutex> lock(gate);auto logical=Absolute(name);if(logical.empty())return {};
 // A data decoder is not a second executable loader. Do not allow code aliases
 // even though ordinary read-only game file I/O legitimately exposes them.
 if(Executable(logical)){SetLastError(ERROR_ACCESS_DENIED);return {};}
 return Map(name,false);
}
bool ConfigureDataFiles(const wchar_t* packageRoot,const wchar_t* localRoot){std::lock_guard<std::recursive_mutex> lock(gate);if(!packageRoot||!localRoot)return false;auto p=Fold(packageRoot),d=Fold(localRoot);while(!p.empty()&&p.back()==L'\\')p.pop_back();while(!d.empty()&&d.back()==L'\\')d.pop_back();if(p.empty()||d.empty()||p==d)return false;if(!package.empty())return package==p&&data==d;package=p;data=d;cwd=package+L"\\6.21\\fortnitegame\\binaries\\win64";return true;}
bool RegisterCodeFileAlias(const wchar_t* original,const wchar_t* physical){std::lock_guard<std::recursive_mutex> lock(gate);if(!original||!physical||package.empty())return false;auto o=Fold(original),p=Fold(physical);for(auto path:{o,p})if(path.empty()||path[0]==L'\\'||path.find(L':')!=std::wstring::npos||path.find(L"..")!=std::wstring::npos)return false;if(!Executable(o)||!Executable(p))return false;auto key=package+L"\\"+o,value=package+L"\\"+p;auto old=code.find(key);if(old!=code.end())return old->second==value;code[key]=value;code[value]=value;return true;}
void* ResolveDataFileImport(const char* dll,const char* name){if(!dll||!name||(_stricmp(dll,"kernel32.dll")&&_stricmp(dll,"kernelbase.dll")&&_strnicmp(dll,"api-ms-win-core-file",20)&&_strnicmp(dll,"api-ms-win-core-processenvironment-",33)))return nullptr;
#define API(n,f) if(!strcmp(name,n))return reinterpret_cast<void*>(&f)
 API("CreateFileW",Open);API("GetFileAttributesW",Attributes);API("GetFileAttributesExW",AttributesEx);API("FindFirstFileExW",Find);API("CreateDirectoryW",Directory);API("DeleteFileW",Delete);API("RemoveDirectoryW",RemoveDataDirectory);API("GetCurrentDirectoryW",Current);API("SetCurrentDirectoryW",SetCurrent);
#undef API
 return nullptr;
}
