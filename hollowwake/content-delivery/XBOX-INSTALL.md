# Xbox delivery status

There is no new game-ready package in this source delivery. The native runtime still rejects 208 missing bindings before game startup. Do not treat an earlier companion/preflight download as the completed split-content Fortnite client.

When the integrated client can actually attempt execution, its private release must include the signed x64 Hollowwake.NativeClientExperiment APPX, its matching public CER and the three x64 .NET Native Framework/Runtime/VCLibs dependencies. Keep the working companion installed. Install the private main APPX and Dependencies/x64 files through Xbox Device Portal; never install a certificate as a dependency, and never share a private signing key.

The intended launch action is account linking or session restore, runtime readiness, authenticated content manifest, content size/SHA verification into LocalState/GameClean/6.21, then HWAttemptPackagedClient. Installed code must already be in the signed package. A LocalState boot import cannot replace packaged executable dependencies. The current archive-plus-data transport can require about 40 GB free; it checks the actual storage allowance before download. It does not request a user PC or manual compilation.

Logs remain in the experimental app's LocalState: native-launch.log, native-imports-unresolved.txt, managed-launch.log, content-ready.txt and GameClean/content-verification.txt. Retrieve them through Device Portal's app file explorer. The readiness marker is not proof of a rendered lobby. The launcher exposes named startup/content errors through its paged log UI.

Documented App-to-Game classification: in Dev Home highlight the app, press View (two rectangles), select View details, choose App type: Game, press B and relaunch. This changes resource allocation; necessity for this build and physical Xbox behavior are unverified. It does not implement missing desktop APIs. Public reference: https://walbourn.github.io/directx-and-uwp-on-xbox-one/ .
