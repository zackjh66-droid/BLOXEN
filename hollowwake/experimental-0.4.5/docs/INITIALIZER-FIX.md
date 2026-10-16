# Observed Xbox initializer failure

User-provided photograph: runtime0.4.3.0, CALLED_UNSUPPORTED msvcrt.dll!_initterm caller=dbghelp.dll RVA=703977. Allocator/clock contracts621 and dependency blockers0. This demonstrates an original dependency calling an unsupported API, not main executable entry or lobby success.

0.4.5 resolves only msvcrt.dll _initterm/_initterm_e to original implementation with the documented legacy C ABI: half-open callback table, skip nulls, invoke in order; _initterm_e returns the first nonzero result without running later callbacks. Callback exceptions are not swallowed. No fabricated success is returned.

Reference: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/initterm-initterm-e?view=msvc-170 .

Focused test is a small original mapped DLL importing these exact msvcrt symbols; its real DLL_PROCESS_ATTACH checks null entries, ordered callbacks, empty tables, successful integer initializers, and error73 short circuit before allocator tests. Production Dependencies.cpp and LegacyCrt.h resolve and execute it. No downloaded game code runs in CI.

The user must test the real dependency on Xbox. Later unimplemented APIs, TLS/thread/lifecycle and graphics/network gaps remain. An unsupported call intentionally terminates after flushing its precise name, since continuing a partially initialized guest could corrupt the host. Reopen Show launch log to recover the next call.
