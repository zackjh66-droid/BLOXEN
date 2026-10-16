# Hollowwake 0.4.5 verification — 2026-10-16

## Completed

- Hosted Windows2022 source build, development-signed x64 APPX, signature verification, installation and native UWP activation passed. Exact compiled source: 8b4b2d4156b5cce51aab9982c02b187ee7eaef89 . Workflow: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38866206680 . Standard public Windows runner; no paid resource or testing VM provisioned.
- Real production dependency mapper executed original owned DLL fixtures importing msvcrt!_initterm/_initterm_e. Verified null skipping, callback order, empty tables, success and error73 stopping later callbacks. Allocation, clock/wrap, forwarders and negative non-relocatable executable checks passed. These were real calls, not expected-value placeholder exports.
- Signed UWP package passed existing memory, window/message, TLS-slot, allocator, clock and DXGI presentation contracts; named unsupported-call termination, post-crash startup recovery and persisted logs passed. This is not Fortnite rendering.
- Actual UWP UI: displayed device code, approved own test account via live backend, polled and displayed username; protected session restored after process restart; logout revoked session; web revocation returned native UI to linking. Controller-style orange focus is visible in the Windows screenshot via UI Automation. No physical gamepad was attached.
- Static APPX block-map:145 blocks verified. Three native modules checked for x64/AppContainer and imports against supplied VCLibs; no missing VCLibs exports. Windows signature verification log included. Public CER only; private signing key non-exportable on ephemeral runner.
-17 server tests passed: maintained authentication, device code expiration/reuse/polling/typo handling, durable state, profile isolation, shared admin authority, catalog/locker revisions, suspension, owner authorization and prototype adapter rules. These prototype tests do not establish Fortnite login.
- Live Sites version10 deployed successfully at https://hollowwake.jihnjohn10.chatgpt.site . Source e0a278f01c62b89fc127451ad19d316ed22dd822 . Same DB binding, unchanged environment revision2, no destructive migration or account reset. Version9 remains rollback.
-14 live browser checks passed: registration, cookie login, explicit approval, cross-account rejection, code reuse, saved username/profile, token renewal, device revocation, browser logout/login, honest unavailable recovery, invalid-code errors, connectivity loss,25 code typos followed by successful approval/redemption, nine mobile routes without overflow. No JavaScript page errors.
- Six main routes checked at1440x1000 and390x844: no horizontal overflow. Reduced-motion mode disables entrance animations. Authenticated settings and device operations also exercised in the live browser suite.
- Official public Epic Season6 cube/corruption background verified against the September27,2018 announcement and its CDN; source and conditional fan-content terms recorded. Original transparent SVG branding and PNG alpha corners checked. Licensed Anton font included.
- Public package/source/dependency/certificate/report URLs fetched and matched release sizes/SHA256. Delivery ZIP scan found no private-key markers, configured credential values, private databases or PAK archives. This is a scoped delivery check, not a full security audit.
- Anonymous public control API and private owner site both returned401; public status returned200. The previously exposed Sites credential had already been rotated and old-credential rejection verified in preserved remediation evidence. No secret is included here.

## Limits and failures

The exact archive dbghelp was statically inspected only. Its reported return RVA703977 follows a call to _initterm with a17-entry half-open initializer table. No downloaded Fortnite executable or DLL was run in hosted CI. Only the user has tested earlier builds on physical Xbox.

User-reported0.4.3 Xbox evidence: real dbghelp DLL_PROCESS_ATTACH reached msvcrt!_initterm after earlier GetTickCount and allocator fixes. 0.4.5 implements that missing operation; it has NOT been run on Xbox. Other missing APIs may stop the next attempt. Unsupported calls intentionally flush logs and terminate rather than return fake success.

Original Fortnite main entry, original loading screen/lobby, character rendering, game controller menus, locker and client-backend authentication remain unverified/unfinished. Native HTTP/account handoff and complete Windows ABI/thread/exception/graphics behavior are incomplete. The version-specific adapter remains disabled in production. Full PAK import cannot fix these runtime gaps. Matchmaking is unavailable.

Native screenshots are actual hosted Windows UWP captures at800x600; maximizing did not increase the captured viewport on that runner. The menu is scrollable at that size and lower buttons were exercised by UI Automation. These are not Xbox screenshots, not full television-layout verification and not a mock/recreated Fortnite lobby. Website screenshots are actual deployed Chromium pages. No design-concept image is presented as implementation evidence.

Recovery email remains blocked by lack of an authorized configured mail service. No email delivery is claimed. The existing owner-private admin was preserved; its UI was not redesigned in this iteration.

An initial anonymous-control probe expected403 but observed401; the assertion was corrected to accept either authentication denial, and no anonymous access was granted. A Sites source push with an expired short-lived write credential was rejected403; a newly minted repo-scoped credential completed the push without printing or persisting its secret.
