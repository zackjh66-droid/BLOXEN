# Private full-content package option

This is an implemented build option, not a finished Fortnite port. No original
Fortnite entry point/loading screen/lobby has been verified. Existing releases,
archive, accounts and deployments are unchanged by this option.

## What changes

`Build.ps1 -PrivatePreparedDirectory ... -PrivateContentDirectory ...` requires
the real prepared client/dependencies AND all 139 pinned non-code content files.
The build copies only verified manifest members. It rejects links, unexpected
files, collisions, changed sizes/hashes and insufficient staging space. It
audits the content hashes again inside the signed APPX.

The signed `bundled-content.json` marker binds to the exact signed content plan.
The launcher requires its linked account and hashes all installed data before
the existing real-client launch call. This mode never downloads missing content:
a damaged/incomplete package produces a reinstall error. The native file bridge
uses installed content for reads and wildcard enumeration, rather than stale
LocalState imports. Saved files remain in LocalState. No runtime dependency or
unsupported API check is bypassed.

## Reproducible private build

Use an authorized hosted Windows runner with VS/UWP and SDK 19041 as for the
existing native build. Keep game-containing inputs/artifacts private. Prepare
the existing converted native code via `prepare.py` as documented separately.
Use a fresh checkout; no existing game data is overwritten.

```powershell
python private-package/bundle_content.py C:\PrivateInputs\6.21.zip
python private-package/bundle_content.py C:\PrivateInputs\6.21.zip --stage C:\PrivateInputs\FullContent
.\Build.ps1 -OutputDirectory C:\PrivateOutput -PrivatePreparedDirectory C:\PrivateInputs\PreparedClient -PrivateContentDirectory C:\PrivateInputs\FullContent
```

The inspection/staging command hashes the original archive before extracting.
It never executes game code or supplied launchers. Staging failure retains the
incomplete directory without a ready marker; inspect it rather than reusing it.
Build script compilation/signing and exact Xbox sideload size acceptance still
need verification for a full-content package. The certificate export is public
only; private signing keys remain non-exportable in the hosted certificate store.

## Size and trimming evidence

The preserved archive is 12,528,679,454 bytes with SHA-256
`bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5`.
The selected 139 data files expand to 26,449,827,840 bytes; their existing ZIP
compressed members total 12,356,022,452 bytes. These are measured ZIP sizes,
not an MSIX size prediction. Signing/compression must be measured on the result.

An 8 GB target is not established. The six main PAK chunks already occupy more
than that under the existing ZIP compression. Their asset dependency inventory
is unavailable: the smallest examined split has an encrypted v7 index; earlier
bounded scans of all six did not validate a plaintext index. Deleting whole
chunks could remove shared lobby, character, UI and shader dependencies. No
chunks, languages, movies or original Epic assets are trimmed in this option.
Third-party launchers/scripts are excluded by the existing code/data allowlists;
their removal is not evidence of a pristine original Epic distribution.

## Remaining blockers

The current Linux workspace has approximately 11 GB free, less than the 26.45 GB
expanded content alone. A complete build also needs separate preparation, app
staging, intermediate output and compressed-package headroom. No full-content
APPX was produced here. The hosted Windows private input route previously failed
to obtain the entire archive; its code-prefix workaround supplies native images
only, not the PAK data. A complete private input transfer plus sufficient runner
disk is required before this build option can produce its full package.

The user's Xbox log independently reports 42 missing platform bindings out of
838, including `USER32.dll!LoadCursorFromFileW`. Bundling assets does not implement
those APIs. Native Fortnite login/presence and lobby execution remain unverified.
Matchmaking stays unavailable. No diagnostic-only package is offered as the game.
