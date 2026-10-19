# Hollowwake 0.5.3 automatic data delivery

This changes installation, not the evidence of Fortnite compatibility. The original Season 6 entry point, loading screen and lobby remain unverified. Do not treat downloads, account linking or successful import checks as lobby success. Matchmaking remains unavailable.

## What is installed and downloaded

The development-signed x64 MSIX contains the converted 6.21-CL-4526925 main image, the current 26-image native dependency selection, 11 generated compatibility proxies and Hollowwake's runtime/UI. Executable code must be installed in the signed package; no downloaded DLL or third-party launcher is executed from LocalState.

On first account linking or restored sign-in, the launcher verifies existing game data, checks free app storage and downloads missing selected data automatically. The signed ContentPlan621.json and ContentIndex621.json pin paths, ZIP extents, expanded sizes and SHA-256 values. The current signed manifest is installed with the app; account authorization is checked against the live Hollowwake API before and during transfer. The archive itself is an existing public community mirror, not an authenticated Hollowwake content host or an Epic authenticity reference. Hollowwake passwords, bearer tokens and cookies are never sent to that mirror.

The app requests exact HTTPS ranges, in at most 16 MiB chunks, and streams raw stored/deflated ZIP members into their final directory through a temporary file. A changed ETag, incorrect Content-Range/length/status, expansion overflow or SHA mismatch rejects that file. Only a fully verified file becomes ready. No full original ZIP or separate compressed copy is stored by this new downloader. Previously imported original archives are preserved.

- Selected compressed data transfer: 12,356,022,452 bytes (12.36 decimal GB).
- Selected expanded data: 26,449,827,840 bytes (26.45 decimal GB).
- First installation needs at least expanded missing bytes plus 1 GiB free app storage; allow approximately 27.6 GB plus the installed app/frameworks.
- 139 selected data files. Six Fortnite PAKs contain 25,328,403,641 bytes. No opaque/encrypted PAK content was removed: no verified lobby-only dependency map or successful regression run exists to justify that.
- The selection omits archive launchers/scripts, cached third-party modifications and files outside the existing native/data inventory. Static dependency closure does not establish that every dynamic client dependency is present. The original archive is unchanged.

Use PAUSE (B) to stop and DOWNLOAD / VERIFY CONTENT to retry. Already verified files survive process restarts. A disconnected read resumes within the same active transfer; restarting the process restarts the unfinished file. The app deliberately rehashes existing selected files on the next startup rather than trusting a writable marker alone. Loss of account authorization stops the transfer; it does not turn an incomplete directory into ready game content.

## Account and diagnostics

Existing website accounts, approval, protected persistent sign-in and owner-only admin are preserved. User-reported Xbox evidence confirms earlier companion linking only. Fortnite-compatible in-client login, presence, profile and shop integration remain separate unfinished work. No Epic credentials should be entered.

The app submits structured private import/failure reports after sign-in. If native code closes the app, reopening submits the incomplete attempt before beginning a new audit. Reports include exact missing bindings, stages and numeric errors, not passwords, tokens, public machine paths or arbitrary raw logs. Raw logs remain in the app's LocalState; the owner dashboard can review the structured reports. No hundreds of screen photographs are needed.

The native launch button still enforces preflight. It calls HWAttemptPackagedClient only after authenticated content verification and required import checks. The known 42 missing unredirected Xbox bindings from the user's older 0.5.0 report have not been retested on this hardware. This update includes an actual MSVC stack-probe binding and cursor-file fallback, but does not claim the remaining bindings or game execution succeed.

## Reproducible hosted build

Use the existing repository branch hollowwake/native-system-route. The package code revision is recorded in the delivery report. The standard public windows-2022 workflow builds original runtime fixtures and the owned UWP app, then reads pinned private game inputs, prepares copies and builds/signs the separate private package. Private files are recipient-encrypted before Actions artifact export. No private signing key is exported or delivered. No paid runner was provisioned.

The checked-in workflow is .github/workflows/hollowwake-native-client-050.yml, with a source copy at hollowwake/native-client-0.5.0/workflow.yml. It uses VS2022, Windows SDK 10.0.19041.0, the declared UWP NuGet dependencies, Python pefile 2024.8.26 and the existing pinned source manifest. Build.ps1 with -PrivatePreparedDirectory creates 0.5.3.0 MSIX. Without private inputs the output is an owned public preflight APPX; that output is not a game package.

Reproduce the content index from the unchanged private archive without extracting or running it:

    python hollowwake/native-client-0.5.0/private-package/index_content.py <private-original-6.21.zip> hollowwake/native-client-0.5.0/App/ContentPlan621.json hollowwake/native-client-0.5.0/App/ContentIndex621.json --verify

The tool rehashes the original archive before comparing all selected local/central ZIP headers and extents. The original archive SHA-256 is bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5. Per-file reference hashes were measured from that preserved community archive; they are not Epic-published release hashes. Build procedure is reproducible; signatures/timestamps are not byte-identical between builds.

## Xbox test scope

Install the MSIX and included x64 dependencies via Xbox Device Portal. Keep the working companion installed as a separate rollback. The public CER accompanies the development package; no private key is needed. Do not uninstall the current native identity if you want its LocalState data retained during an update.

1. Confirm the linked account restores (or complete a fresh explicit device approval).
2. Confirm the automatic content progress starts, Pause responds to controller B, and Retry retains verified files. Record free storage and any exact download failure.
3. After all content is verified, invoke the actual native-client launch once. Record whether an original Fortnite loading screen is shown, an exact import/loader failure appears, or the app closes.
4. Reopen after a closure and remain signed in to send the structured report. A passing preflight is not game execution evidence.

If the OS exposes Dev Home > app details > App type, selecting Game requests the documented game resource allocation; exact UI labels and its necessity for this app are not verified on hardware here. It does not provide desktop Win32 compatibility. See https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/system-resource-allocation .

Historical documents under native-client-0.5.0/docs describe older checkpoints. This document and the dated package test report supersede their package version and download method descriptions.
