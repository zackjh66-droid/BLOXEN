# ASSETS

Table `assets` (SQLite, `src/lib/db.js`): id, type id/name, name, source (archive URL), source date, provenance grade, availability, note; `asset_uses` links assets to places; `missing_asset_log` records every request for a MISSING asset; local path and SHA-256 columns exist for assets that are actually stored.

Current state: **every catalog asset is MISSING** (only metadata archived). Place-embedded asset ids (e.g. Crossroads 1, World HQ 29) are recorded as references only.

Asset service (`/asset/?id=` on the compat server): numeric ids only (400 otherwise); MISSING → 404 with body "MISSING" plus a log row; **never** proxies to Roblox and never returns a homemade substitute. Avatar wearables that are MISSING are skipped by the game server and reported (`missingAssets`), which the e2e test asserts. Body colours are data, not assets, and do reach the spawned character (asserted).

No uploads are accepted anywhere (removes the malicious-upload surface). To add real assets later: place the retrieved file under `preservation/assets/<sha256>`, record source/date/SHA-256/grade, and flip availability — no code path exists yet (NOT STARTED).
