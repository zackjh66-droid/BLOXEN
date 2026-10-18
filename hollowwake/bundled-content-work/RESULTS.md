# Hollowwake bundled-content implementation checkpoint

Implemented source: `fb8384a4caeb4fe527212e44111203e63af58e15` on
`zackjh66-droid/BLOXEN`, branch `hollowwake/native-system-route`.

The native/C#/XAML/build changes were compiled and development-signature verified
on GitHub's existing Windows runner, run 39151477260, job 117745839840. The tested
commit was 14758e030a364e7ca67ddca499d1fdeac709835c. The subsequent commit only
tightens Python preparation validation and adds one staging regression test;
its eight Python tests pass locally. The Windows run passed seven staging tests,
the shared C# transfer tests, and both production native file bridge modes.
The bundled fixture deliberately places different bytes in LocalState: the
installed package data is read instead. Saved-file writes still reach LocalState.

The Windows check built the source/check package, not the full private client
with all game data. Its package is not being presented as a game deliverable.
No game execution, original entry point, loading screen, lobby, live Fortnite
authentication, or new Xbox success is claimed.

Full archive SHA-256 was rechecked against the original pin. The 139 selected
data files expand to 26,449,827,840 bytes and occupy 12,356,022,452 bytes using
their existing ZIP compression. There is no measured full MSIX size. Even all
61 loose movies together occupy only 1,018,380,116 compressed bytes, so removing
movies would not establish an 8 GB package and could remove needed cinematics.
No original assets were trimmed; no original archive bytes were modified.

Local disk has about 11 GB free. Full extraction is blocked before it begins by
the space check; no incomplete full-content package is delivered. The exact
expanded requirement exceeds that space by over 15 GB before preparation,
packaging and signing intermediates. A complete private Windows input/build path
with enough disk remains needed. The source's final-range probe returned HTTP200
with ZIP-header bytes instead of the requested final range: that mirror does not
honour this request, so it cannot currently repair a truncated transfer by Range.

Independently, the user's Xbox preflight reports 42 missing platform bindings
out of 838, including USER32.dll!LoadCursorFromFileW. This implementation changes
content delivery; it does not pretend to fix those native startup failures.
Encrypted/opaque PAK dependency inventories prevent a verified lobby-only trim.
Native Fortnite protocol/login/presence and lobby remain incomplete/unverified.
Matchmaking remains disabled. Website/accounts/admin and working rollback were
not deployed over or replaced in this checkpoint. The preserved private0.5.1
APPX still hashes to
86dc1f339e9c40e82f4cdc54d84ec06650b149e93fee29420cff4ec7fbe33f0c.
