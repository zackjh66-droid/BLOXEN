# Hollowwake 0.5.9 private content transport

Experimental actual-client package; no verified Xbox loading screen or lobby.
The signed package retains the existing converted 6.21-CL-4526925 client and
compatibility DLLs. This version changes the asset transport, not the unknown
0.5.7 native crash. Matchmaking remains disabled.

The launcher requests /api/device/content-lease using its linked Hollowwake
device session. Sites authorizes the existing private-download owner and obtains
a short-lived private Blob read URL from hollowwake-content. The server's HMAC,
Vercel protection credential and storage-write authority never enter the app.
No browser or project session cookie/bearer is sent to Blob. Each 16 MiB range
asks the lease provider for a current URL; the provider renews two minutes before
expiry, including during a large PAK. Exact host/path/build/archive size and
SHA-256 are pinned. The immutable signed 139-file inventory remains unchanged.
There is no silent fallback to the old external archive host in this app flow.

Only size/SHA-256-verified files are committed. All 26,449,827,840 selected bytes
are retained, including every selected PAK and signature. A final independent
WinRT pass opens each installed file. The full ZIP is never retained. The original
archive is 12,528,679,454 bytes; no game data was trimmed speculatively. The
modified executable/runtime are installed by MSIX, not downloaded as loose code.

Update the experimental package in place to preserve valid data and linking.
Keep the working companion and earlier rollback release. Install the x64 MSIX
with its three x64 framework dependencies using Xbox Device Portal. Allow at
least 27.6 GB free app storage for missing selected assets. Signing certificates
are public; no signing private key is distributed.

Xbox checklist: link account; observe actual received bytes while missing files
are downloaded; verify 139 files and independent WinRT storage checks; use Full
log's exact running LocalState path to inspect GameClean; attempt Launch Native
Client; if it closes, reopen to upload its sanitized crash/module/RVA report.
No new Xbox outcome is inferred from hosted compilation or HTTP fixture tests.
