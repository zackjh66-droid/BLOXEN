#pragma once
#include <windows.h>
// Tests real OS-loader export resolution for the fixed unredirected inventory.
// Calls no game entry point and invokes none of the resolved exports.
// Every pair is audited. requiredMissing counts normal imports across all
// selected dependencies; unavailable delay-only functions stay in the report.
unsigned AuditPlatformImports(const wchar_t* outputFile,unsigned* requiredMissing=nullptr);
// One additional unredirected normal import introduced by the two observed
// dynamically loaded APEX modules; original 838-pair inventory stays intact.
unsigned AuditApexStartupImports(const wchar_t* outputFile);
