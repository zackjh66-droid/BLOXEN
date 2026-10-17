#include "../Native/StartupImport.h"
#include <cstring>
#include <cstdio>
static char* __cdecl Guest(){static char value[]="guest arguments with spaces";return value;}
int main(){
 auto provider=LoadLibraryW(L"StartupProvider.dll"),consumer=LoadLibraryW(L"StartupConsumer.dll");if(!provider||!consumer)return 1;
 auto original=(char*(__cdecl*)())GetProcAddress(provider,"_get_narrow_winmain_command_line");auto read=(char*(__cdecl*)())GetProcAddress(consumer,"ReadFixtureWinMainCommand");if(!original||!read||strcmp(read(),"original host arguments"))return 2;
 if(BindWinMainCommandImport(consumer,"wrong-provider.dll",(void*)&Guest)||GetLastError()!=ERROR_PROC_NOT_FOUND)return 3;
 if(!BindWinMainCommandImport(consumer,"StartupProvider.dll",(void*)&Guest)||strcmp(read(),Guest()))return 4;
 if(strcmp(original(),"original host arguments"))return 5;
 puts("PASS real per-image WinMain command import rebinding; actual imported call changes and host provider stays unchanged. No game/Xbox execution.");return 0;
}
