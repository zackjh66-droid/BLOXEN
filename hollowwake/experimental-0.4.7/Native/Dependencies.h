#pragma once
#include <string>
void ConfigureDependencies(const std::wstring& root,void*(*imports)(const char*,const char*),void(*log)(const std::string&),void*(*query)(const char*,const char*)=nullptr);
void* ResolveDependency(const char* dll,const char* symbol);
unsigned DependencyFailures();
bool InitializeDependencies();
std::string DescribeDependencyAddress(unsigned long long address);
