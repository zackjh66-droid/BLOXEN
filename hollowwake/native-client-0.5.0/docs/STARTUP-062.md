# Hollowwake 0.6.2: observed Xbox client pointer-write repair

Experimental integrated client package; original loading screen and lobby
remain unverified. Matchmaking stays disabled. No game data is public.

The Xbox 0.6.1 report identifies an access violation in client code at RVA
0x05FA3A7B, `mov dword ptr [rcx], eax`, with a low write target. Static
inspection finds immediately preceding ECX arithmetic that truncates an
RBP-relative address. The same instruction window exists in the preserved
original 6.21 input. The structured crash-upload schema did not retain the
access/target fields; the on-device summary did.

Private preparation now requires the exact prior converted image SHA-256
and exact instruction bytes. It replaces only the preceding nine-byte
address calculation with a direct branch to appended RX code. That code
executes the original arithmetic (preserving its flags), restores RCX to
the full 64-bit effective address with flag-neutral LEA, then branches
back to the original write. It does not skip the write, an import check,
account validation, or an engine initialization operation. No new call,
stack manipulation, writable executable section, or absolute relocation
is introduced. TLS, imports, exports, unwind, and relocation directories
and DLL entry remain unchanged. Source game inputs remain unchanged.

The prepared client grows by one 512-byte aligned file block. Its new
size/SHA-256 are enforced by the actual native Launch path and included
in the package's installed-code manifest. Updating the package reuses the
existing verified game-content files. It does not require downloading
the 26.45 GB asset set again.

Validation scope:

- Linux owned x64 arithmetic fixtures reproduce the original high-address
  fault in an isolated child, verify repaired high and low writes, and
  compare arithmetic flags. No game executable is run.
- Windows owned x64 fixture checks the same writes, arithmetic flags,
  preserved nonvolatile register, and RW-to-RX direct branches. Its result
  must be reported separately from compilation and package signing.
- Complete private preparation must regenerate all code hashes from the
  preserved exact 6.21 inputs. No arbitrary alternative build is accepted.

Xbox checklist after compilation, tests, signing, and delivery pass:

1. Install this experimental update in place, retaining the companion,
   previous rollback installers, sign-in, and LocalState game content.
2. Select Launch once after content verification completes.
3. Record whether an original game screen appears. If it returns Home,
   reopen and capture Show Launch Log's initial failure summary, including
   exception, module, RVA, access type, and target. Do not re-download
   content merely because client initialization faults.

This is a targeted repair of one observed instruction sequence. It does
not establish successful complete engine startup or eliminate the need
to investigate the next concrete failure on physical Xbox hardware.
