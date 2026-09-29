# PROVENANCE

Grades used everywhere in BLOXEN (most to least trustworthy):
`PRESERVED` (original artifact from the rights-holder's own preserved distribution) · `ARCHIVED-EXACT` (archive capture of the exact object/date) · `ARCHIVED-NEAR-DATE` (capture close to, but not at, 2015-07-23) · `RECONSTRUCTED-FROM-EVIDENCE` (rebuilt, each choice tied to cited evidence) · `INFERRED` (best guess, no direct evidence) · `UNKNOWN`.

A grade describes **the artifact BLOXEN holds**, not the historical existence of the thing. File names are never provenance.

## Component register

| Component | Artifact BLOXEN holds | Grade | Basis / what is NOT proven |
|---|---|---|---|
| Windows Player 0.205.0.61876 (`RobloxPlayerBeta.exe`) | 771-file tree from `KloBraticc/2015-Client` @7a0742c (quarantine, **never executed**) | **ARCHIVED-NEAR-DATE** (strong, not PRESERVED) | SHA-256 matches the required value; PE timestamp 2015-07-23T19:45:30Z; Authenticode signer `ROBLOX Corporation` with Symantec countersignature 2015-07-23T19:51:54Z (digest + signer signature + issuer-chain links checked; root trust, revocation, countersignature NOT checked); `ReflectionMetadata.xml` byte-identical to RobloxAPI/build-archive. The other 769 files are not independently verified. Source repo is a GitHub mirror-of-a-mirror with no licence. A hash match is **not a safety claim**. |
| Client API dump | `preservation/reference/api-0.205.0.61876.json` from RobloxAPI/build-archive | ARCHIVED-EXACT (version-keyed) | 332 classes / 851 properties / 266 events / 124 enums. The client's own tables are larger (968/320): the gap is presumed hidden members, UNVERIFIED. |
| Client deploy record | `setup.rbxcdn.com/DeployHistory.txt` (Roblox's own public deploy log, read via Wayback-independent fetch): `New WindowsPlayer version-0d46087630eb46cd at 7/23/2015 11:33:45 PM, file version 0,205,0,61876`; manifest `preservation/manifests/deploy-history-2015-07.json` | ARCHIVED-EXACT for version/time/file-version (timezone not stated in the file) | Independently agrees with RobloxAPI/build-archive and the PE `FileVersion`. RccService `version-953dbb2d418145cc` (11:43:14 PM) and Studio `version-c996ec7e054749b8` (11:48:34 PM) from the same deploy were **not acquired**. A version line is not a file hash: it does not prove the 771-file tree is byte-identical. |
| Logged-out Home page text | Wayback 20150723171630 | ARCHIVED-EXACT (text) | Layout/CSS not retained → layout is RECONSTRUCTED-FROM-EVIDENCE. |
| Games page structure (sorts, filters, genres, launch-overlay text) | Wayback 20150726062742 | ARCHIVED-NEAR-DATE | Game list is JS-loaded and absent from the capture. |
| Logged-out Home/Login/Register layout | Web Design Museum 2015 screenshot (500px reduction, quarantine only) | ARCHIVED-NEAR-DATE (structure); RECONSTRUCTED stylesheet; all imagery MISSING | sizes are proportional estimates |
| Other pages' CSS values | RobloxLabs/web (MIT fan remake carrying real 2012-14 CSS), reference only, not redistributed | ARCHIVED-NEAR-DATE for values; RECONSTRUCTED for the stylesheet | No 2015 pixel reference found for these pages. |
| Catalog seed 20150309034826 | 24 items, `preservation/catalog/` | ARCHIVED-NEAR-DATE | Capture is 4.5 months before target. 2 of 4 chunks read. |
| Catalog seed 20150616223239 | 20 items (Collectibles query) | ARCHIVED-NEAR-DATE | 5.5 weeks before target. 3 of 7 chunks read; hand-transcribed. |
| Catalog thumbnails / asset content | none | **MISSING** | Not retrieved; never substituted. |
| Places | 9 manifest entries, 3 bytes-stored in `preservation/store` (git-ignored) | **INFERRED** at best | beagleded archive has no per-file provenance (untested, may be duplicate/broken per its own README). Public status of place IDs is supported by Archive Team's uncopylocked list, which does NOT prove byte identity. |
| Compat endpoint formats | `src/compat/server.js` | INFERRED | Written from memory + static strings in the client; never exercised by the real client. |
| Network protocol (RakNet framing, property ids, value encodings, ID_DATA) | `src/gameserver/*` | INFERRED (`bloxen-profile-v0`) except the facts below | Only the facts in PROTOCOL.md "Real-client evidence" are authoritative. |
| BrickColor hex values | `static/css/brickcolors.css` | INFERRED | From memory, not archived. |

## Acquisition & handling rules applied
* Everything historical is acquired into quarantine, hashed, inventoried, **not executed**. Originals are immutable; working copies are separate (launcher refuses any other path and rewrites only a working copy's `AppSettings.xml`).
* Large/unsafe material stays out of Git (`.gitignore`: `preservation/store|places|client`, `quarantine`). Manifests carry hashes and source instructions; `tools/fetch_places.js` re-acquires places and verifies SHA-256.
* Rejected/quarantined: `natural-disaster-survival` (no evidence of an official release of this file), `roblox-evil-game-idk-stolen` (name and content indicate a leak/stolen copy → REJECTED). Not found and not substituted: *Welcome to the Town of Robloxia*, *Work at a Pizza Place*.
