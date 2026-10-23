# Hollowwake actual-client startup status — 23 October 2026

Status: blocked in converted-client startup. No verified Hollowwake lobby or
working Xbox game package is claimed. The failed private 0.6.15 candidate and the 0.6.16/0.6.17 candidates
are not replacements for the live download.

The Windows desktop used here is a temporary Microsoft-hosted GitHub Actions
Windows Server 2022 machine in the project's repository. Source editing uses
the Linux cloud workspace. Neither machine is the user's PC or Xbox.

## Current implementation

Source: `ed044de8f5ff52bcc03b23084cec9e5673441b6c` on
`hollowwake/native-system-route`, repository `zackjh66-droid/BLOXEN`.

- Client-local OpenSSL configuration delegates to the real already-loaded CRT
  getenv import and selects the library's documented legacy CPU fallback when
  no explicit setting exists. Host environment, CPUID, hashes and signature
  decisions are preserved. Actual game execution observed this path and passed
  its earlier SHA initialization failure.
- Concrete core DLL lookups return real loaded OS module handles. Filename,
  address lookup and reference operations use those real images; export queries
  retain the explicit compatibility policy.
- Original source DLL names identify the signed compatibility proxy images.
  DirectSound retains the existing dedicated HWAUD.dll provider, including when
  a generated HWDS.dll proxy also exists. Real owned DLL tests cover this
  precedence and the core-module contracts.
- The actual Launch action still calls the native client path. The Windows
  harness now rejects negative native HRESULTs, absent original entry threads,
  absent engine initialization and process exit. Launcher survival alone is
  not a successful startup check. Engine and Frontend milestones are separate
  from visible menus and lobby verification.

## Latest ordinary actual execution

