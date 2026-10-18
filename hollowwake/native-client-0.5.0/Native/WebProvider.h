#pragma once
// Native WinHTTP/WinINet compatibility with owned handles and configured origin.
// No account/token exchange is fabricated by this transport adapter.
void* ResolveWebImport(const char* dll,const char* symbol);
