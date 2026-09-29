# SCRIPTING — inventory, runtime design, what runs and what does not

Scope rule from the task: inventory, hash and classify scripts first; design the runtime separately; **do not claim a game is playable until its logic actually works.**
Nothing in this document is REAL-CLIENT-TESTED. Labels used: UNIT-TESTED, SIMULATOR-TESTED.

## 1. Inventory (done before any execution)

`tools/script_inventory.js` is a static, non-executing lexical scan over every stored place. It writes `research/script-inventory.json` (path, class, byte size, SHA-256, API probe hits; **no source text**). Headline numbers for the 5 stored places:

| place | scripts | notes |
|---|---|---|
| tabularasa | 0 | |
| crossroads-2007-client | 25 Script | regeneration ×5, LeaderboardV3, trampoline bouncers ×14, TeamBeacon ×4, BattleArmor, AndYetItMoves |
| crossroads-uncopylocked-commit | 48 (40 Script + 8 LocalScript) | 5 flagged, all by `Teleport` mentions |
| happyhomeinrobloxia | 0 | |
| robloxhq | 2 Script | |
| roblox-world-headquarters | 22 (16 Script + 6 LocalScript), 3.4 KB | Jet Boots HopperBin, 14 `BreakJoints` calls (no-op); 0 flagged |
| mission-to-the-moon | 3 Script, 3 KB | Moon low-gravity (`BodyForce`), NoHelmet, Cannon; 0 flagged |

API tokens across all stored scripts (how many scripts mention each): `wait()` 55, `script.Parent` 53, FindFirstChild family 45, `connect` 43, Humanoid 42, Touched 39, Vector3/CFrame.new 38, Instance.new 24, Clone 14, Remove/Destroy 13, Died/Health 13, `math.*` 12, `workspace` 11, GUI 10, coroutine 10, BodyMovers 10, Teams 8, Players 5, Teleport 5, CharacterAdded 4, Changed/ChildAdded 3, leaderstats 3, GetService 2, BrickColor 2, Tool 1, `table.*` 1.
**None** use `loadstring`, `require`, Http, InsertService, DataStore, Marketplace, `os` or `io`. That is a property of these files, not a guarantee about other places, so the sandbox denies all of them regardless.

## 2. Runtime design

```
Place file ──(importer, inert)──► World.scriptSources  (source text held as data, never run)
                                        │   only if ScriptHost is constructed AND scripts:true / BLOXEN_SCRIPTS=1
                                        ▼
                          ScriptHost (src/script/engine.js)
                            ├─ Interp  (src/script/lua.js)  sandboxed Lua 5.1 interpreter, step budget per resume slice
                            ├─ Scheduler                      cooperative threads, virtual clock, wait()/spawn()/delay()
                            ├─ API surface                    Instance wrappers → World (the only thing scripts can touch)
                            └─ report()                       per-script status + every unsupported member hit
```

* **Disabled by default.** `GameServer({scripts:true})` or `BLOXEN_SCRIPTS=1`. Without it the behaviour is exactly the earlier GEOMETRY-ONLY server (a test asserts nothing is created).
* **Interpreter** (`lua.js`, ~650 lines, zero deps): full Lua 5.1 syntax (lexer, parser, closures, varargs, metatables, multiple returns, long strings), `string` incl. Lua patterns, `table`, `math` (deterministic seedable PRNG), `coroutine` (wait() inside a coroutine suspends only that coroutine, as in Roblox), `pcall`/`error` with `chunk:line:` messages. Implemented as generators so `wait()` can suspend without threads.
* **Sandbox.** No `io`, `os.execute/remove/getenv/...`, `require`, `loadstring`, `load`, `dofile`, `getfenv/setfenv`, `debug`, `package`, `collectgarbage`. Tests assert every one of these is absent. Each resume slice has an instruction budget (default 2,000,000 steps); `BudgetError` is not catchable by `pcall`, so a runaway `while true do end` stops that script only. `string.rep` results are size-capped. Blocked services (`HttpService`, `DataStoreService`, `MarketplaceService`, `InsertService`, `TeleportService`, `BadgeService`, …) raise an error and are counted in the report. Scripts cannot create `Script`/`LocalScript` instances.
* **Per-script environments.** Each script writes globals into its own table and falls back to the shared sandbox globals (Roblox behaves the same); `_G` and `shared` are the only shared user tables. This matters: the five Crossroads regeneration scripts all assign globals named `model`, `backup`, `message`.
* **Engine boundary.** Scripts only see `LuaInstance` wrappers around `World` instances; every property write is type-checked against the API schema and goes through `world.setProp`, so it replicates to clients through the normal change feed. Re-parenting into the live tree allocates **fresh instance ids** and announces every replicable instance (`world.attach`); removing emits `del`; clones stay detached (and invisible to clients) until parented.
* **Server-local members.** `NotReplicated` members in the API dump (for example `Humanoid.Health`, `Jump`) are readable/writable by scripts but never sent on the wire; `Humanoid.Health = 0` kills the player through the normal respawn path.
* **Events.** `Changed`, `ChildAdded`, `ChildRemoved`, `Humanoid.Died/HealthChanged`, `Players.PlayerAdded/PlayerRemoving`, `Player.CharacterAdded` fire from the world change feed; handlers run as new scheduler threads (errors are contained per handler). Scripts run to their first `wait()` before the game server accepts clients.
* **Legacy aliases** of the 2007 scripts are accepted: `:clone() :remove() :findFirstChild() :children() game:service()` and lowerCamel property names.

