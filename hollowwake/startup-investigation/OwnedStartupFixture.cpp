#include <windows.h>
int main(){SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);volatile int* invalid=nullptr;*invalid=621;return 1;}
