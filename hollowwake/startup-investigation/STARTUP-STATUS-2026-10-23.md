# Hollowwake actual-client startup status — 23 October 2026

Status: blocked in converted-client startup. No verified Hollowwake lobby or
working Xbox game package is claimed. The private 0.6.15 candidate is not a
replacement for the live download.

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

[Windows run 39939149207](https://github.com/zackjh66-droid/BLOXEN/actions/runs/39939149207)
uses the runtime above, with test instrumentation source
`90896474fa840a0a43ae005ae06cf87595415bcb`. Neither comparison process had a
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
a lobby were established. The game exchange was issued but was not consumed;
Fortnite-compatible login has not been demonstrated by this execution.

The loaded main image base was `140725066596352`. The fatal execute target was
`140730459904736`, exactly image base plus `0x141775ae0` (preferred image base
`0x140000000` plus `0x1775ae0`). Earlier bounded trace evidence located this
same target calculation at original helper RVA `0x6097501`. This latest normal
run confirms the same out-of-image target pattern without a debugger; it does
not itself capture that helper's internal operands.

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

The replacement ordinary integrated Windows run is `40002881667`. Its actual
execution result must be assessed before making further claims or changing the
live download. No new public diagnostic-only package was released.

## Preserved evidence and releases

- Public numeric/log/screenshot evidence: artifact `12495898831`, ZIP SHA-256
  `ec31f9826da22fc39edd40712e540e9cb8ef26fe30ec08711f82be2126a26ed1`.
- Recipient-encrypted private signed candidate: artifact `12497000789`, ZIP
  SHA-256 `2a201183036786b1b35028c1bcf8cbdbde5153a3b5a4bc0b0b828b16e502d9cc`.
  It is retained for private investigation and is a known-failing candidate.
- Local evidence: `integrated-windows-test/result-39939149207/` and its verified
  ZIP. No plaintext proprietary game files were published as a public artifact.
- Live website, backend, accounts, owner administration, live 0.6.5 download,
  working 0.4.4 companion and rollback releases were not replaced.
- Matchmaking remains disabled. Physical Xbox loading, lobby, graphics and
  controller behavior remain unverified. No additional Xbox retry is needed to
  establish this already-reproduced Windows failure.
