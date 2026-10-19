#pragma once
// Exact Season 6 DirectSound demand. Returns real reviewed provider exports,
// never a success thunk. Missing packaged DLL/export remains unresolved.
void* ResolveAudioProviderImport(const char* module,const char* symbol);
