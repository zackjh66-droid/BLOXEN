# Hollowwake 0.4 — client initialization experiment

This download is a development-signed x64 UWP runtime that can import the actual inspected 6.21 client privately onto the console and attempt its original initialization. It is not a completed Fortnite port. No original lobby, character, locker or shop success is claimed. Xbox execution is unverified.

The old working companion, account service, owner-only administration and rollback packages are preserved. This package has a separate identity and uses your existing Hollowwake account with a new device approval.

Changes: original x64 image/dependency mapping, TLS donors, unwind registration, supported window/message/controller hooks, real DXGI/D3D11 composition bridge, explicit original entry attempt, fail-fast missing-API logs, post-crash log access, pinned minimum import, optional full import, and orange HW/focus branding. There are no placeholder successful implementations of missing native APIs.

Hosted Windows checks passed on the exact signed package: installation/activation, executable-memory fixture, distinct packaged TLS slots, window/message and graphics contracts, actual missing-API fail-fast log/process exit, startup recovery, live own-account linking, protected restart sign-in, logout and revocation. Original mapped-TLS fixtures also passed, including modified-file rejection. No downloaded game code ran in hosted CI. See WindowsVerification for scope and logs.

131 import offsets/sizes/hashes were statically checked against the unchanged source ZIP; a source HTTPS byte range was checked independently. Native CLR/C++ imports were checked against provided x64 VCLibs. These do not establish console execution. No physical controller or in-game behavior has been verified.

Install using INSTALL.md. Start with Show launch log, then the small Import boot files test, then Attempt client boot. Return the last EXECUTE/STOP/CRASH/CALLED_UNSUPPORTED record. The full archive is not necessary just to reproduce an unsupported call.

Remaining requirements are in LIMITATIONS.md: extensive Windows ABI/lifecycle coverage, private native HTTP/authentication handoff, complete version-compatible backend/presence and validated assets. The production client-protocol adapter remains disabled. No matches are provided.

Signing: public certificate only; private key was non-exportable on the ephemeral build runner. Self-signed development certificate, not Microsoft/Epic publisher endorsement. Game content is not included.

Live website: https://hollowwake.jihnjohn10.chatgpt.site
Admin remains owner-authenticated at the existing private address; it is deliberately omitted from public release metadata.
