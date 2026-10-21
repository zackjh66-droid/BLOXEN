# Hollowwake 0.6.3: complete x64 return-frame adaptation

Experimental integrated real-client test package. Original loading screen and
lobby remain unverified. Matchmaking remains disabled; retain existing content,
accounts, companion, and rollback installers.

## Hardware finding and previous repair gap

Xbox 0.6.2 reports exception 0xC0000005 at client RVA 0x05FA3A8D, the RET
instruction, after 0.6.1 faulted on a write at RVA 0x05FA3A7B. The new report
says access=read and target=18446744073709551615. That value is the reported
fault address; it is not proof that the saved return address itself was -1.

The previous repair covered a single effective-address calculation. Its owned
fixture checked writes and flags, but did not return through the surrounding
frame. This was insufficient: the preceding sequence truncates the computed
64-bit target through EAX and a four-byte local, then writes four bytes at
RBP+0x8C. The actual x64 epilogue sets RSP=RBP+0x80, pops an eight-byte RBP,
and consumes an eight-byte return slot at RBP+0x88.

## Implementation

Private preparation now adapts the complete value/address/write sequence.
It retains the client's own RVA-plus-module-base target in RAX, stores/loads
the full eight-byte local, preserves the original ECX arithmetic flags,
addresses RBP+0x88 with LEA RCX, and writes all eight bytes before the unchanged
original epilogue. The target is calculated by the client, never replaced with
a fixed continuation, skipped check, successful return, or lobby placeholder.

The repair requires the exact preserved pre-repair converted SHA-256 and exact
17-byte sequence plus 17-byte epilogue. The input archive/client source remains
unchanged. Direct relative branches and appended RX code are used; no extra
stack adjustment or call is introduced into the client's repair. PE imports,
exports, TLS, relocations and unwind directories remain unchanged. Original
import inventory counts are unchanged; this does not validate optional Xbox
features. The native Launch action enforces the newly prepared DLL's size and
SHA-256, and installed-code verification uses the generated package index.

New structured reports preserve fault access type (0/1/8) and target as numeric
strings, including full 64-bit values. No raw log, stack dump, register dump,
credential, URL or file contents is uploaded by this change. Existing device
authentication and report-access controls remain required.

## Validation scope

The owned fixture now uses the production-generated trampoline and complete
frame/epilogue. It computes a real continuation, executes RET through the slot,
and verifies RSP/RBP/RBX/RSI and arithmetic flags. All four combinations of
low/high frame and low/high code addresses are exercised. Isolated Linux child
tests demonstrate that the original and 0.6.2 partial sequences fault, rather
than counting a successful helper compilation as startup validation. A Windows
x64 execution test covers the same complete frames. Report its actual result
separately from compilation and signing.

Separate private CPU-analysis models use explicitly synthetic OS inputs and
stop at unimplemented imports or execution limits. They are not Windows/Xbox
execution evidence, do not supply production API behavior, and do not establish
that the complete client starts. No model or synthetic API enters the package.

## Xbox test after build/signing verification

Update in place; keep the 26.45 GB existing game data. Let verification reuse
the existing files, then select Launch once. Record whether an original game
screen appears. If it returns Home, reopen the launcher to upload the sanitized
crash report. This version is a targeted integrated execution attempt, not a
demonstrated functioning Fortnite lobby. Do not repeat 0.6.2 launches or delete
game data to diagnose a native return-frame fault.
