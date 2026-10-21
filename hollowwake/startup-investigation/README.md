This bounded native Windows investigation runs the exact preserved Season 6
executable in a separate process before changing its DLL conversion again.
It is not an Xbox package and does not establish loading-screen or lobby success.

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
