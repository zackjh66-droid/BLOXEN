#include <windows.h>
#include <iostream>
using Read=DWORD(*)();using Change=void(*)(DWORD);
struct Functions{Read read;Change change;};
DWORD WINAPI Thread(void* value){auto p=static_cast<Functions*>(value);if(p->read()!=621)return 1;p->change(4526925);return p->read()==4526925?0:2;}
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;auto module=LoadLibraryW(argv[1]);if(!module)return 3;
 Functions f={reinterpret_cast<Read>(GetProcAddress(module,"ReadOwnedTls")),reinterpret_cast<Change>(GetProcAddress(module,"ChangeOwnedTls"))};if(!f.read||!f.change||f.read()!=621)return 4;
 auto thread=CreateThread(nullptr,0,Thread,&f,0,nullptr);if(!thread)return 5;WaitForSingleObject(thread,INFINITE);DWORD result=1;GetExitCodeThread(thread,&result);CloseHandle(thread);
 if(result||f.read()!=621)return 6;FreeLibrary(module);std::cout<<"PASS owned OS-loaded /NOENTRY DLL static TLS on existing/new thread, isolation preserved. Not game/Xbox proof.\n";return 0;
}
