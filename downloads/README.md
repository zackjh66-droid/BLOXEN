Hollowwake 0.2.0 is an unofficial, independently authored UWP account companion. **The actual Fortnite Season 6 client/lobby is not included; the requested Fortnite port remains unfinished.** No Epic game files are redistributed here.

Live website and persistent Worker/D1 account backend: https://hollowwake.jihnjohn10.chatgpt.site

The x64 APPX was built and development-signed on a standard free hosted Windows 2022 runner. Its matching public certificate and three x64 framework dependencies are included in the package ZIP. No private signing key is exported. Microsoft's SignTool verified the package. Hosted Windows installed and activated it; native UI tests passed linking, username display, protected sign-in restoration after process restart, sign-out and heartbeat detection of website revocation. **Xbox hardware compatibility remains unverified.**

Download `Hollowwake-Companion-0.2.0-x64.zip`, extract it, and use Xbox Device Portal's Apps/Add flow to upload the APPX plus all three files under Dependencies/x64. Use the matching CER if the portal offers a certificate field. Follow INSTALL.md. No SDK or compilation is required on your computer.

`Hollowwake-Updated-Source.zip` contains the companion, retained Node backend, deployed Sites Worker/D1 source, migrations, environment examples, hosted-build scripts and documentation. Public artwork is original Hollowwake art; original private game inputs are preserved separately. `Hollowwake-Verification.zip` contains logs, reports and mobile/desktop screenshots. ARTIFACTS.json records ZIP hashes; SHA256.json inside the package ZIP records individual file hashes.

Live browser testing passed 14 scenarios with no JavaScript page errors. Worker/D1 and Node authentication tests passed. A deployed device code expired after its real ten-minute lifetime. Both dependency audits report zero known vulnerabilities.

Recovery email/verification delivery remains unavailable until an authorized mail sender/service is connected. Registration currently uses unverified email addresses and project passwords. The original Fortnite runtime, original loading/lobby rendering, locker/cosmetics, client-compatible Epic protocol adapter and physical Xbox/controller tests remain outstanding. See DELIVERY-REPORT.md in the source/package for precise blockers and tested limits.
