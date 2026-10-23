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

## Latest actual execution

[Windows run 39909641511](https://github.com/zackjh66-droid/BLOXEN/actions/runs/39909641511)
uses the code above with no debugger attached to either comparison process.

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

The loaded main image base was `140727534616576`. The fatal execute target was
`140732927924960`, exactly image base plus `0x141775ae0` (preferred image base
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
producer of the bad operand/state has not been established. Captured
first-initialization state is not proved portable across processes; its BSS,
thread state and process registrations are not established by section/IAT
restoration. This is an unresolved technical limitation of the current
conversion, not a demonstrated requirement to change servers or reacquire
content. No jump, check, exception decision or API success is patched to hide
this failure.

## Preserved evidence and releases

- Public numeric/log/screenshot evidence: artifact `12482969145`, ZIP SHA-256
  `4e2d6abada10af0288fbd90659a3b7953bceff125d8c0e37c07f2aace787b76b`.
- Recipient-encrypted private signed candidate: artifact `12483901189`, ZIP
  SHA-256 `44eff440c018bc34923889999b138f8c10680fa50680cb639170b711d2a6f354`.
  It is retained for private investigation and is a known-failing candidate.
- Local evidence: `integrated-windows-test/result-39909641511/` and its verified
  ZIP. No plaintext proprietary game files were published as a public artifact.
- Live website, backend, accounts, owner administration, live 0.6.5 download,
  working 0.4.4 companion and rollback releases were not replaced.
- Matchmaking remains disabled. Physical Xbox loading, lobby, graphics and
  controller behavior remain unverified. No additional Xbox retry is needed to
  establish this already-reproduced Windows failure.
