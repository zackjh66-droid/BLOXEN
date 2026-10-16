#pragma once
#include <string>
void ConfigureDependencies(const std::wstring& root,void*(*imports)(const char*,const char*),void(*log)(const std::string&));
void* ResolveDependency(const char* dll,const char* symbol);
unsigned DependencyFailures();
bool InitializeDependencies();
std::string DescribeDependencyAddress(unsigned long long address);
