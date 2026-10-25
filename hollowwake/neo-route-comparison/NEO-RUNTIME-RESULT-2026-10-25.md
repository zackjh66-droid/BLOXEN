# Neo reference: actual Windows installation and activation

Exact source: `https://content-cdn.neofn.dev/NeoXS_1.0.6.0_x64.msix`.

The real supplied package was installed and activated on a disposable Windows Server 2022 runner in GitHub Actions run [40203106130](https://github.com/zackjh66-droid/BLOXEN/actions/runs/40203106130). This was a reference-app execution, not a Hollowwake or Xbox game test. It did not reach Fortnite's loading screen or lobby.

## Observed screen and blocking operation

The actual running window displayed **Neo**, **Scan to sign in**, a QR code, and **or go to neofn.dev/activate**, with a temporary device-link code. That screen was captured ten times; captures remain recipient-encrypted, and the device-link code is not published. UIAutomation did not expose these custom-rendered labels, so its automatic `loginGateObserved=false` field does not describe the visual result. Human visual review of the decrypted capture establishes that the sign-in screen was present.

No installer action was exposed to the observer, and no game-content transfer was observed over 24 samples, approximately four minutes. The app remained running until the observer stopped it. The operation blocked at Neo's device/account-linking screen. The minimum missing requirement for continuing the requested download through Neo is an authorized linked Neo session, established by its normal activation flow. No account password, third-party session token, or Hollowwake credential was supplied. This result does not establish that Neo lacks downloadable content or that it fails on Xbox.

## Actual installed files

- Package SHA-256: `9d7ead048380ffd73d15afe1c53bd03c54a4ba186a93856bff652937c2c03ef8`; 210,415,579 bytes.
- Registered package: `Microsoft.MicrosoftEdge.Neo_1.0.6.0_x64__65q16g0tnjser`.
- Package signer: `CN=Ender`. Signature verified; the Microsoft-prefixed package name is not proof of Microsoft authorship.
- Installed `NeoXS/Binaries/Win64/FortniteClient-Win64-Shipping.dll`: 123,868,672 bytes, SHA-256 `888cf95913857f6b75b6f7ee570a2a6e59c1b71c83db3b5da40f47a8c3b2223d`. This exactly matches the bundled DLL. Previously inspected version marker is 10.40-CL-9380822, not Hollowwake 6.21.
- Bundled PAK/SIG files: zero.
- Observed PAK/SIG files outside the package: zero. Observed additional EXE/DLL files outside package: zero. This inventory covers the package, its LocalState, and `D:\DevelopmentFiles\Neo`, not every possible directory on the machine.
- Evidence ZIP SHA-256: `772e9198eda9f9630574b5a3739fb72919de4e78e4b558d157b2e772e02f6810`; artifact 12603076629, 298,803 bytes. It contains bounded metadata and encrypted screenshots, not game files.

## Relevant architectural difference

Read-only examination of the supplied host confirms that its launch method gives `CreateThread` a callback at RVA `0x9f20`, passing its game export as the thread parameter. The callback calls that export. Hollowwake's current route instead passes the original game entry directly as the OS thread start address with the measured original PEB argument. Neo's worker callback is a concrete difference suitable for a controlled Hollowwake experiment; its suitability for the different Season 6 entry and its effect on the current original-game crash remain untested. No Neo proprietary implementation was transplanted into Hollowwake.

The signed host plus bundled client DLL plus separately installed content architecture is supported by the package inventory and the user's Xbox screenshots. This run did **not** obtain the separately downloaded content, so no claims are made about its changed DLLs, custom PAK contents, authentication behavior after linking, or a successful Fortnite launch.

The existing Hollowwake companion, website, account services, download releases, signing material and rollback were unchanged. No new game package was published.
