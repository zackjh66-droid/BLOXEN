# CATALOG

Source: the site's internal `catalog/json` endpoint as captured by the Wayback Machine (raw `id_` captures, JSON intact). Seeds: `preservation/catalog/catalog-*.json`, loaded by `src/lib/db.js`.

| Capture | Query | Items in seed | Chunks read | Grade |
|---|---|---|---|---|
| 20150309034826 | default catalog | 42 (Hat 27, Gear 7, Package 5, T-Shirt 2, Face 1) | 4 of 4 | ARCHIVED-NEAR-DATE (4.5 months before target) |
| 20150616223239 | Category=2 Subcategory=2 (Collectibles) | 42 (Hat 25, Gear 16, Face 1) | 7 of 7 | ARCHIVED-NEAR-DATE (5.5 weeks before) |

Per item the seed keeps: asset ID, name (verbatim incl. double-encoded entities), creator + ID, type id/name, price R$/Tickets (`null` = off sale), description, created/updated (ms), sales, favorited, Limited-U flag, item URL, source capture + archive URL, provenance. Thumbnail URLs exist in the capture (`t*.rbxcdn.com`) but thumbnails and asset content were **not retrieved → MISSING**, shown as such in the UI and recorded in the `assets` table (`availability='MISSING'`).

**Gaps (not filled, not invented):** no Shirts (type 11), Pants (12), Heads (17), free items or Packages beyond those captured; both captures have now been read completely (4 of 4 and 7 of 7 chunks), but each covers only the first page of its query, and a few records cut by chunk edges could not be recovered (see each file's `completeness`). Whether July 2015 prices equalled these captures is UNKNOWN. `tools/build_catalog_20150616.js` documents the hand transcription; items cut at chunk boundaries were re-joined only when both halves were read, otherwise dropped rather than guessed. The 2015-06 items added after the original builder script (chunks 3–6) and the 18 later March items were added by direct transcription into the JSON files. Blackvalk (124730194) is in both captures: 1,000,000 Tix in March, Limited/off sale in June; the later capture supersedes it in the seed (83 distinct items).

Behaviour: buying deducts R$ from a balance that starts at a BLOXEN-chosen 100,000 (not historical). Limited-U items are "Sold Out", Limited (resale-only, `PriceView 1`/`IsLimited`) items and Builders Club items (`MinimumMembershipLevel 1`, Midnight Shades) are refused with the archive-based reason (resale and membership are not implemented; `tests/catalog.test.js`); off-sale items cannot be bought (bug found and fixed, test added). Equip requires ownership.

BrickColor names/hex values (`src/lib/services.js`) are INFERRED; the default body colour 194 (Medium stone grey) and face are UNKNOWN for 2015 defaults.
