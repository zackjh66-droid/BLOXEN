#pragma once
#include <windows.h>
// Tests real OS-loader export resolution for the fixed unredirected inventory.
// Calls no game entry point and invokes none of the resolved exports.
// Every pair is audited. requiredMissing counts normal imports across all
// selected dependencies; unavailable delay-only functions stay in the report.
unsigned AuditPlatformImports(const wchar_t* outputFile,unsigned* requiredMissing=nullptr);
