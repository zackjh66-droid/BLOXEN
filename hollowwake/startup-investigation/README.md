This bounded native Windows investigation runs the exact preserved Season 6
executable in a separate process before changing its DLL conversion again.
It is not an Xbox package and does not establish loading-screen or lobby success.
Both debugger-attached and plain process runs are bounded. CPU time and one
numeric main-thread snapshot at the time bound distinguish a CPU loop from a
wait. Plain mode does not claim an entry breakpoint or install hardware watches.

A separate converted baseline reuses the existing converter, original desktop
imports and original dependencies. Its tiny owned host verifies the generated
DLL hash and invokes the preserved export on a one-MiB thread. It uses neither
the Xbox compatibility adapters nor the later pointer/endpoint modifications.
This distinguishes conversion semantics from those additional changes. It is
not the integrated launcher, has no login ticket or complete asset set, and
cannot validate the Xbox route or lobby.

The identity comparison then reuses the production namespace for self-module,
logical original filename and command-line imports, and the production WinMain
tail binding. Other modules still use actual desktop APIs. This deliberately
tests the identified self-identity subset, not the complete Xbox namespace or
all native adapters. Native code and startup checks remain intact. A hardware
watch at the helper entry captures its argument and whether its observed
comparison operands agree; later snapshots also retain the computed frame base.

The entry-context experiment passes the real current process PEB pointer as
the first argument and restores RAX to the original entry address using an owned
leaf MASM thunk. This is motivated by measured original-process entry register state;
the probe checks it against NtQueryInformationProcess. It never changes the PEB
or constructs fake process metadata. This experiment alone does not establish
the full entry ABI or make the host's main-image identity equal the client DLL.

The separate main-image case keeps EXE characteristics and the original filename.
It reuses the existing converter's bounded append mechanism, replacing only the
converter's own entry shim with LEA RAX,original_entry; JMP RAX. The previous
JMP-only comparison retained the new entry address in RAX and still faulted.
This keeps the client as the OS process main image. No original game
instruction/import/TLS/relocation is changed. Its exact prepared hash is pinned
in the probe. This is an initialization comparison, not a UWP/Xbox bootstrap,
and it does not include a new launcher, full game assets, login or matchmaking.

The desktop host is compiled with `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR` because
the preserved game directory supplies MSVCP140 14.13.26020, while CI uses newer
STL headers. A prior identity-host run faulted in MSVCP140 before guest entry.
This compatibility setting addresses that test-host runtime mismatch; it is not
claimed to fix the Xbox game crash or change the preserved runtime dependency.

The no-entry comparison changes only the DLL characteristic, PE entry RVA,
checksum and invalidated certificate directory. It preserves all section
headers/bytes, exports, import metadata, TLS, unwind and relocations. It requires
the exact original SHA and verifies the original TLS callback list is null.
An owned `/NOENTRY` DLL test first validates OS static TLS initialization and
thread isolation. The original game EXE entry is then called by pinned RVA with
the measured entry handoff; no original code or checks are changed. This is a
controlled comparison until its actual native result is reviewed.

The owned no-entry TLS test failed in run 39655659302, so that route is rejected
and is not selected by default. `-EvaluateNoEntry` retains its failing gate; it
has not been changed into a passing check. It is not a packaging option.

The selected new-section comparison retains a real owned DLL entry and preserves
all nine original section headers/contents. It moves the NT header/table back
40 bytes within the DOS stub to create a real additional header slot, preserving
the opaque original bytes at 0x440..0x600. The final client code section is not
extended. The DOS/Rich tail is changed and the original input remains unchanged.
All original export slots and key directories are checked after conversion.
This is a genuine layout comparison, not a restoration/spoof of runtime headers
or a bypass of any client check. Native execution results determine whether it
is usable; static checks alone do not establish startup.

Native runs through 39656500020 still fault at the original write for the
converted variants. Entry register restoration, the self-identity subset and
preserving original section extents have not fixed that initialization.

The initialized-image capture runs the exact unchanged signed-source client as
a main process, then stops at the original entry's first call-return address.
It requires the observed initialized comparison state, reads only the original
module's header/file-backed section ranges, and stops the process. It does not
patch checks, instructions, runtime headers or the PEB. A missing boundary or
incomplete section read rejects the capture. Only numeric status is public;
all game-containing bytes are AES-GCM encrypted to the existing recipient key
before upload. This is private build-input research, not a runnable converted
client, signed package, Xbox test or lobby result. Loader-state normalization
and native execution must still be validated before any derived input is used.

The probe verifies the original client SHA-256 in both PowerShell and C++.
It sets hardware execution breakpoints at the preserved entry and dispatch
helper. It records numeric control-flow state and two bounded image fields.
It does not patch image instructions/data, OS PEB fields, checks, exception
dispositions, or return addresses. Guest exceptions are passed to normal OS
and guest handling. Presence of a debugger can itself affect protected startup;
compare this evidence accordingly rather than assuming native equivalence.

The child has a clean environment without CI credentials, no Hollowwake login
ticket/account, outbound traffic blocked by a per-program firewall rule, a
one-process/two-GiB kill-on-close job, and a 30-second execution bound. These
constraints may themselves prevent later game behavior. In particular this
does not stage all content or test an original menu. A separate owned crashing
child first tests that real first/second chance exceptions and process exit
are recorded. That fixture is only validation of the probe.

Archive acquisition still verifies all selected file hashes and does not run
archive scripts or third-party launchers. Private inputs and executable probes
are not uploaded. Only numeric evidence, build logs and bounded acquisition
status are retained. No signed package or website deployment is produced.

Run 39661179987: direct OS CreateThread startup preserves the original measured
RAX/RDX and RCX/R8 state, but both baseline and production self-identity cases
still fault at original RVA 0x5FA3A7B. No initialized boundary is reached. Do not
adopt the thread handoff alone as a crash fix. The next MainMetadataOnly profile
retains the original OS entry RVA and exact original section bytes, comparing
certificate/checksum metadata only against all existing conversion metadata.
Each candidate is SHA-pinned for its own bounded native execution. This profile
is a main-process Windows experiment, not an Xbox application launch method.

The integrated Windows launch and unchanged-source full-data comparison now run
without debugger attachment by default. Explicit `[native-trace]` workflow
requests opt into the existing bounded debugger for investigation; results
record whether it was requested and attached. Debugger traces change OS-visible
process state and cannot be treated as an ordinary launch control. Captured
initialized input still comes from the separately documented debug observation;
its process-specific initialization equivalence remains unproven. No debug
checks or process metadata are patched to make either route pass.
