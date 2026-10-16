# Current source and preserved rollback

- `experimental-0.4.5/`: current native compatibility runtime, C#/XAML host, exact import metadata and hosted build/test scripts. The `_initterm` implementation is in `Native/LegacyCrt.h`; its real mapped initializer fixture is `Native/tests/FixtureMemory.cpp`.
- `client-runtime/`: shared original TLS and CoreWindow/DXGI bridge code used by this build.
- `sites/`: exact current Sites website/backend source, Worker embedding/build configuration and D1 migrations. Run npm ci, npm run build and npm test there. Existing production environment secrets and persistent database remain on Sites; no production database, passwords or signing secrets are in this download.
- Earlier experimental and companion directories are preserved rollback/source history. Root `web/` and `backend/` are the preserved original local implementation; use `sites/` for the current deployed website, not the older root UI.

The Sites source commit is recorded in the verification report. The pinned hosted Windows source commit identifies the actual compiled native/XAML code; later documentation/packaging-only commits are listed separately when applicable. No downloaded game code executes in hosted verification.

Live public website/backend: https://hollowwake.jihnjohn10.chatgpt.site . The existing private owner portal is preserved and omitted from public source metadata. A random URL is not authorization. Recovery email is still unavailable pending an authorized mail service.
