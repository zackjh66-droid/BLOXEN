# Hollowwake native client integration — 0.5.5 work in progress

The target is Fortnite 6.21-CL-4526925 through Xbox Dev Mode. **The original loading screen and lobby have not been reached.** The signed 0.5.4 release and working account-linking companion remain available as rollback; this source contains later, unfinished runtime changes.

Launch calls `HWAttemptPackagedClient`, not the old LocalState manual mapper. Selected game executable/dependency copies must be inside the signed package. The native OS loader then owns their TLS, DLL lifecycle and unwind registration. Generated proxies dispatch explicit implemented API behavior; unsupported calls are not replaced by successful return values.

Content download starts after account linking or restored sign-in. The signed plan requires 139 files and **26,449,827,840 installed data bytes**. Indexed compressed range downloads avoid storing another full archive. Progress, cancellation/resume, per-file SHA-256 and a final on-disk audit are implemented. Every selected data byte was independently decompressed and hash-checked from the preserved archive; that is not an Xbox transfer/storage test. Code is installed with the package, not executed from downloaded writable data.

The original fixed audits remain 843 redirected pairs and 838 platform pairs. Reports retain every missing name, including delayed imports. Normal imports across all selected dependencies block loading; original delayed imports retain Windows demand-loading/failure behavior. Neither an exported address nor a successful Windows fixture proves Xbox compatibility.

The latest user Xbox baseline contained 42 missing platform bindings. Five timer/network/performance bindings have implemented behavior and hosted checks. Three DirectSound bindings now dispatch a reviewed, locally built DSOAL/OpenAL implementation, with its own dependency inventory and packaged behavior test. Their final integration results must be read separately; do not claim all bindings are complete. Mesa UWP WGL remains a build/integration candidate, not a functioning Fortnite renderer.

The existing Hollowwake account session stays outside guest code. The socket/web policies and project account API are not proof of Fortnite-compatible login. Client OAuth/profile/catalog integration, presence and original lobby operation remain unverified. Matchmaking stays unavailable.

See [build instructions](docs/BUILD.md), [limitations](docs/LIMITATIONS.md) and [installation](docs/INSTALL.md). No private signing keys or game files belong in public source/artifacts.
