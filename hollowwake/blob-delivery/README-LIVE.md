# Live scope and verification

Private content gateway project: hollowwake-content (Hollowwake only).
Private Blob store: hollowwake-client-private, store_rIcsWVi50gaGbCuh.
Original public hollowwake-blob and existing hollowwake Vercel project unchanged.
Sites account website remains https://hollowwake.jihnjohn10.chatgpt.site.

Sites /api/device/content-lease validates the existing linked device session,
moderation/revocation and private-download owner. Only then does it call the
protected Vercel gateway using server-only HMAC and protection credentials.
The reply is constrained to the exact build, bytes, SHA-256, private host and
path. Native 0.5.9 separately validates these pins and renews before each range
when necessary. No account bearer or storage-write credential goes to Blob.

The SDK is pinned to 2.8.2. Its no-callback presignUrl output is losslessly
converted into its multipart client payload. The automatic stream uploader
was replaced with serial, bounded 32 MiB manual SDK parts after a hosted upload
terminated before completion. The preserved original archive is rehashed before
every attempt. An incomplete upload is never marked ready. A complete stored
readback and exact ranges must pass before verified.json is created.

Use mirror-verification.json, when present, as actual full upload/readback
proof. A fixture pass, issued URL, head response or transferred prefix alone is
not that proof. No code here executes the game. The unresolved Xbox 0.5.7
0xC0000005 needs a current module/RVA report; no loading/lobby outcome is claimed.

Reported $20 credit is user-supplied. We did not raise a spend limit, change
plan, or confirm the remaining credit. Storage and download traffic are metered;
access stays private and scoped to the authorized test owner.
