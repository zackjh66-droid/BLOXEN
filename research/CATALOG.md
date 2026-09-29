# CATALOG

Source: the site's internal `catalog/json` endpoint as captured by the Wayback Machine (raw `id_` captures, JSON intact). Seeds: `preservation/catalog/catalog-*.json`, loaded by `src/lib/db.js`.

| Capture | Query | Items in seed | Chunks read | Grade |
|---|---|---|---|---|
| 20150309034826 | default catalog | 24 (Hat 15, Gear 4, Package 2, T-Shirt 2, Face 1) | 2 of 4 | ARCHIVED-NEAR-DATE (4.5 months before target) |
| 20150616223239 | Category=2 Subcategory=2 (Collectibles) | 20 (Hat 9, Gear 10, Face 1) | 3 of 7 | ARCHIVED-NEAR-DATE (5.5 weeks before) |

Per item the seed keeps: asset ID, name (verbatim incl. double-encoded entities), creator + ID, type id/name, price R$/Tickets (`null` = off sale), description, created/updated (ms), sales, favorited, Limited-U flag, item URL, source capture + archive URL, provenance. Thumbnail URLs exist in the capture (`t*.rbxcdn.com`) but thumbnails and asset content were **not retrieved → MISSING**, shown as such in the UI and recorded in the `assets` table (`availability='MISSING'`).

**Gaps (not filled, not invented):** no Shirts (type 11), Pants (12), Heads (17), free items or Packages beyond those captured; remaining chunks (20150309034826: 2–3, 20150616223239: 3–6) are unread. Whether July 2015 prices equalled these captures is UNKNOWN. `tools/build_catalog_20150616.js` documents the hand transcription; items cut at chunk boundaries were dropped rather than guessed.

Behaviour: buying deducts R$ from a balance that starts at a BLOXEN-chosen 100,000 (not historical). Limited-U items are "Sold Out" (resale not implemented); off-sale items cannot be bought (bug found and fixed, test added). Equip requires ownership.

BrickColor names/hex values (`src/lib/services.js`) are INFERRED; the default body colour 194 (Medium stone grey) and face are UNKNOWN for 2015 defaults.
