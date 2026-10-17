extern "C" __declspec(dllimport) char* __cdecl _get_narrow_winmain_command_line();
extern "C" __declspec(dllexport) char* __cdecl ReadFixtureWinMainCommand(){return _get_narrow_winmain_command_line();}
