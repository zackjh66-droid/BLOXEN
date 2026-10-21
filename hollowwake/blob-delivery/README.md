# Hollowwake private content delivery

This directory belongs to Hollowwake. Project Zero and the existing Sites
website/authentication are not replaced or modified by this Vercel gateway.

The user authorized use of reported $20 Vercel credit. This phase stages one
12,528,679,454-byte preserved archive, performs one full authenticated readback,
and tests byte ranges. It does not upgrade a plan or raise an account spend
limit. The billing connector reports usage charges, not remaining credit; the
reported $20 balance is user-supplied, not independently confirmed. Blob storage
is metered and grows with uploaded bytes. Private download requests also consume
transfer; do not describe this as unlimited/free hosting. No arbitrary public
proxy or unauthenticated game-download path is provided.

The original user-created `hollowwake-blob` store is public and retained unchanged.
Game bytes use a separate private store scoped to `hollowwake-content`. That
project retains Vercel Deployment Protection. A project-specific automation
credential is used only by the operator and the Sites backend. A second HMAC
credential authenticates the Sites backend's exact-build read leases. Neither
credential belongs in an Xbox package or browser source.

The upload endpoint issues only a fixed-path, fixed-size private multipart upload.
It refuses overwriting a sealed release. Operator credentials arrive through
hidden stdin; no token is written by upload.mjs. The installed Blob SDK is pinned
in package-lock.json. node_modules and local credentials are not source inputs.

Reproduce: npm ci --ignore-scripts, then npm test. Deploy package.json,
package-lock.json, vercel.json, policy.mjs and api/control.js to the exact
Hollowwake content project with BLOB_STORE_ID, UPLOAD_KEY_SHA256 (temporary),
and CONTENT_GATEWAY_KEY (secret). Runtime Blob authorization uses project OIDC;
no long-lived storage token is requested from the user. Use upload.mjs with the
preserved archive and hidden operator configuration. It checks its complete
original SHA-256, uploads, checks anonymous denial, reads the complete stored
object back, compares its SHA-256 and four actual ranges, then marks it ready.
Remove the temporary upload credential after sealing and redeploy. Keep the
permanent gateway and protection credentials server-side in Sites.

A successful mirror does not establish Fortnite startup or a working lobby.
Client integration must request short-lived private leases through linked
Hollowwake authentication; changing a website link does not change an installed
0.5.8 client, whose transport still uses the previous CDN. Matchmaking remains off.