[Windows run 40002881667](https://github.com/zackjh66-droid/BLOXEN/actions/runs/40002881667)
uses the runtime above, with post-fault observation and preserved dependency
restoration source `6216d2f8add45abe4ce100db3eb18b57e1a24f2b`. Neither comparison process had a
debugger attached. A separate read-only observer sampled the unchanged original.

The private package compiled, signed and passed package-content hash checks.
The normal account/device-link flow and actual Launch button were exercised.
All 139 selected data files (26,449,827,840 bytes) and 53 code files verified.
The preserved archive is 12,528,679,454 bytes, SHA-256
`bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5`.
The exact original executable remains SHA-256
`9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478`.

The actual converted client reached its original entry thread, initialized its
engine, created a real D3D11 composition chain and completed loading
`/Game/Maps/Frontend`. It then exited with `0xC0000005` before visible menus or
a lobby were established. Fortnite-compatible login has not been demonstrated by this execution.

The loaded main image base was `140712188575744`. The fatal execute target was
`140717581884128`, exactly image base plus `0x141775ae0` (preferred image base
`0x140000000` plus `0x1775ae0`). The new read-only post-fault observer verified
the exact original helper instructions at RVA `0x6097501`, both retained stack
target slots, and the target arithmetic. Its recorded operand was the loaded
image base itself; its constant was `5393308384` (`0x141775ae0`). No debugger,
context changes or exception-disposition changes were used.

The retained frame contained candidate comparison index 0, left DWORD
`2391641668`, right DWORD `2582946561`, and a right-side image location at RVA
`20332922` (`0x136417a`). Other predecessors of this helper exist. These frame
values do not prove which comparison branch executed or identify an integrity
check. The producer/context error remains unresolved; the jump is not patched.

The unchanged original executable, with the same verified content and CPU
configuration, stayed alive for the full 180-second comparison and rendered
its original Epic sign-in screen. No Hollowwake/Epic credentials were supplied
and its outbound traffic was blocked. That screen is an original Windows
baseline, not Hollowwake authentication, an Xbox test or a lobby demonstration.

## Binding inventory and remaining requirement

Against the same Windows inventories before and after the module changes:

| Inventory | Before unresolved | After unresolved |
| --- | ---: | ---: |
| Redirected static bindings, 843 entries | 0 | 0 |
| Unredirected platform baseline, 838 entries | 0 | 0 |
| Additional unredirected APEX binding, 1 entry | 0 | 0 |
| Observed dynamic PSAPI group, 5 entries | 0 | 0 |

These counts measure Windows binding availability, not complete behavior or
Xbox compatibility. The module changes implement behavior; they do not claim
an artificial reduction of the static inventory.

The latest run also reports dynamic lookup
`HWU32.dll!SetProcessDPIAware`. Its current proxy has only the generated static
exports, and the native window resolver restricts its demand to the static
inventory. This dynamic lookup remains unimplemented. Its failure was nonfatal
in this run: the engine initialized and Frontend loaded afterward. The two
other logged names `GetFileInformationByHandleExW` and
`SetFileInformationByHandleW` are not real Windows export names; no invented
aliases were added for them.

The blocking operation is the converted client's out-of-image control transfer.
The minimum remaining requirement is a correct initialization/context path
that produces the same valid in-image transfer as the original process. The
producer of the bad operand/control-flow path has not been established. The
following checks narrow the problem; they do not demonstrate a working conversion:

- The ordinary unchanged original produced 84 read-only samples over about 170
  seconds. Its runtime header ImageBase equalled its actual loaded base. The
  decoded transfer constant stayed `0x141775ae0` and the stored preferred base
  stayed `0x140000000`. Those stored values alone do not explain the failure;
  changing them to avoid the jump would not be a justified repair.
- At the original first-call-return boundary, all 3,149,432 bytes of the zero-fill
  tail were zero. There is no measured omitted BSS relocation state to replay
  from that boundary (run 39941433057).
- Watches on the real OS-bound AddVectoredExceptionHandler and
  RemoveVectoredExceptionHandler imports observed no registrations before that
  boundary, with coverage armed and no pending calls (run 39943536896).
- The original entry thread's complete 28-byte static TLS block matched both
  the original file template and the runtime image template byte for byte
  (run 39944503883). No missing static TLS contents were measured there.

The three boundary checks use the explicitly documented debugger observation;
that scope does not prove all ordinary or later initialization state portable.
No BSS contents, TLS data or guessed exception handlers were replayed into the
client. The unresolved requirement remains correct actual startup/control flow,
not a demonstrated requirement to change servers or reacquire content. No jump, check, exception decision or API success is patched to hide
this failure.

## Follow-up tracing limits

Run `39949659164`, source `b6c8f3ed841b21e0767f2fd83abb44d1c885e784`,
compiled and exercised the actual integrated package with an explicitly attached
Windows debugger. That process exited with `0xC0000005` at `7705811632` before
engine/Frontend milestones were recorded. The requested helper watch produced
no comparison event. This is inconclusive for the ordinary failure described
above; it is not evidence that the new input values were collected. The plain,
unchanged original comparison again stayed alive for 180 seconds.

Evidence artifact `12500873758` has ZIP SHA-256
`9516d96877152daadca2355153d226213942ba9ef47688f0bbe367352486cf98`.
The recipient-encrypted failed candidate remains private and was not promoted.

Source `6e668e842f9b3aa870a8c7f39eaa00e29359bf02` instead adds a read-only
post-fault observer to the existing native exception logger. For the exact
ordinary execute-fault pattern it validates the unchanged original helper bytes,
both retained stack target slots and the target arithmetic before reading the
former frame's bounded comparison inputs. It changes no context, instruction,
branch, return value or exception disposition. This is investigation code, not a
startup repair. The owned-code checks passed in run `40001569453`, but actual execution was
skipped because the historical provider artifact was no longer available. This
is a test-input availability failure, not a new client execution result.

Source `6216d2f8add45abe4ce100db3eb18b57e1a24f2b` restores the exact preserved
licensed providers from existing private Blob storage. The complete original
archive is 39,172,799 bytes, SHA-256
`233ed20ffcf8d87230c126453e6a99d36060dddb5f32ac994ec9ba6619f200dd`.
Its six provider binaries match their original inventories; corresponding source
and licenses remain included. A full stored-object SHA-256 read succeeded and
anonymous access was denied. CI receives only per-run GET access, verifies the
whole archive before extraction, validates paths and binary hashes, and retains
the original Windows behavior receipts. Local full-archive restoration passed;
one-byte corruption and wrong-run leases were rejected before final outputs.
This restores completed work without substituting or recompiling providers.

The replacement ordinary integrated Windows run `40002881667` completed with
engine/Frontend execution and the same fatal transfer, as described above. Its
post-fault observer captured the retained operands successfully. No new public
diagnostic-only package was released.

## Current graphics implementation under test

Source `e046b5028ed6adc2bcfaca502bd0e02dae4fb630`, candidate 0.6.16, adds a
native-HWND path for the exact original windowed `ALLOW_MODE_SWITCH` DXGI
request. The actual OS factory receives the original descriptor and returns its
actual swap chain or failure. Native buffer-count, flags, resize and present
behavior remain native; no unsupported operation is reported as successful.
The composition path retains its exclusive/multisample/flag checks.

Private preparation now audits the three original DXGI request instructions
instead of rewriting them. Both preparation and generated-header compilation
reject modified operands. This removes the earlier three instruction writes;
it does not establish that those writes caused the transfer failure. No client
branch, HRESULT check or authentication check is removed. An extended owned
fixture exercises genuine HWND creation, flags/count reporting, count-one resize,
GPU readback, Present and an invalid native resize failure. Local read-only audit
and negative-modification checks passed. The Windows owned-code fixture also
passed actual native HWND creation, original flag/count reporting, count-one
resize, GPU readback, Present and invalid-resize failure checks in run
`40008449820`. That run closed during production-app content verification, before Launch was
invoked. Its actual-client result is therefore **unexecuted**, not a success or
failure of the native graphics route. All 139 staged files and the whole source
archive passed their hashes; the launcher logged an unhandled managed
`0x80004005` exception at `launcher-ready`. It did not record an exception
message or stack in that build. The earlier native fault in 0.6.15 remains the
latest actual Fortnite execution failure. Native HWND/DXGI behavior on Xbox
remains unverified.

Evidence artifact `12526666568`, ZIP SHA-256
`1c09132d5298c5fded382e1b3c33f91c48939cbbc4be04a5c2b550c235d7cdb2`,
preserves that prelaunch failure. The signed recipient-encrypted candidate is
artifact `12525887903`, ZIP SHA-256
`0e1498abba0e95d94ef6412d0d900e59cd65e228f30829016280f6670f140f01`.
It was not promoted.

## Managed startup gate under test

Source `2031ace997f731f4c683110f368216133b2ac2d8`, candidate 0.6.17, retains
the original-instruction/native-DXGI work. Launch and manual verification remain
disabled until account/startup completion; the handlers also check that state.
The focus handler records ownership before changing properties and ignores
repeated focus events for the same button. The test
harness waits for completed startup and an enabled Launch control before manual
verification; all 139 content hashes and the independent Windows storage checks
remain required. This corrects an observable startup race but does not yet prove
the cause of the managed closure.

A bounded managed failure reporter adds only known error categories, static
managed frame names, exception types and HRESULTs. Raw messages, arguments,
URLs, credentials and paths are excluded. The original exception remains
unhandled. Local C# tests passed category extraction, secret/path exclusion and
unavailable-stack-metadata fallback. Windows/AOT and full-client checks are
pending in run `40013633784`; no public diagnostic-only package is released.

## Preserved evidence and releases

- Public numeric/log/screenshot evidence: artifact `12524173332`, ZIP SHA-256
  `8a7a622fb24ac9fa8a211494578ac0cb1f7db6cd6c9c39007fb7dabfb7503124`.
- Recipient-encrypted private signed candidate: artifact `12524198204`, ZIP
  SHA-256 `07828ee0a3dedc561860459fef0dea165c0ad53fffa60f52a18e31d7a4615154`.
  It is retained for private investigation and is a known-failing candidate.
- Local evidence: `integrated-windows-test/result-40002881667/` and its verified
  ZIP. No plaintext proprietary game files were published as a public artifact.
- Live website, backend, accounts, owner administration, live 0.6.5 download,
  working 0.4.4 companion and rollback releases were not replaced.
- Matchmaking remains disabled. Physical Xbox loading, lobby, graphics and
  controller behavior remain unverified. No additional Xbox retry is needed to
  establish this already-reproduced Windows failure.
