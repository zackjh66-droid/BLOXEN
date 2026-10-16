#pragma once
#include <string>
void ConfigureUnsupported(void(*log)(const std::string&));
void* UnsupportedImport(const char* dll,const char* symbol);
bool SealUnsupported();
unsigned UnsupportedCount();
