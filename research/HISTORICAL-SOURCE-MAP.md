# HISTORICAL SOURCE MAP

| Need | Source used | Grade | Not used / next |
|---|---|---|---|
| Client binaries | KloBraticc/2015-Client @7a0742c (upstream claimed archive.roblonium.com) | ARCHIVED-NEAR-DATE | **CedarCoder1/Roblox-Versions-Archive checked: its CSVs only list 2025–26 versions (681–722) — no 2015 coverage, useless for cross-reference.** archive.org `roblox-clients-2006-2021` (metadata read: 7z bundles with md5/sha1 but its "RobloxPlayer.7z" and "Studios 2015–2016.7z" are marked *not added yet*; `ROBLOX CLIENT COLLECTION_v1.7z` 3.5 GB / 107,028 files is unexamined and is the remaining candidate for an independent hash; it also has `Places.7z` (517 files, 79 MB) — a further possible places source, unexamined). archive.roblonium.com not checked directly |
| Build dates / versions | RobloxAPI/build-archive; setup.rbxcdn.com/DeployHistory.txt (70 chunks; chunk 12 = Apr–Aug 2015 holds the exact line for 0.205.0.61876, see CLIENT.md) | PRESERVED (DeployHistory), ARCHIVED-EXACT (build-archive) | |
| API dump | RobloxAPI/build-archive data for version-0d46087630eb46cd | ARCHIVED-EXACT | |
| File-format spec | RobloxAPI/spec (CC-BY-SA-4.0); `formats/rbxlx.md` READ in full and used for `tests/rbxlx-spec.test.js` (Color3 packed/element forms, Content url/null/binary/hash, Ref, numeric/vector types, ProtectedString, entity-bomb/deep-nesting resilience); one deviation found and fixed (legacy Content binary/hash now read as empty); binary parsed from the format as implemented + tests | — | read rbxlx.md and diff against `rbxlx.js` |
| Website text/structure | Wayback 20150722–25 captures (Home 20150723171630), Games 20150726062742, `/Login` 20150724123053 | ARCHIVED-EXACT/NEAR-DATE | CSS/sprites/JS sub-resources unreachable |
| CSS values | RobloxLabs/web (MIT, 2012–14) | ARCHIVED-NEAR-DATE (values only) | no 2015 pixel reference found |
| Catalog | Wayback catalog/json 20150309034826, 20150616223239 | ARCHIVED-NEAR-DATE | remaining chunks |
| Places | beagleded/Roblox-Places-Archive; Archive Team uncopylocked list; fandom | INFERRED | archive.org collection, other independent copies for byte cross-check |
| RakNet | facebookarchive/RakNet (4.x) frequency table | reference only | client embeds 3.x; identity unverified |
| 2015 client behaviour | Project owner's prior real-client findings (PROTOCOL.md) | authoritative per owner | not re-verified here |

Dead ends: Wayback is reachable only through the `fetch_page` tool, not from the sandbox's Node/curl (ECONNRESET); image search produced no 2015 site screenshots.
