# Hollowwake split delivery integration

Scope: existing Hollowwake, branch hollowwake/native-system-route in zackjh66-droid/BLOXEN. No Dealbot/Project Zero files or repositories are changed or copied.

The two-part route is adopted as a continuation of Hollowwake's native packaging route, not a replacement runtime. The signed installed package contains prepared client code, local dependencies and original Hollowwake import bridges. Large non-code data remains in LocalState/GameClean/6.21 with original relative paths. LocalState is not an executable dependency source.

## Proven and implemented

- Prior hosted run 38965324618 completed BOTH the original-code public build checks and a private development-signed game-containing APPX build. It validated every prepared image hash and all 11 proxy DLLs inside that signed APPX. Proprietary output was deliberately not publicly uploaded; that ephemeral runner is no longer a download host. This is packaging evidence, not game execution or a successful port.
- private-package/content_manifest.py reads and hashes the preserved original ZIP only. A private plan was generated for 139 data files, 26,449,827,840 expanded bytes. No code/scripts/anti-cheat launchers/configuration/CMS caches are selected. No content is deleted from the original archive. Only file paths, lengths and SHA-256 hashes are generated; no distribution permission is inferred.
- GET /api/device/content/6.21 in the Node backend requires the existing live linked-device bearer session, rejects browser sessions, checks moderation/revocation through the same session guard and returns no-store metadata. HOLLOWWAKE_CONTENT_MANIFEST_JSON supplies owner-reviewed server configuration; unset or invalid configuration returns 503. No database migration or account reset is needed.
- The native application's actual Launch_Click validates the account and current native import preflight, fetches that authenticated content plan, downloads/reuses the pinned archive without sending any account credential to the archive host, verifies its size/SHA, checks each selected data member, retains changed prior copies and writes the ready marker only after success. A complete pinned archive stays unchanged. An interrupted unverified full transfer restarts on retry; verified extracted files are reused. The conservative free-space check occurs before download and includes missing content plus the archive and safety margin.
- Transfer rechecks device authorization at most every 30 seconds, renews the short-lived manifest only when its ID remains identical, supports cancellation, and revalidates session immediately before native startup. Malformed paths, links, duplicate paths, executable extensions, over-expansion, wrong lengths and hash mismatches fail closed.
- The file bridge maps those original logical paths to the downloaded content. Guest writes to the verified content/CEF data trees are denied, while own Saved data remains writable. Installed executable aliases still map only to the signed package.
- No additional diagnostic-only package is released. Hosted workflow uploads evidence only; game-containing packages remain private.

## Deployment integration

Current live website remains https://hollowwake.jihnjohn10.chatgpt.site . Its working accounts/admin/backend remain unchanged. The content endpoint is implemented and tested in the Node backend source; it is not claimed deployed.

For the existing Sites Worker, copy backend/content621.mjs and the small authenticated endpoint insertion into its existing backend/core.mjs, and set config.contentManifest from server-side env.HOLLOWWAKE_CONTENT_MANIFEST_JSON in worker/index.mjs. Keep every existing auth option, D1 binding, owner check and runtime secret. Do not put that variable into public browser assets. Never upload game bytes to a public Site. Only configure metadata and any hosting route after applicable distribution permission is established.

Sites' required source helper scripts/site-workflow.mjs is absent in this environment and not readable at either tested skill-resource location. No unsupported replacement deployment or private credential disclosure was attempted. A supported Sites source checkout/publishing helper is needed to deploy the backend change safely.

## Exact launch blockers

The current native preflight still returns HRESULT 0x8007007F before LoadPackagedLibrary: 208 missing bindings among 843 redirected DLL/symbol pairs, including USER32!GetRawInputData, RegisterRawInputDevices and TranslateMessage. There are 838 further symbol pairs in 62 unredirected OS libraries whose Xbox availability is unverified. Native startup is not marked ready by a download or successful packaging.

The converted EXE's conventional CRT command line (_get_narrow_winmain_command_line), full device/context/swapchain identity/presentation, remaining window/input semantics and a verified libcurl/MCP endpoint plus short-lived Fortnite grant handoff remain unfinished. Existing /client621 profile/catalog prototype and Hollowwake website login are not evidence of Fortnite login. Original loading, character rendering and lobby have not executed. Matchmaking stays unavailable. Xbox execution remains unverified.

These are remaining implementation requirements, not a request for a user PC or manual compilation. Source and build logs are delivered; no game-ready download is claimed.
