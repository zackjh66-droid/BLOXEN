#include "../Native/StartupImport.h"
#include <cstring>
#include <cstdio>
static char* __cdecl Guest(){static char value[]="guest arguments with spaces";return value;}
static int Fail(int code,const char* operation,DWORD error=0){printf("OWNED_STARTUP_IMPORT_FAILURE code=%d operation=%s Win32=%lu\n",code,operation,error);return code;}
int main(){
 auto provider=LoadLibraryW(L"StartupProvider.dll");if(!provider)return Fail(1,"load-provider",GetLastError());
 auto consumer=LoadLibraryW(L"StartupConsumer.dll");if(!consumer)return Fail(1,"load-repaired-consumer",GetLastError());
 auto original=(char*(__cdecl*)())GetProcAddress(provider,"_get_narrow_winmain_command_line");auto read=(char*(__cdecl*)())GetProcAddress(consumer,"ReadFixtureWinMainCommand");auto copied=(char*(__cdecl*)())GetProcAddress(consumer,"ReadCopiedFixtureWinMainCommand");auto slots=(const ULONG_PTR*)GetProcAddress(consumer,"OwnedCopiedImportSlots");
 if(!original||!read||!copied||!slots)return Fail(2,"lookup-owned-exports",GetLastError());
 if(slots[0]!=(ULONG_PTR)original)return Fail(2,"os-loader-copied-slot-mismatch");
 if(strcmp(read(),"original host arguments")||strcmp(copied(),"original host arguments"))return Fail(2,"original-owned-call-results");
 if(BindWinMainCommandImport(consumer,"wrong-provider.dll",(void*)&Guest)||GetLastError()!=ERROR_PROC_NOT_FOUND)return Fail(3,"unknown-provider-error",GetLastError());
 if(!BindWinMainCommandImport(consumer,"StartupProvider.dll",(void*)&Guest))return Fail(4,"bind-both-tables",GetLastError());
 if(strcmp(read(),Guest())||strcmp(copied(),Guest())||slots[0]!=(ULONG_PTR)&Guest)return Fail(4,"bound-owned-call-results");
 if(strcmp(original(),"original host arguments"))return Fail(5,"provider-state-changed");
 puts("PASS actual OS loader restores copied read-only import outside original IAT directory; both real WinMain call sites rebound and original provider unchanged. No game/Xbox execution.");return 0;
}
