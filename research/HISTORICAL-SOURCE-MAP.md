# HISTORICAL SOURCE MAP

| Need | Source used | Grade | Not used / next |
|---|---|---|---|
| Client binaries | KloBraticc/2015-Client @7a0742c (upstream claimed archive.roblonium.com) | ARCHIVED-NEAR-DATE | archive.org `roblox-clients-2006-2021`, archive.roblonium.com direct, Roblox-Versions-Archive (cloned to quarantine, not yet compared) |
| Build dates / versions | RobloxAPI/build-archive; setup.rbxcdn.com/DeployHistory.txt (70 chunks, only chunk 0 = 2009-11→2010 read; July-2015 line NOT yet found) | ARCHIVED-EXACT (build-archive) | find the 2015-07 DeployHistory line |
| API dump | RobloxAPI/build-archive data for version-0d46087630eb46cd | ARCHIVED-EXACT | |
| File-format spec | RobloxAPI/spec (CC-BY-SA-4.0); `formats/rbxlx.md` unread; binary parsed from the format as implemented + tests | — | read rbxlx.md and diff against `rbxlx.js` |
| Website text/structure | Wayback 20150722–25 captures (Home 20150723171630), Games 20150726062742, `/Login` 20150724123053 | ARCHIVED-EXACT/NEAR-DATE | CSS/sprites/JS sub-resources unreachable |
| CSS values | RobloxLabs/web (MIT, 2012–14) | ARCHIVED-NEAR-DATE (values only) | no 2015 pixel reference found |
| Catalog | Wayback catalog/json 20150309034826, 20150616223239 | ARCHIVED-NEAR-DATE | remaining chunks |
| Places | beagleded/Roblox-Places-Archive; Archive Team uncopylocked list; fandom | INFERRED | archive.org collection, other independent copies for byte cross-check |
| RakNet | facebookarchive/RakNet (4.x) frequency table | reference only | client embeds 3.x; identity unverified |
| 2015 client behaviour | Project owner's prior real-client findings (PROTOCOL.md) | authoritative per owner | not re-verified here |

Dead ends: Wayback is reachable only through the `fetch_page` tool, not from the sandbox's Node/curl (ECONNRESET); image search produced no 2015 site screenshots.
