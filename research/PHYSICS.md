# Physics and `Touched`

> **REAL PHYSICS-ACCURATE TOUCHED = BLOCKED / UNIMPLEMENTED.**
> BLOXEN has **no physics engine**. What exists is a clearly-labelled **simplified contact model** (UNIT-TESTED, SIMULATOR-TESTED, not REAL-CLIENT-TESTED, not claimed to match Roblox).

## What the game server does (implemented)

| Behaviour | Status | Where |
|---|---|---|
| Player movement: the owning client reports a CFrame; the server validates finite numbers, world bounds and a speed budget (WalkSpeed 16 x1.5 slack), then applies it and replicates it | SIMULATOR-TESTED (INFERRED message ids) | `World.applyMove` |
| Falling below `fallenY` kills the character; respawn timer; `Humanoid.Died` | UNIT-TESTED | `World.kill` |
| **Simplified contact model** for scripts: a part with a live `Touched`/`TouchEnded` connection fires when a player's character parts overlap it (oriented-box separating-axis test over the 15 axes, contact tolerance 0.05 stud), sampled once per script step; `TouchEnded` when the overlap stops | UNIT-TESTED (`tests/script-engine.test.js`), SIMULATOR-TESTED | `src/script/engine.js` (`obbOverlap`, `boxOf`) |
| Every use is recorded in the compatibility report (`touchedConnections`, `unsupported` entry "event Touched: fires only when a player's character overlaps the part…") | UNIT-TESTED | `host.report()` |

## What does not exist (and therefore is not claimed)

- Rigid-body simulation, gravity on unanchored parts, collisions between parts, joints/welds motion, `Velocity`/BodyMovers integration. The 14 Crossroads trampoline scripts do fire on contact and set `Torso.Velocity = (0, 200, 0)`; that property is replicated but **nothing simulates the launch**. Whether the real client would then move the character is UNVERIFIED.
- Part-vs-part `Touched` (a part falling on a part), `CanCollide`-aware contact, `Touched` for non-player models, `GetTouchingParts`/`Region3` queries.
- Sleeping/ownership rules (which peer simulates an unanchored part) - INFERRED/UNKNOWN for 0.205.0.61876.
- Any claim that BLOXEN's `Touched` ordering, rate or tolerance equals Roblox's. The 0.05-stud tolerance and step sampling are BLOXEN choices.

## Why "BLOCKED" rather than "NOT STARTED"

Faithful `Touched` needs (a) the real client's character-physics behaviour (the client is authoritative for its own character's motion in the target era - INFERRED, not verified) and (b) captured real-client traffic showing what state it sends and expects. Both need the real client running (Windows required) or new historical evidence. Writing a physics engine without either would be guessing. The simplified model is the most BLOXEN can honestly offer without them, and it is labelled everywhere it surfaces (Game Details compatibility blockers, script report).

## Tests

`tests/script-engine.test.js` (no firing without a player; fires once per contact and `TouchEnded` for player characters, not distant parts or non-players; rotation-aware box test; real Crossroads scripts: Touched/BodyMover scripts are reported, not faked), `tests/script-e2e.test.js` (scripts over the wire; Touched itself is NOT tested through a simulated client), `tools/script_report.js` (`contactProbe`: drops a player on each Touched-listening part in every preserved place).
