// Original target requiring an actual DIR64 relocation; no CRT or entry point.
extern "C" {
int fixtureValue=621;
int* volatile fixturePointer=&fixtureValue;
__declspec(dllexport) int Answer(){return *fixturePointer;}
}
