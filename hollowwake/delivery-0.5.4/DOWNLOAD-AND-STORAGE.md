# Hollowwake 0.5.4 — measured automatic installation

This update addresses data delivery and proof of installed files. It does not resolve the 42 Xbox platform bindings reported by the user's 0.5.3 launch. Fortnite's original entry point, loading screen and lobby have not been demonstrated.

The signed MSIX installs the modified Season 6 client/runtime code. Downloading executable DLLs into LocalState is not used for native loading. The new signed package-code-index.json records hashes/sizes for 38 installed code files: the selected 26 converted client/dependency images, 11 compatibility proxies and Hollowwake's native runtime. The app rehashes them before checking/downloading data.

After a linked account signs in or restores its session, missing selected game data downloads automatically. A visible progress bar shows received compressed bytes, expected download bytes, transfer speed, verified file count and verified expanded bytes. Client code has its own measured size/count line. The 139 data files contain 26,449,827,840 expanded bytes; a complete new transfer selects 12,356,022,452 compressed bytes from the preserved pinned archive. The app needs missing expanded bytes plus 1 GiB free app storage. It does not save another full ZIP.

The source remains https://public-build-archives-bucket.erafn.dev/6.21.zip. Each selected range, expanded length and SHA-256 is pinned by signed metadata. Hollowwake credentials/cookies are not forwarded to the archive. This is a community source; its hashes are not Epic-issued authenticity references.

Readiness now requires an independent second pass over actual final paths, hashing every selected file and comparing their measured sizes and count. A writable ready marker or an in-memory set cannot establish completion. Stale markers are removed before an audit; cancellation, low space, corruption and changed final files leave readiness false. Unneeded launcher/scripts and cached modifications remain excluded; encrypted PAK contents are not stripped without a verified dependency map.

If files are already present and valid, the app shows zero new transfer bytes and the actual verified installed bytes. It does not redownload them solely to make the counter move. The Xbox console's overall storage summary is not the measurement used by the app. File lengths and available app-volume storage before/after are recorded separately; no claim is made that an outer console summary updates immediately or represents the same volume.

New structured private reports include CONTENT_INVENTORY, CONTENT_TRANSFER, CONTENT_DATA_READY/PENDING, CONTENT_CODE and available app-volume free-space counters. Their numbers are not lost when launching resets the attempt-log offset. Completion/failure submits the report automatically. These reports contain numeric receipts, not raw directory listings, account secrets or arbitrary file contents.

Account linking/profile APIs and the experimental client621 HTTP adapter use the existing Hollowwake Sites origin. Fortnite's original login/presence protocol integration is still incomplete. An allowed backend hostname and a successful Hollowwake session do not prove the original game can authenticate. Matchmaking remains unavailable. No match server or paid resources were started.

Install as an update to Hollowwake.NativeClientExperiment to preserve its app data. Keep the separate working companion and the 0.5.3 rollback. Do not uninstall merely because verification finds existing files. The source/build inputs and original archive remain unchanged except for the explicitly recorded source copies and regenerated private package.

Expected Xbox check: observe client-code verification, data checking, then either a real byte-counted transfer or an explicit measured reuse. On completion, the selected data count must be 139/139 and 26.45 GB, with measured client code shown separately. Pause and retry retain verified files. Original Fortnite execution remains a separate blocked check; this release must not be described as a functioning lobby.
