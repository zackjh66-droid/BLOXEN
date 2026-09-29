# GAMES — candidate places, evidence and decisions

Source archive: `github.com/beagleded/Roblox-Places-Archive` HEAD 51b379b (created 2023-09-21, one author, no licence, README says places are untested and may be duplicates/broken; no per-file provenance). **125/125 files parse** (`preservation/manifests/places-archive-beagleded-intake.json`): 121 pre-2015-API, 4 containing post-2015 features (Classic Basplate, Lumber-Tycoon, Marshall-Point-Lighthouse, giant rocket arena2 — not adopted).
Curated manifest: `preservation/manifests/places.json` (full per-item fields: name, creator, place ID, evidence, date, source repo/commit/path, method, public evidence, SHA-256, size, format, dependencies, grade). Files stay in git-ignored `preservation/store/<sha256>.rbx` (read-only), fetchable with `node tools/fetch_places.js <id…>` which verifies SHA-256.

| id | Title / creator | Place ID | Status | Grade | Notes |
|---|---|---|---|---|---|
| tabularasa | Tabula Rasa | — | ACCEPTED P1 | INFERRED | 10 instances, 1 part, no scripts. Smallest; used as the real-place e2e. |
| crossroads-2007-client | Classic: Crossroads, Roblox | 1818 | ACCEPTED P1 | INFERRED | commit 91f90e9 "Added /extra/ games from March 2007 client". 1835 inst, 1679 parts, 25 scripts. Preferred over HEAD version (narrower claimed origin). |
| crossroads-uncopylocked-commit | Classic: Crossroads, Roblox | 1818 | ACCEPTED P1 | INFERRED | Later file, 2155 inst, 48 scripts, 47 asset refs. |
| happyhomeinrobloxia | Happy Home in Robloxia | — | ACCEPTED P1 | INFERRED | 2010 inst, no scripts. Different title from "Welcome to the Town of Robloxia". |
| robloxhq | ROBLOX HQ | — | ACCEPTED P1 | INFERRED | 8577 inst, 2 scripts. |
| roblox-world-headquarters | ROBLOX World Headquarters, builderman | 1501 | ACCEPTED P2 | INFERRED | Binary rbxl, 8640 inst. Archive Team uncopylocked list includes 1501. |
| mission-to-the-moon | Mission to the Moon, Shedletsky | 1784 | ACCEPTED-WITH-CAVEAT | UNKNOWN | Archive Team lists 1784; file identity unverified. |
| natural-disaster-survival | Stickmasterluke | 189707 | **QUARANTINED** | UNKNOWN | No evidence of an official uncopylocked release of this file. |
| roblox-evil-game-idk-stolen | — | — | **REJECTED** | UNKNOWN | File name/content indicate a stolen copy (673 scripts). Not used. |
| (not found) | Welcome to the Town of Robloxia, 1dev2 | 20723719 | NOT FOUND | — | Only fan copies; none trusted. |
| (not found) | Work at a Pizza Place, Dued1 | — | NOT FOUND | — | Never officially open-sourced; fan copies only. |

Public/uncopylocked evidence comes from the Archive Team list (`wiki.archiveteam.org/index.php/Roblox/uncopylocked`, chunk 0 only read) and fandom pages — secondary sources. They support that the *place* was public; they do **not** prove the archive file is that exact version. Hence INFERRED, never ARCHIVED-EXACT. Mismatch of era: most places predate 2015 (2007-era Crossroads); no July-2015-era place has been found.

## Playability (honest)
Every place is labelled **GEOMETRY-ONLY**: parts, models and spawn replicate to the simulator; scripts are stored as inert data (never executed during intake or at runtime). No script runtime exists, so **no game is claimed playable as designed**. Importer output per place: SUPPORTED/PARTIAL/UNSUPPORTED/UNKNOWN report (`importerCompat`). Script inventory/classification (hashes, API use) is produced by `src/importer/analyze.js`; a safe runtime (sandboxed Lua VM, deterministic, permission-gated API) is NOT STARTED.
