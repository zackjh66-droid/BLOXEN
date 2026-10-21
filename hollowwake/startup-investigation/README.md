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
the first argument, instead of leaving RCX equal to the thread's function-pointer
argument. This is motivated by measured original-process entry register state;
the probe checks it against NtQueryInformationProcess. It never changes the PEB
or constructs fake process metadata. This experiment alone does not establish
the full entry ABI or make the host's main-image identity equal the client DLL.

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
