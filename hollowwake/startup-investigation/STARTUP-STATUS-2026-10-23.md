# Hollowwake actual-client startup status — 23 October 2026

Status: blocked in converted-client startup. No verified Hollowwake lobby or
working Xbox game package is claimed. Private 0.6.15–0.6.20 candidates
are not replacements for the live download.

The Windows desktop used here is a temporary Microsoft-hosted GitHub Actions
Windows Server 2022 machine in the project's repository. Source editing uses
the Linux cloud workspace. Neither machine is the user's PC or Xbox.

## Current implementation

Latest actual-tested source: `4b7bfa78c8e2dfa3d939bc4816badf834f82cb1a` on
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

## Latest integrated execution — private 0.6.20

[Windows run 40031157902](https://github.com/zackjh66-droid/BLOXEN/actions/runs/40031157902)
compiled and signed the private package, verified all 139 selected data files
(26,449,827,840 bytes) and 53 code files, completed normal account linking and
invoked the actual Launch button without a debugger. Fortnite's original entry
thread, engine initialization and `/Game/Maps/Frontend` load were observed.
The original native HWND/DXGI request returned HRESULT 0. Visible original menus,
Fortnite login and the lobby were not demonstrated. Xbox remains untested.

The process exited with `0xC0000005`. Its loaded base was `140734150213632`.
The retained indirect-jump target was `140739543522016`, exactly loaded base
plus `0x141775ae0`. This target is noncanonical under the machine's x64 address
layout, so Windows reported a read of `UINT64_MAX` at the original
`jmp [rsp-8]`, RVA `0x609753b`, rather than an execute fault at the target.
The invalid transfer remains unresolved. No jump or decision is changed.

The header-state operation logged **already matches current module**. It did
not change the actual client's header in this run and did not resolve the
transfer. Correcting its ordering removed 0.6.19's pre-entry access denial; that
is separate from repairing the actual game failure. All four unchanged Windows
binding inventories below again had zero unresolved entries. No reduction of
these inventories or Xbox compatibility is claimed.

Evidence artifact `12536033207`, 9,456,438 bytes, has ZIP SHA-256
`71f188cba9a295ac20b422b27f202a64281bc13c3fad6f1718a0897633055af1`.
The failed signed candidate is retained recipient-encrypted in artifact
`12536805512`, ZIP SHA-256
`9ee6ed20c0072a506c898b4bbe3a82c6d4c615b1caf0e7eaf14841deebce0b0e`.
It is not promoted to the live download. The unchanged original control stayed
alive for all 180 seconds; its isolated Epic sign-in is not Hollowwake login.

The next internal investigation expands the read-only post-fault capture to this
validated noncanonical branch form, with retained source-table RVAs and recognized
normalization-base identities. It changes no execution state and is not a
client-startup repair or requested Xbox release. Owned tests exercise a real
Windows noncanonical exception, bounded high-address mappings and rejection of
mismatched records, instructions and retained stack targets.

## Earlier ordinary actual execution — private 0.6.18

[Windows run 40020543600](https://github.com/zackjh66-droid/BLOXEN/actions/runs/40020543600)
uses private version 0.6.18, including section-preserving import preparation,
managed startup/focus guards, preserved
original DXGI instructions, the real native HWND route and post-fault
observation. Neither comparison process had a debugger attached.

The private package compiled, signed and passed package-content hash checks.
The normal account/device-link flow and actual Launch button were exercised.
All 139 selected data files (26,449,827,840 bytes) and 53 code files verified.
The preserved archive is 12,528,679,454 bytes, SHA-256
`bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5`.
The exact original executable remains SHA-256
`9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478`.

The actual converted client reached its original entry thread, initialized its
engine, created a real native HWND D3D11 chain and completed loading
`/Game/Maps/Frontend`. It then exited with `0xC0000005` before visible menus or
a lobby were established. Fortnite-compatible login has not been demonstrated by this execution.

The loaded main image base was `140716194398208`. The fatal execute target was
`140721587706592`, exactly image base plus `0x141775ae0` (preferred image base
`0x140000000` plus `0x1775ae0`). The new read-only post-fault observer verified
the exact original helper instructions at RVA `0x6097501`, both retained stack
target slots, and the target arithmetic. Its recorded operand was the loaded
image base itself; its constant was `5393308384` (`0x141775ae0`). No debugger,
context changes or exception-disposition changes were used.

The retained frame contained candidate comparison index 0, left DWORD
`3119817473`, right DWORD `2582946561`, and a right-side image location at RVA
`20332922` (`0x136417a`). Other predecessors of this helper exist. These frame
values do not prove which comparison branch executed or identify an integrity
check. The producer/context error remains unresolved; the jump is not patched.

The original unmodified swap-chain request (800×600, format 24, one buffer,
flags 2) returned HRESULT 0 from the real native factory. Preserving the three
original graphics instructions did not resolve the later invalid control
transfer. Managed content verification and actual Launch completed in this run.
Evidence artifact `12530879752` has ZIP SHA-256
`684ef0c6ec830a69c8a6c0aa75cff31d200a0b3e57f71bf4695cf777ce78e3de`.

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

## Tested section-preserving import preparation

Source `051ea6a65219a571f9ef4a15ca165338216f2f60` / private 0.6.18
relocates the main image's normal import descriptors and proxy names into an
owned appended area. It preserves original normal import-name/descriptor data
in `.idata`, original thunk RVAs and all original delay descriptors. Previous
normal import-name changes were in writable `.idata`, not executable `.text`.
The actual private preparation uses this path; dependencies and rollback mode
retain their existing route. The two original executable raw sections are
independently compared with the pinned initialized input during preparation
and build. No checks, branches or instruction operands are changed.

Local layout validation against the real canonical client passed: 45 normal
descriptors / 821 bindings, 19 delay descriptors / 260 bindings, preserved
original section bytes. Deliberately changing an instruction fails the audit.
The main image currently has no redirected delay-provider descriptors. Python
and PowerShell syntax checks passed. The owned Windows loader lifecycle test passed (real imports, entry, thread/TLS,
unwind and forwarded exports). The signed integrated actual-client run
`40020543600` completed with the failure above. This layout correction did not
resolve that transfer. The unchanged original comparison stayed alive for all
180 seconds. The normalized standalone comparison was explicitly skipped
because it requires native-host import rebinding and permission restoration.

The latest retained comparison DWORDs are `0xb9f4a301` and `0x99f4a301`,
a difference of `0x20000000`. Static references locate the left array at the
former frame plus `0x68`, and the right array at original RVA `0x136417a`.
This narrows the investigation; it does not prove that a DLL flag or any
particular image-validation decision caused the transfer. No expected checksum,
check result, header flag or bad jump has been patched to force continuation.

## Failure report receiver correction

Production logs showed current diagnostic POSTs rejected with HTTP 400. The
live receiver's strict allowlist omitted seven structured event types already
emitted by the launcher, including dynamic import audits and bounded crash
stack candidates, and omitted `stack_index`. The same payload was reproduced
as rejected by the old validator and accepted by the corrected validator.
The correction accepts only the known event types and bounds candidate indices
to 0–63; arbitrary fields, raw logs, credentials, unknown events and unauthenticated
uploads remain rejected. Nine backend tests passed, including real linked-device
authentication, account-scoped reads, signed owner access and rate limits.

Sites version 49 / source `74d59551e438a1b5b93c2f2d30cbcc9806ac88c7`
is live at https://hollowwake.jihnjohn10.chatgpt.site . Deployment
`appgdep_6adbcbda8c548191924f4211807c85f4` succeeded, environment revision
35 preserved. Existing client download/release hashes, accounts, website
presentation and rollback are unchanged. Live `/api/status` returns 200 and
anonymous diagnostic reads/uploads return 401 using the owned verification
client. Python urllib's default user-agent receives upstream Cloudflare 1010;
that is separate from the application validator.

The private signed 0.6.18 candidate remains recipient encrypted in artifact
`12531053951`, ZIP SHA-256
`91d3729b0d98426ad5555369034212ce689f15a36c1c12d47f2185ee21b16e4a`.
It was not promoted as a working game or offered for another Xbox retry.

## Measured initialization repair under test — private 0.6.19

Source `a306a08acc75dd7c7ff1babbca68d1e2e458d276`, run
`40027567703`, adds validation of an observed original initialized-header state to native
client startup. The preserved original runtime header is exactly the source header
with `OptionalHeader.ImageBase` (offset `0x200`) changed from preferred address
`0x140000000` to that original process's loaded module address. All other
header bytes are identical. No header DIR64 relocation exists, so the OS
relocation table does not describe that field transformation. This does not
identify its writer or prove that the native OS loader leaves it incorrect.

The normalizer now verifies that complete observed header contract and records
the rule rather than replaying a captured address. After the real packaged DLL
is loaded, production startup validates the live image/header and atomically
sets only that field to the current module address, restoring actual page
permissions and propagating failures. Already-correct state is accepted. The
original instructions, DLL flag, entry, checksums, check results and exception
dispositions are unchanged. The signed disk image retains its preferred base
for normal OS loading. This records a measured initialized-state rule. The later 0.6.20 run confirmed
that the native-loaded header was already correct; it did not fix the transfer.

Existing owned Windows tests now cover exact header-byte preservation, real
protection failure, idempotence, offset rejection and restoration. The real
converted-DLL lifecycle fixture also tests original entry, static TLS, threads,
unwind, module identity, forwarded exports and unload after this transformation.
Python and PowerShell syntax checks and the original owned Windows checks
passed. The integrated 0.6.19 run then returned `0x80070005` before creating
Fortnite’s entry thread: captured executable-copy header permissions had been
applied before the non-executable header write. This run did not test whether
the header-state repair resolves the earlier game crash. No new public download
is claimed.

The 0.6.20 correction moves header initialization before permission replay and
shares that complete production sequence with the converted-DLL regression
test. The regression reproduces the denied late write under the actual source
header protection (`PAGE_EXECUTE_WRITECOPY`), then verifies the correct sequence
and subsequent native lifecycle. It keeps the access check intact. Startup
failures now use the existing `NATIVE_STARTUP_FAILED` diagnostic event so their
HRESULT is accepted by the current report schema. The complete fresh 0.6.20
execution test completed with the unresolved actual-game failure above. The header transformation is an observed
initialized-state contract; the capture does not independently establish which
initialization component originally wrote that field.
