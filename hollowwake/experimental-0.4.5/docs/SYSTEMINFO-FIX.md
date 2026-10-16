# Version-specific dependency startup fix

User-provided physical Xbox screenshot: runtime0.4.4.0, CALLED_UNSUPPORTED KERNEL32.dll!GetSystemInfo caller=dbghelp.dll RVA1221867 (0x12a4eb). Memory/TLS/window/graphics/clock/allocator contracts report621. This is an original dependency invocation, not main-entry or lobby success.

Static disassembly of the hash-allowlisted original dbghelp confirms a call through IAT RVA0x1118 and an immediate read of SYSTEM_INFO.dwPageSize. Its 17-entry _initterm table at RVA0x15d0 includes this routine at0x12a4dc and then the GetVersion initializer at0x12a504. No original module is executed in hosted analysis.

SystemInfo.h binds GetSystemInfo and GetNativeSystemInfo to the actual SDK exports, without synthetic values. A real original mapped DLL imports GetSystemInfo, calls it during DLL_PROCESS_ATTACH, checks structure canaries and copies the result for comparison against the host's real query. The packaged UWP contract also compares page size/allocation granularity, address bounds, processor architecture/count/mask and canaries against GetNativeSystemInfo.

The first build exposed a genuine SDK constraint: GetVersion is excluded from UWP headers (C2039/C2065). Its compatibility implementation queries GetVersionExW, which is in the public Application API partition, and packs documented major/minor/build/platform bits. The result remains host-manifest-dependent; it is not a spoofed Windows release or an assertion of the real kernel version. Failure writes a named CRASH record and fails fast. The mapped fixture's result is compared to the real legacy GetVersion on hosted desktop Windows. No placeholder success or fixed version constant is used.

References: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsysteminfo ; https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo ; https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversion ; https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/sysinfoapi.h .
