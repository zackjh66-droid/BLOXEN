#include "../Native/StartupImport.h"
#include "../Native/ClientImageStartup.h"
#include "../Native/ClientCryptoEnvironment.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
static char* __cdecl Guest(){static char value[]="guest arguments with spaces";return value;}
static ClientCryptoEnvironment cryptoEnvironment;
static char* __cdecl GuestEnvironment(const char* name){return cryptoEnvironment.Lookup(name);}
static void Stage(const char* name){printf("OWNED_STARTUP_STAGE %s\n",name);fflush(stdout);}
static void Log(const std::string& value){puts(value.c_str());fflush(stdout);}
static int Fail(int code,const char* operation,DWORD error=0){printf("OWNED_STARTUP_IMPORT_FAILURE code=%d operation=%s Win32=%lu\n",code,operation,error);fflush(stdout);return code;}
int main(int argc,char** argv){
 if(argc!=4)return Fail(1,"fixture-plan-arguments");
 Stage("load-provider");auto provider=LoadLibraryW(L"StartupProvider.dll");if(!provider)return Fail(1,"load-provider",GetLastError());
 Stage("load-consumer-original-pe-layout");auto consumer=LoadLibraryW(L"StartupConsumer.dll");if(!consumer)return Fail(1,"load-consumer",GetLastError());
 auto original=(char*(__cdecl*)())GetProcAddress(provider,"_get_narrow_winmain_command_line");auto read=(char*(__cdecl*)())GetProcAddress(consumer,"ReadFixtureWinMainCommand");auto copied=(char*(__cdecl*)())GetProcAddress(consumer,"ReadCopiedFixtureWinMainCommand");auto slots=(const ULONG_PTR*)GetProcAddress(consumer,"OwnedCopiedImportSlots");
 if(!original||!read||!copied||!slots)return Fail(2,"lookup-owned-exports",GetLastError());
 ClientCompilerImportCopy plan={(DWORD)strtoul(argv[1],nullptr,0),(DWORD)strtoul(argv[2],nullptr,0),strtoull(argv[3],nullptr,0),"StartupProvider.dll","_get_narrow_winmain_command_line"};
 auto headers=(IMAGE_NT_HEADERS64*)((BYTE*)consumer+((IMAGE_DOS_HEADER*)consumer)->e_lfanew);auto size=headers->OptionalHeader.SizeOfImage;
 if(slots!=(const ULONG_PTR*)((BYTE*)consumer+plan.destinationRva)||slots[0]!=plan.unboundValue||strcmp(read(),"original host arguments"))return Fail(2,"signed-unbound-slot-and-declared-import");
 if(SUCCEEDED(VerifyClientImportCopies(consumer,size,&plan,1,&VirtualQuery,&Log)))return Fail(2,"unbound-copy-falsely-passed-verification");
 Stage("restore-from-real-native-loader-binding");auto restored=RestoreClientImportCopies(consumer,size,&plan,1,&VirtualQuery,&VirtualProtectFromApp,&Log);if(FAILED(restored))return Fail(3,"restore-copied-import",(DWORD)restored);
 if(slots[0]!=(ULONG_PTR)original||strcmp(copied(),"original host arguments"))return Fail(3,"restored-real-provider-call");
 if(BindWinMainCommandImport(consumer,"wrong-provider.dll",(void*)&Guest,&plan,1)||GetLastError()!=ERROR_PROC_NOT_FOUND)return Fail(4,"unknown-provider-error",GetLastError());
 Stage("bind-declared-and-measured-copy");if(!BindWinMainCommandImport(consumer,"StartupProvider.dll",(void*)&Guest,&plan,1))return Fail(4,"bind-both-tables",GetLastError());
 if(strcmp(read(),Guest())||strcmp(copied(),Guest())||slots[0]!=(ULONG_PTR)&Guest)return Fail(4,"bound-owned-call-results");
 if(strcmp(original(),"original host arguments"))return Fail(5,"provider-state-changed");
 Stage("bind-real-client-crt-getenv");
 auto environment=(char*(__cdecl*)(const char*))GetProcAddress(consumer,"ReadFixtureEnvironment");
 auto realEnvironment=QueryClientCrtImport(consumer,"api-ms-win-crt-environment-l1-1-0.dll","getenv");
 if(!environment||!realEnvironment)return Fail(6,"actual-consumer-getenv-binding",GetLastError());
 if(BindClientCrtImport(consumer,"api-ms-win-crt-environment-l1-1-0.dll","getenv",reinterpret_cast<void*>(1),reinterpret_cast<void*>(&GuestEnvironment))||GetLastError()!=ERROR_RETRY)return Fail(6,"reject-unexpected-original-binding",GetLastError());
 auto prior=getenv("OPENSSL_ia32cap");bool hadPrior=prior!=nullptr;std::string saved=prior?prior:"";
 if(_putenv_s("OPENSSL_ia32cap","")||!cryptoEnvironment.Bind(reinterpret_cast<ClientCryptoEnvironment::Getter>(realEnvironment))||!cryptoEnvironment.Enable())return Fail(6,"configure-real-provider-delegation");
 bool bound=BindClientCrtImport(consumer,"api-ms-win-crt-environment-l1-1-0.dll","getenv",realEnvironment,reinterpret_cast<void*>(&GuestEnvironment));
 auto selected=bound?environment("OPENSSL_ia32cap"):nullptr;
 bool clientOnly=selected&&!strcmp(selected,":~0x20000000")&&getenv("OPENSSL_ia32cap")==nullptr;
 bool environmentRestored=_putenv_s("OPENSSL_ia32cap",hadPrior?saved.c_str():"")==0;
 if(!bound||!clientOnly||!environmentRestored)return Fail(6,"real-module-getenv-call-or-host-isolation",GetLastError());
 puts("PASS actual OS-loaded consumer CRT getenv binding configured through its real IAT; unexpected binding rejected; host getenv unchanged.");
 puts("PASS actual OS-loaded provider, real current-process copied-import restoration, rejection before restoration, both real WinMain call sites rebound; original provider unchanged. No game/Xbox execution.");return 0;
}
