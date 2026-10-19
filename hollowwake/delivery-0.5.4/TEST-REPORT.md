# Hollowwake 0.5.4 test report — 2026-10-19

Code commit: 7665a33a40559d74b969030b349ebcfaf327c79a.
Hosted build: https://github.com/zackjh66-droid/BLOXEN/actions/runs/39297425168 .
MSIX: 127,549,306 bytes; SHA-256 e3118b550a9a6cbb314b56c3c3dec75ed27f4396787a4bbb71b76c658c317963.
Installed code: 38 files, 224,256,272 bytes, independently checked against the signed code index after private artifact recovery. Windows signtool verified the development signature; the public CER matches. No private signing/recipient key is delivered.

Windows native/C#/XAML compilation, signing, original HTTP transfer/streaming tests, final-path rehash, measured already-installed reuse, corruption, post-download tampering, stale-marker removal, low-space failure and installed-code hash tests passed. The actual owned UWP progress bar reported its 50-percent fixture value and all menu/data controls fit within the hosted screen. That fixture did not download game data or execute Fortnite.

The updated backend passes 32 tests including exact 64-bit storage/transfer numbers, linked-session authorization, private owner downloads, revocation, account isolation and rejection of arbitrary raw paths or secrets. No database migration was required for the new structured numeric receipts.

Full 26.45 GB installation on Xbox using this update remains unverified. The user's earlier 0.5.3 screenshot reported verified content, but the user observed no storage reduction. That report had no installed-file sizes. This update is designed to measure that uncertainty, not pretend the earlier download was independently confirmed.

The retrieved user Xbox 0.5.3 failure report contained 42 missing unredirected platform bindings (16 marked main-image imports) and zero missing redirected imports. Fortnite entry was not reached. This release changes installation/measurement; those runtime requirements, original Fortnite login/presence integration and a working lobby remain unresolved. No matches or game servers are provided. The existing Hollowwake backend is live, but original game authentication against it has not been established.

Source archive/preserved game inputs, companion, 0.5.3 rollback, account database, owner-only admin and redesigned website are preserved. No Fortnite binary was executed in the hosted environment and no physical Xbox success for 0.5.4 is claimed.
