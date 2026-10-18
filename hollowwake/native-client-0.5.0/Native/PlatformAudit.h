#pragma once
#include <windows.h>
// Tests real OS-loader export resolution for the fixed unredirected inventory.
// Calls no game entry point and invokes none of the resolved exports.
unsigned AuditPlatformImports(const wchar_t* outputFile);