## 3. What runs, what does not (600 virtual s, `research/script-runtime-report.json`)

Contact model (added after the first pass, UNIT + SIMULATOR-TESTED): `Touched`/`TouchEnded` now fire **only** when a player's character parts overlap a listening part (oriented-box separating-axis test, 0.05-stud tolerance, sampled once per server step; one `Touched` per contact per part, `TouchEnded` on leaving). There is still no physics engine: parts touching other parts, falling or thrown parts, vehicles and anything driven by `Velocity`/BodyMovers never fire `Touched`. The report states this limit in its `unsupported` list.

Crossroads 2007 file (25 scripts), main run with no players:

| group | count | result |
|---|---|---|
| Regenerate Castle / Tower / Hideout / Lost Temple / Ramp and Trees | 5 | **run**: model `:clone()`d, `:remove()`d on a timer, re-parented (fresh ids), Message shown/hidden. `:makeJoints()` is a **no-op** (joints are not simulated). |
| AndYetItMoves | 1 | **runs**: Lighting `TimeOfDay` advances one minute per second. |
| LeaderboardV3 | 1 | **runs**: creates `leaderstats` (KOs, Wipeouts) under each Player, hooks `Humanoid.Died`/respawn. Whether the real client's PlayerList shows it is UNKNOWN. |
| PL3Trampoline bouncers | 14 | connect to `Touched`; **fire when a character overlaps them** and set `Torso.Velocity = (0,200,0)`. `Velocity` is replicated as a property, but nothing simulates motion on the server, and whether the real client applies a server-set `Velocity` to its own character is UNKNOWN. So: **launch effect UNVERIFIED**. |
| TeamBeacon ×4 | 4 | **now load and run** (they were 4 errors: lowercase `BodyPosition.position` is a NotReplicated server-local Vector3, now supported; `Humanoid.Torso/LeftLeg/RightLeg` and `BasePart.Color` legacy members added). Team joining/heal/harm logic runs on contact; the beacon's own rise (`BodyVelocity`) is not simulated. |
| BattleArmorScript | 1 | connects `Touched`; fires on contact. |

**Contact probe** (`contactProbe` in the report: a player is dropped on each Touched-listening part in turn; coarse, the player dies on some parts): Crossroads 2007: 42 listeners, 15 contacted, no script errors. Crossroads uncopylocked commit: 48 listeners, 21 contacted; teleporter scripts print `found hit` / `Found humanoid` / `Cloning Script` and then fail: `TeleportScript is not a valid member of Script` and `TouchScript is not a valid member of Model`. Cause: **scripts are not instances in the world tree**, so a script cannot find a sibling/child Script (`script.TeleportScript`, `Model.TouchScript`) and cannot `:clone()` it. Teleporting also needs `TeleportService`, which a local server does not have. Those places stay non-playable.

World Headquarters (120 virtual s): 10 finished, 6 LocalScripts not run, 6 disabled, 0 errors. The Jet Boots script loads (`Selected` is a player-input event that connects but never fires, recorded as unsupported). Mission to the Moon: the Moon script runs (low gravity is a `BodyForce`, not simulated); the Cannon script fails on its **own bug**: `Vector3.new(math.random(), math.random(), math.random)` passes the function `math.random` for the third argument, which real Roblox also rejects (`tests/script-engine.test.js` pins this). Both stay GEOMETRY-ONLY.

An internal JavaScript error inside a script thread no longer crashes the host: the script is stopped and the error is recorded.

**Verdict: no place is playable.** Regeneration, day/night, leaderboards and now contact-triggered logic run; movement effects (Velocity, BodyMovers, joints), tool input, `TeleportService` and script-to-script lookups do not. The honest label stays **GEOMETRY-ONLY + PARTIAL SCRIPT LOGIC**.

## 4. What is needed next (in order)

1. ~~Server-side overlap for `Touched`~~ DONE for player characters (see §3). Still open: part-vs-part contact (needs physics).
1b. **Scripts as tree members** (non-replicated), so `script.Child`, `Model.SomeScript` and `:clone()` of scripts work (blocks the Crossroads teleporters).
2. BodyMovers/Velocity (10 scripts) require real physics integration; assess after 1.
3. ~~`Color` and `Humanoid.Torso`-style legacy members~~ DONE (BrickColor mapping is INFERRED: hex table).
4. Tools (`Activated`, `Equipped`) and `Backpack` semantics for StarterPack gear.
5. Message/Hint GUI behaviour on the real client (does a server-created `Message` render?). UNKNOWN until the Windows gate.
6. Fidelity check against real Lua 5.1 (the test expectations were derived by hand, not by running reference Lua). Known deviations: `%g` formatting, `os.date`, `string.format('%q')` details, `tostring` of table addresses, `table.sort` stability (merge sort), error message wording for upvalues.

## 5. Tests

`tests/lua.test.js` (19: language semantics, stdlib, patterns, coroutines, scheduler, sandbox, budget), `tests/script-engine.test.js` (13: engine API, regeneration, players/leaderstats/Died/respawn, sandbox refusals, per-script env, the real Crossroads file when present), `tests/script-e2e.test.js` (2: script-created instances reach the simulator client over the wire; default-off proven).
