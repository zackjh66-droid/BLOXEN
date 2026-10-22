#include "../Native/StartupImport.h"
#include <cstring>
#include <cstdio>
static char* __cdecl Guest(){static char value[]="guest arguments with spaces";return value;}
int main(){
 auto provider=LoadLibraryW(L"StartupProvider.dll"),consumer=LoadLibraryW(L"StartupConsumer.dll");if(!provider||!consumer)return 1;
 auto original=(char*(__cdecl*)())GetProcAddress(provider,"_get_narrow_winmain_command_line");auto read=(char*(__cdecl*)())GetProcAddress(consumer,"ReadFixtureWinMainCommand");auto copied=(char*(__cdecl*)())GetProcAddress(consumer,"ReadCopiedFixtureWinMainCommand");auto slots=(const ULONG_PTR*)GetProcAddress(consumer,"OwnedCopiedImportSlots");
 if(!original||!read||!copied||!slots||slots[0]!=(ULONG_PTR)original||strcmp(read(),"original host arguments")||strcmp(copied(),"original host arguments"))return 2;
 if(BindWinMainCommandImport(consumer,"wrong-provider.dll",(void*)&Guest)||GetLastError()!=ERROR_PROC_NOT_FOUND)return 3;
 if(!BindWinMainCommandImport(consumer,"StartupProvider.dll",(void*)&Guest)||strcmp(read(),Guest())||strcmp(copied(),Guest())||slots[0]!=(ULONG_PTR)&Guest)return 4;
 if(strcmp(original(),"original host arguments"))return 5;
 puts("PASS actual OS loader restores copied read-only import outside original IAT directory; both real WinMain call sites rebound and original provider unchanged. No game/Xbox execution.");return 0;
}
