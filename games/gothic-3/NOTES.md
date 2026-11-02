# Gothic 3 preparation notes (2026-10-16 and 2026-10-17)

## Observed locally

- Co-op Anything revision: `abd32804f9bfc137162cce98a58fedb0287e1c32`.
- SDK revision: `90bfd344de4510dda7ac9da7461cc7f1eac911f7`.
- Local Steam installation found; executable metadata identifies CP 1.75.14 and x86. Private identity evidence is in `local/identity.json`.
- Parallel Universe files are present. The installation must not be described as clean vanilla.
- Upstream `detect_engine` returns no match for this installation. It has no Genome signature; this does not contradict the SDK's identification of Gothic 3 as Genome.
  2026-10-21: this branch adds a Genome signature (`SharedBase.dll` plus `Engine.dll`, evidence also lists
  `Game.dll`, `Script.dll`, `Projects_compiled.pak`) and a Gothic 3 SDK hooking hint. Checked: both isolated
  test copies now return `Genome (Piranha Bytes)` with the four Gothic 3 projects as same-engine projects, also
  through the stdio server; the repo and mod source folders and a synthetic Source layout (`engine.dll` without
  `SharedBase.dll`) do not. Not checked: other Genome games (Risen, ELEX), whose file layout is unknown here.
- Isolated installation copy created. Initial preparation had no save isolation; the continuation below supersedes that status.

## Searched

- Local directory, write-ups and engine-family playbook for Gothic/Genome.
- Public GitHub open issues and PRs, unauthenticated REST, first 100 of each: one result each, no Gothic/Genome matches on 2026-10-16.
- Web queries: Gothic 3 multiplayer coop mod Genome g3sdk; Gothic 3 SDK multiplayer; Gothic 3 Heroes Pigeonade GitHub multiplayer.
- Public GitHub repository search for Gothic 3 multiplayer and exact Gothic 3 Heroes: no candidate source repository returned.
- Nexus author page and Gothic Up hands-on report; SDK primary repository README and build configuration.
- No Discord account access, login, archive search or exhaustive fork/branch audit was performed. Research is preliminary, not evidence that no other work exists.

## Inference

Existing-project evaluation is the next step. A fresh networking implementation is premature until Heroes and the old mod have been assessed.

## Unknown

Current Heroes build/hash, source and license; full patch-stack compatibility and two-client gameplay. Startup tests in the continuation below supersede the initial no-runtime status.

## Continuation: artifact triage and profile isolation

- Git fast-forward pull: already up to date. Public open issue/PR checks again returned one each and no Gothic/Genome matches. Native coop-anything MCP tools are available and were used for triage and identity.
- User confirms that neither Heroes nor G3MP archives are locally available. Nexus download needs login; Heroes release is linked through Discord. No accounts or private channels were accessed. Exact requested artifact checks remain open.
- Broader unauthenticated GitHub searches: Gothic3Heroes (0), gothic3 multiplayer (0), G3MP (4, mostly unrelated), Pigeonade (0), gothic3 (21), exact Gothic 3 (52). The broad search found yevh/Gothic3 / Myrtana Accord. The previous narrow-search result did not establish absence of a co-op project.
- Myrtana: one main branch and release v0.35.0-beta.1 at commit 327c0b0a2409db1f7ba0ba9a7f09cb171c5500e0; no forks returned. Official 3.15 MB archive downloaded, published SHA-256 and 31 internal checksums verified. Package contains PowerShell and native binaries but no C++ core source. Five published stack hashes match our stored baseline; three current files now intentionally differ due to isolation. Installer was read, not executed. See COMPATIBILITY.md and private research evidence.
- G3MPFHook at commit 6f059f1e877bfb9fb72e87a8f7719a7cfd992abc is an MIT-licensed debugging/hot-mounting tool, not proof of a co-op implementation. No releases/forks returned; one master branch inspected. Not installed.
- Saved local inspection found Script_AnimLab.dll in the original copied baseline. It was preserved and disabled only in the test copy so it cannot silently alter the new baseline.
- Implemented a local x86 shell-folder proxy with no registry or global profile changes. Six engine/PUP import-library names redirected; all originals backed up. Unknown folder identifiers fail closed. Known profile paths map beneath the test installation.
- Checks actually run: native Release build; independent probe for six profile folders plus local write, unknown-folder rejection and system-folder access; 28 active binary hashes/import tables; unchanged 12 protected save files; complete baseline/overlay verification of all 235 source files; MCP eight-tool checks. No game process or mod installer started.
- Corrected legacy directory classification to alpha: author page labels the release Public Alpha, and public forum-index author activity in 2024 makes a categorical abandonment claim unjustified. Added Heroes and Myrtana with honest QA levels and regenerated DIRECTORY.md.
- Remaining scope: API-level save-path verification does not prove every possible runtime access. Added mods/external launchers require another audit. New source-code development still needs a usable licensed core; Myrtana can be evaluated as an artifact without claiming its native core is open source.

## Continuation: real client startup and bounded SDK diagnostics

- Upstream fast-forward check remained current. Latest unauthenticated open-list check returned three issues and two PRs, with no Gothic/Genome matches in their titles/bodies.
- Manually staged the pinned Myrtana archive in the test copy after import review; installer never executed. Added a separately built GPL SDK QA helper. Native audit now covers 32 active modules. Full baseline/overlay comparison still verifies all 235 original files and their expected test-copy changes.
- Contradiction reported: package doctor names Update Pack 1.04.x, while installer and hash manifest do not establish it. No such additional pack was installed. Complete supported-stack compatibility remains unknown.
- Windowed 960×720 failed in both base and modded copies; restoring 1024×768 allowed startup. The real mod loaded, reported verified native layout, and the actual client joined the local server. Network welcome and menu-only state were observed.
- Corrected local SDK helper exports: this build's script loader requires `_ScriptInit@0`. Both decorated and alias exports now exist. Compilation alone had not detected the loader mismatch.
- Menu bot asserts startup world/no hero/no save permission, isolated save-folder query, and actual server-side client registration. Callback detachment before deferred window close avoids the prior synchronous-shutdown access violation. A subsequent bounded menu run passed. A negative-control run requiring a nonexistent world failed with the expected state timeout and cleaned up its processes.
- Failed bot experiments: direct session start in the startup world; then campaign-world restart plus session start. The latter loaded the campaign world but session initialization did not return. These incomplete entry sequences are disabled by default; they do not establish a mod bug in normal menu entry.
- Original save hashes remained unchanged in all completed runs. No actual disposable save/load or two-client gameplay test has passed. Remote proxy spawning remains off. Directory QA stays none. See RUNTIME-QA.md; private receipts and complete per-run logs stay in local/runtime/.

## Continuation: menu-driven campaign entry (Claude Code session, 2026-10-16)

### Observed (from private run logs, re-read this session)

- Run 230338: `startmenu` (WM_COMMAND to "Neues Spiel", id 20571, through the game GUI dispatcher) followed by `confirmnewgame` ("Ja", id 20711, inside the modal confirmation) reached `running=1 save_allowed=1 world=G3_World_01 hero=PC_Hero` with the save folder in the isolated profile. No OS input was simulated. Myrtana logged `world=1 gameplay=1` and the Ardea start NPCs.
- The outer `startmenu` call never logged its return: the new game keeps running inside the helper's dispatch frame. The helper therefore refused every later non-read-only command as nested, so no save was attempted.
- Run 231120 (`focusnew`/`focusyes`) only set GUI focus; whatever activated the buttons in that run is not recorded. It is not counted as a bot-only entry. In that run the in-game save dialog opened, with "Speichern" and "Löschen" disabled before a slot was chosen.
- Runs 230522 and 231025 produced no helper log (QA flag not enabled); diagnostic only.
- `isolation/verify.py` passes under the owner's Windows account; the deployed QA DLL hash equals the current build.

### Derived

- Campaign entry by bot is solved in principle; the blocker is the helper's own nesting guard, not the game or the mod.

### Tried in this session (runs 232208 to 233840)

1. Inline `startmenu` + `confirmnewgame` with the nesting guard relaxed (232208): the campaign started, then the session hook stopped being called about 3 s later. The cause was not the nesting.
2. Focus + Enter queued to the game window's own message queue (`pressenter`, 232506): bot-only campaign entry without the helper on the stack. Same stop after world activation.
3. Cause: the opening cutscene `Data/Video/G3_Intro.bik` (Bink header: 5354 frames at 25 fps = 214 s) blocks the session loop, so neither Myrtana's frame driver nor the QA hook runs. The owner identified it from the screen. Low CPU, window responsive, no extra dialog windows.
4. The runner queues one Escape (`WM_KEYDOWN/UP VK_ESCAPE`, posted only to the game process's visible window, no OS input). This skipped the intro in every run (233218 onward). The runner then waits for Myrtana's `Native new character qualified` line before sending any command.
5. `gCWorld::CreateSaveGame("CoopQA_Smoke")` returned 3, `ExistsSaveGame` = 0, and no file appeared. Neither the engine nor Myrtana logged a save. This is not the game's real save path. Native in-game save remains untested.
6. Myrtana persists co-op progress as a server-side campaign checkpoint (`campaign.g3coop`: `G3COOP_CAMPAIGN 3 …`, `player|1|QAHost|…`, `native_inventory|1|…`). The suite asserts this checkpoint instead of a native save.
7. Leaving a running campaign: Myrtana refused a raw posted `WM_CLOSE` ("window close before DefWindowProc") and the menu path "Spiel beenden" → "Ja" ("menu quit before termination flag"). It admitted `WM_SYSCOMMAND/SC_CLOSE` (the title-bar close path through DefWindowProc) with the game menu open: "Native early window close admitted", then the engine logged `DestroyEngine - all engine modules destroyed`.
8. The new-game prompt reads "… ein neues Spiel MIT Alternativem Balancing …": the Community Patch alternative balancing option is on in the test copy.

### Checks that ran

- `smoke.py --game --suite campaign`: PASS twice (233733, 233814). It asserts: menu state and isolated save folder, server-side client registration, bot-only new game, intro skipped, Myrtana character qualification, `running=1 paused=0 save_allowed=1 world=G3_World_01 hero=PC_Hero` with the isolated save folder, the server campaign checkpoint holding the host character and inventory, the normal engine shutdown, and the 12 protected saves hash-identical.
- `--suite menu`: PASS (233809). `--suite menu --negative-control`: FAIL as required (233840).
- Single client only. No two-client, combat, inventory, quest or reload assertion has run. QA level stays `none`.

### Unknown

- Whether a native save (in-game dialog or F5) works under Myrtana, and where it writes.
- Myrtana "Continue campaign" (server restart without `--new`, reload of the checkpoint) is untested.
- Myrtana also writes per-campaign intent journals on the client: `<game dir>/CoopSaves/native-{npc,inventory,reputation}-intents-<owner>-<campaign>.txt` (+ `.lock`). They persist between runs. Before testing "Continue campaign", decide whether each run starts with an empty `CoopSaves` (archived into the run folder) so leftovers from earlier campaigns cannot affect results. The local copy check now allows only this name pattern there; after the change it passed again.

## Continuation: two clients (2026-10-17)

### Observed

- A second full copy of the install (client copy) was created from the verified host copy with a hash receipt; it has its own redirected profile. Its isolation check passes.
- The second `Gothic3.exe` exited with code 0 right after the patch modules loaded. The host process holds a named mutex `\Sessions\1\BaseNamedObjects\GenomeMutex`. After the host's QA plugin closed its own handle to it, the client started normally. No window-class check was hit.
- The client joined as player 2 ("Xardas companion / co-op hero"), the server admitted its campaign character binding, and each client logged the other in its roster.
- Shipped `g3coop_tuning.ini`: `spawn_in_world_proxies = 0`, `shared_npc_scene = 0`, `shared_npc_death = 0`, `shared_npc_kill_xp = 0`, `character_sync = 0`. NPC deaths in the Ardea start scene (two slaves) were seen only on the host, and the server logged nothing about them.
- Quest sync works in the shipped configuration: host `RunQuest` reached the client in 4 s; a client completion is logged as "Native quest completion deferred ... awaiting_canonical", the server logs "progress=completed committed=1", then both clients apply it.
- `coop` suite: 3 of 3 PASS; negative control FAIL as required. Myrtana writes `CoopSaves/quest-intents-<owner>-<campaign>.txt` as well; the copy checks now allow quest, loot and death journal names (types taken from DLL strings).
- `rejoin` suite: FAIL. After a normal close and restart with a new game, the client logged "Campaign character binding deferred: reason=3" every two seconds and never received the campaign's quests.

### Derived

- The QA level `automated` is justified for the scope "two players in one campaign, shared quest state both ways", not for story co-op as a whole.
- Rejoin most likely requires loading the player's own campaign-bound native save.

### Unknown

- Native saving under Myrtana; whether "world clock synced to host" ever fires; loot, reputation and inventory sharing; behaviour with the experimental switches on.

## Continuation: path decision (2026-10-17)

Owner's goal: play Gothic 3 in co-op with friends.

### Observed

- Session start: upstream has one new commit (console playbook pages), no Gothic, Genome or Myrtana issues or PRs.
- Myrtana's LICENSE in the verified ZIP: "This starter kit is released under the MIT License, except where noted." Nothing in the package is noted as excluded; the file also says to follow the Gothic 3 SDK's licence when linking against it. The GitHub repo shows no detected licence (NOASSERTION). The earlier note "MIT for the starter kit" read this too narrowly: as written, the shipped binaries (`Script_G3Coop.dll`, `g3coop_server.exe`) fall under MIT too.
- Tooling present: Java 22, Docker 28.4. Ghidra not installed.

### Derived

- AGENTS.md path 1 (finish what exists) applies: Myrtana exists and is buggy. Per reviving-abandoned-mods.md: decompile the native layer, audit, fix as a separate package, test live and automate, then make it easy to play and give fixes back to the author.

### Unknown

- Whether the author considers the binaries covered by the MIT text; ask before redistributing modified binaries.

## Continuation: decompiling Myrtana (2026-10-17)

### Observed

- Ghidra 12.1.4 (official release, SHA-256 `ddac49f9...d4db` matched the release notes) headless over `Script_G3Coop.dll` from the verified ZIP: 21,975 functions, 1 decompile failure. Output stays in the owner's private workspace.
- The binding check returns reason codes; for reason 3 the mod shows "Co-op: this character already has campaign progress. Use Continue or select a new character profile." Reason 2 asks for a New Game or an explicit import; the fallback asks to load the character's matching campaign save.

### Derived

- The failed `rejoin` suite tested the wrong flow: a returning player is meant to load their own campaign-marked native save, not start a new game. Reason 3 is the mod working as designed. The rejoin result in the write-up must say so.
- Native saving is therefore required for playing over several sessions; testing it is the next step.
- Native save: F5 posted to the game window did not save (the game reads gameplay keys elsewhere). Myrtana's `IsSaveAllowed` hook only logs F5 state and passes the game's answer through. Myrtana itself triggers a native "QuickSave" through two internal Game.dll calls reached from `gCGUIManager::HandleMessage` (+0x78, +0x7e), checking for the E8 opcode first. The save dialog opens through the GUI; its "Speichern" button stays disabled until a slot is chosen.
- Leaving a campaign: the close and load gates refuse while a global Myrtana state object shows pending work (several flags and a counter); once set, nothing in the gate clears it.

### Contradiction (stop and report, AGENTS rule 6)

- Release notes promise "See and fight beside other players" and shared NPC state. The shipped tuning file says remote bodies are experimental and off ("frozen_ghost": static pose, possible flicker; animation playback unsupported; two-game behaviour open), and `shared_npc_scene` is a "one-NPC health authority" for a single configured target (`Orc_GameStartRaider_Scout_01`); `shared_npc_control` "does not yet replicate NPC animation, target selection, death or XP". The docs it cites (`docs/PROXIMITY_INVESTIGATION.md`, `docs/CAPABILITY_AUDIT.md`) are not in the public repository.
- Derived: Myrtana provides networking, identity, campaign persistence and quest/reputation/loot/inventory sync, but not visible animated players or general enemy and combat sync.

## Continuation: build from zero chosen (2026-10-17)

The owner chose to build an own co-op mod on the Gothic 3 SDK (AGENTS.md path 2) and reuse everything learned so far. Myrtana stays a reference, not a dependency.

### Observed (private Ghidra analysis of the game's Game.dll; no decompiled game code leaves the workspace)

- Myrtana's "QuickSave" routine calls two internal Game.dll functions reached from `gCGUIManager::HandleMessage`: a `gCHUDFileManager` singleton getter and `gCHUDFileManager::LoadGame(name)`. It loads the "QuickSave" slot; it does not save.
- `gCHUDFileManager::SaveGame(name)` only calls the exported `gCWorld::SaveGameWorld(name, image, &header, 0x18)`. The 24-byte header is `1` followed by year, day, hour, minute and second read from the clock property set of the `World_MCP` entity. A mod can save the way the game does through exported functions only.
- `gCHUDFileManager::LoadGame(name)` reads the header, stops the session, disables thread pools, swaps or reloads the world, calls the exported `gCWorld::LoadGameWorld` and moves the player camera to the saved position. Rebuilding it from exported functions is more work; the GUI load dialog is the safer first path.
- `Gothic3.exe` and `Engine.dll` import no Steam library; `Engine.dll` reads input through `DINPUT8.dll`, which is why keys posted to the window reach menus but not gameplay keys such as F5.

### Own mod, stage 1 (hook point)

- Working title G3 Together (`Script_G3Together.dll`), GPL-3.0-or-later, private workspace for now.
- A subclass of `eCEngineComponentBase` registered from `ScriptInit` with `eCModuleAdmin::RegisterModule` gets `Process()` about 100 times per second in the campaign world, plus `WorldActivate`/`WorldDeactivate`/`PreShutdown`. This is the engine's own module interface; no vtable patching needed.
- The test bot now lives inside the mod (active only with the flag file). A profile switch moves Myrtana and the old helper to quarantine in both copies and records it in the receipt; both isolation checks follow the receipt.
- Stage 1 exit test passed: bot-driven new game, the mod logs `world=G3_World_01 hero=PC_Hero pos=... yaw=...` every second at 95 to 103 frames per second.
- The intro video starts only after the world is active. An Escape sent during loading is lost and the video then runs its full 214 s; the runner now waits until the mod's per-second report stalls before sending Escape.
- Open bug in our mod: closing the game crashes in engine shutdown at `Engine.dll+0x5B2B6` with a register pointing into our DLL, also when the module unregisters itself in `PreShutdown`. Under analysis.

### Own mod, stage 2 (second player visible)

- Transport: ENet 1.3.18 (MIT, git tag pinned to its commit), listen host, 20 Hz state with world hash, position and rotation quaternion. ENet must be compiled with `/Gz` like the Gothic script target.
- Default ENet timeouts dropped a joining client after about 9 s: the client's engine loop stops during world load and the intro video. Peer timeouts are now 60 to 180 s, and clients reconnect automatically.
- Puppet: `Entity::Spawn(Template("Slave_01"), pose)`, collision off, `Entity::MoveTo` every frame with interpolation; removed when its player leaves, goes stale or is in another world.
- Stage 2 exit test passed 3 of 3: both sides show the other's puppet; the client walks a 3 m circle in eight steps and the host's puppet follows within 50 cm (errors 2.5 to 42 cm, identical across runs); the puppet disappears when the client leaves; both games shut down cleanly. Negative control (demands a point 10 m away) fails as required.
- Not yet: animation, the player's real appearance, AI of the puppet NPC (it still runs its routine between moves).
- Stage 2b (puppet motion), all in the shipped game, no new assets: animation slot 0 of `eCVisualAnimation_PS` (`GetMotionDesc(type 0)`) is the motion a body visibly plays. Movement motions (`_Move_`) cannot be replayed with `StartPlayAni`; the engine plays them only while a body travels. Feeding the puppet's movement controller (`PSCharacterMovement::SetMovementMode` + `SetMovementGoal`) with the remote hero's mode makes the engine pick walk, run or sprint itself. Restarting an AI goto every 150 ms kept the body in its start-up pose.
- A spawned NPC keeps reacting to perception (watching the Ardea fight, observing suspects) even with an idle task; setting its routine property `AIMode` to `Abandoned` and the routine name to empty stopped that.
- Bug found by the 5 Hz trace: comparing ENet's clock with GetTickCount made fresh states look stale after unsigned underflow; the puppet was respawned more than 400 times in one run while an earlier, weaker test still passed.
- Stricter stage 2 passed 3 of 3 (negative control fails): one spawn per player, only the idle task, at least 10 walk/run samples during real walks (84 to 85), puppet within 60 cm of the player after each stop (0 to 1.6 cm).
- Puppets now use the hero's own template `PC_Hero` (Myrtana's adapter rejects it; reason not documented). In our tests the game still resolves the real player through `Entity::GetPlayer()` and by name on both sides, and stage 2 passes 3 of 3 with it. Puppets are made immortal because the template may carry the hero's starting inventory.

### Own mod, stage 3a (NPCs with host authority)

- NPC identity across copies: `eCEntityProxy(entity).GetEntityID()` (GUID) on the host, `Entity(bCPropertyID(bCGuid(raw)))` on the client. The same world data gives the same GUIDs.
- Mirrored NPCs on clients kept being handed tasks by scene scripts (`ZS_RevolutionAttack` in the Ardea battle, `ZS_Boss`, `ZS_HangAround`) about six times a second; undoing them per frame left one-frame windows. Clearing combat targets to stop that crashed the game twice in `gCScriptProcessingUnit::sAICombatMoveInstr` (a running combat instruction with an empty target).
- Working fix: wrap every registered AI state and AI function (441 in this build) through `gCScriptAdminExt`'s name maps; the run-time stack's top entry names the state being run, so one wrapper can dispatch to every original. Bodies we drive run only the idle task. A wrapper call naming no wrapped state is counted and was 0 in every run. The per-DLL arrays (`gSScriptDLL`) held a null entry in this build and crashed the first attempt; the admin maps are fine.
- `CanReceivePerceptionFrom` is a plain script (`gSScript`); a filter on it installs but never triggered in these runs.
- Stage 3a exit test passed 3 of 3 (negative control fails): 11 to 17 NPCs seen on both sides, all living ones mirrored, none kept the game's AI across two samples, position error median 5 to 15 cm (worst 137 cm), every NPC that died on the host (slaves in the Ardea fight) dead on the client, clean shutdowns.

## 2026-10-17: own mod, stage 3b (damage with host authority), in progress

Observed (runtime, test copies only):
- gCEntity::DoDamage (Game.dll) calls the admin script `DoLogicalDamage` (victim, damager), then
  `AddHitPoints` (victim, damager, -DamageReceiver.DamageAmount) if the first returned non-zero.
  Script names read at runtime from the bCString globals Game.dll RVA 0x3f188c / 0x3f1890.
- The hit judgement (AssessHit, Script_Game.dll RVA 0x2b580) only records the damage in
  DamageReceiver.DamageAmount (68 for It_1H_Sword_Rust_01 on a slave); hit points drop afterwards.
  `OnDamage` calls AssessHit directly; `DoLogicalDamage` calls it after gates that need a swing in
  progress (weapon DamageType is None outside a swing, attacker action/animation state).
- Calling DoLogicalDamage from a bot without a real swing returns 0 (no damage).
- Calling gCEntity::DoDamage directly from the mod raised an exception: do not do that.

Built (not yet verified by a passing suite):
- combat.cpp: one filter on both `DoLogicalDamage` and `OnDamage`. Client, victim is a mirrored NPC:
  measure direct HP change plus recorded damage, undo both, send HitNpc(guid, delta) to the host.
  Host, victim is a puppet: same, send HitPlayer(delta) to that client, whose hero takes it via AddHitPoints.
  Host applies HitNpc via AddHitPoints and Kill() at 0 HP. Protocol v5 (messages 7/8, reliable).
- QA: arm <template>, hitnpc <guid>, hitpuppet <id>, hp, combatoff (negative control), gamestr <rva>.
- together.py suites stage3b and combatdiag.

Open:
- stage3b has not passed yet (first runs: zero damage because the bot hit through the wrong path).
- Real swings are not bot-driven yet; the per-hook call counter (logical_calls) is meant to show NPC
  fights in the opening raid running through the DoLogicalDamage filter.
- Known limits: host-side kill is Kill() instead of the game's own knockout/death choice; client hero at
  0 HP gets no death reaction yet; puppets hold no weapons (no equipment sync).

Update (combatdiag, host only, same day):
- QA hit through OnDamage + AddHitPoints(-recorded) now deals damage at once: orc scout 459 -> 392
  (recorded 67, It_1H_Sword_Rust_01). The filter saw the call.
- Contradiction: logical_calls=0 during the opening raid, although orcs and slaves were fighting
  (their hit points dropped). So NPC melee hits do NOT reach our swapped `DoLogicalDamage` entry.
  Either Game.dll calls the script through a path that bypasses the gSScript function pointer, or NPC
  hits take another route. Unknown which; resolve before relying on the filter for real swings.

## 2026-10-17: workspace moved

The mod now lives in its own repository next to a private workspace (test copies, SDK, builds) and this
playbook clone. Observed while rebuilding from a fresh build directory: ENet 1.3.18's compress.c does not
compile under /Gz (its range coder functions are stored in ENET_CALLBACK = __cdecl pointers); compile that
one file with /Gd. An older build directory had hidden this with a stale object file. Stage 1, 2 and 3a
suites passed again from the new layout (one run each, 2026-10-17).

## 2026-10-17: how hits really reach hit points (hit-path probe), stage 3b design

Observed (own mod, isolated copies; a probe wrapped the `AddHitPoints` script and recorded return addresses
on the stack, module+RVA, during the opening raid on the host):
- 225 of 236 calls in 45 s: weapon touch damage. `gCTouchDamage_PS::TriggerTarget` (Game.dll RVA 0x693b0)
  calls an admin script whose name is a property of the weapon's TouchDamage set, then
  `AddHitPoints(victim, -DamageReceiver.DamageAmount)`, then the hit effects. Example: victim Hamlar,
  source It_Axe_OrcWaraxe_01, -8.
- 11 calls: the AI combat loop `gCScriptProcessingUnit::sAICombatMoveItlLoop` (RVA 0x16dd00) calls `OnHit`,
  then `AddHitPoints` (source: the attacking NPC itself).
- `gCEntity::DoDamage` (RVA 0x666d0) calls `DoLogicalDamage`, then `AddHitPoints`; not seen in the raid.
- All three reach `AddHitPoints` through `gCScriptAdmin::CallScript` -> `RunScript`, and the swapped
  `gSScript::m_funcScript` entry is what runs. The swapped `DoLogicalDamage` entry was never called in the
  raid, which explains the contradiction noted earlier.
- On a client, mirrored NPCs still hit each other through physics touch damage (their weapons collide even
  with their AI off).

Derived (design, verified by the stage 3b suite):
- Enforce host authority in one place, the `AddHitPoints` script (negative amounts only).
  Client: own hero hit a mirrored NPC -> send the amount to the host, apply nothing; a mirrored NPC hit
  another mirrored NPC or the hero -> drop (the host simulates those). Host: anything hit a remote player's
  puppet -> forward to that player, whose hero takes it via `AddHitPoints`.
- `Entity::Kill` removes the body from the NPC list and from view; let NPCs die with
  `Routine.FullStop(); Routine.SetTask("ZS_RagDollDead")` instead, on host and client.

Unknown:
- Outcomes that `Script_Game.dll` applies by calling AddHitPoints directly (parries, perks) bypass the
  admin and are not covered.
- Knock-out instead of death (ZS_Unconscious) is not decided by the host yet.
- Real player swings are not bot-driven yet; the suite hits through `OnDamage` + `AddHitPoints`.

Stage 3b result (2026-10-17, mod commit "Stage 3b"): suite `stage3b` passed 3/3 with host and client; the
negative control (filter set to pass-through on the client) fails as required ("Client changed the NPC's hit
points itself"); stage 1, 2 and 3a suites still pass; clean engine shutdown and unchanged real saves in every
run. Fixed on the way: the client's own hit judgement started the death state before the host confirmed it
(now only after the host reports the NPC dead), and a target at the 40 m interest edge made the check flaky
(the suite now picks a raider near the client).

## 2026-10-17: stage 4, shared quest state (own mod)

Observed:
- gCQuestManager_PS holds 570 quests in this build (Steam, CP 1.75.14). Status is read with
  `quest->AccessStatus()`; `RunQuest`, `SucceedQuest`, `FailQuest` on the manager change it with the game's
  own side effects, `gCQuest_PS::SetStatus` sets it directly.
- In all stage 4 runs every applied change went through the manager functions (forced=0).
- The client's mod connects to the host while the client is still in the main menu; a state sent at that
  moment cannot be applied (no running game). The client now asks for the host's full state once it is in
  the game and after every reconnect.

Decisions (shared vs per player):
- Quest status: shared, host decides. Each player's game applies it through the manager, so each hero gets
  the quest's experience once (shared progress, individual reward).
- Not covered yet: dialog/info state (gCInfoManager_PS), delivery counters, enclave and political standing,
  item rewards handed out in dialogs, world objects.

Result: suite `stage4` passed 3/3 (changes arrive within one 0.5 s poll in both directions and for a late
joiner); negative control (quest sync off on the client) fails as required; stage 1 to 3b suites still pass.

## 2026-10-17: stage 4b, shared dialog state; can a client hold a dialog?

Observed:
- gCInfoManager_PS holds 4591 dialog options (gCInfo_PS) in this build; `InfoGiven` marks an option as
  told, and other options' `ConditionPlayerKnows` read it. Longest quest and info names fit in 62 bytes.
- Suite `stage4b` (told options reach a late joiner, host -> client, client -> host; an untouched option stays
  untold) passed 3/3; negative control fails; earlier suites still pass. One stage 3b run failed because the
  QA bot's command file write collided with the mod reading it (Windows sharing violation); the bot now
  retries, and the rerun passed.
- Calling `gCInfoManager_PS::StartInfoManager(hero, Diego)` during the opening raid returned false on host
  and client alike (Diego was fighting), so it says nothing about co-op.

Derived from Script_Game.dll (decompiled, private):
- A dialog runs inside the NPC's AI: the reaction at RVA 0xae9b0 sets the NPC's task `ZS_Talk` when the
  player addresses it and no dialog runs; the AI state `ZS_Talk` (RVA 0x24ff0, registered from
  `Reaction_Tasks\ZS_Talk.cpp`) calls `PSInfoManager::Start` and continues in `ZS_Talk_Loop`.
- The mod's client keeps mirrored NPCs out of every AI state except its idle task, so a client most likely
  cannot talk to any host-owned NPC yet. Unverified until a calm test scene exists.

## 2026-10-17: stage 4c, a client's dialog with a host-owned NPC

Correction to the entry above: the derived claim "a client most likely cannot talk to any host-owned NPC"
was wrong. Tested: on the client, `AssessTalk(mirrored Diego, hero)` returned 1 and the dialog ran through
the game's own info manager, although the mirrored NPC's AI mode is Abandoned (10).

Observed:
- Registered names around talking (from the script admin's maps at runtime): scripts `AssessTalk`
  (Script_Game.dll RVA 0x30d50), `PlayerOpenDialog` (0x6bcf0), `StartSayRefuseTalk`; AI states `ZS_Talk`,
  `ZS_Talk_Loop`, `PS_Interact_Talk`, `IS_Running_SelectInfo`, `IS_Running_ProcessInfo`.
- `AssessTalk(npc, player)` refuses NPCs in combat; when it accepts, both get the other as current target
  and the task `ZS_Talk`.
- During the opening raid nobody talks. The raid comes in waves: clearing it (QA only) took two or three
  rounds of killing every raider within 300 m; then Diego, Gorn and Milten switch to small talk.
- `Entity::Kill()` on raiders in mid-fight crashed the host about a second later (EIP=0, access violation
  in Gothic3.exe). Killing through `AddHitPoints` and `ZS_RagDollDead` did not.
- Right after the raid, Hamlar starts a dialog with the host's hero by himself (important info).

Built: the client tells the host when its hero begins or ends a dialog with a mirrored NPC; the host holds
that NPC (no routine, idle task, AI blocked) until the dialog ends or the player disconnects. The client stops
moving the NPC during the dialog and lets it run `ZS_Talk`/`ZS_Talk_Loop`.

First stage 4c run: the client heard BPANKRATZ31453, BPANKRATZ31454 and, after choosing the first option,
FILLER926; the host's Diego did not move (0 cm) and was released afterwards; all three told options reached
the host.

Stage 4c result: suite `stage4c` passed 3/3; negative control (host ignores the dialog notice) fails as
required with "host holds Diego" timing out; stage 1 to 4b suites still pass. In 2 of 8 earlier runs the
client's `AssessTalk` was refused once (seen with `has_info=0` while the host's hero was in Hamlar's dialog);
the suite now retries like a player would and records each refusal. Unknown: why that info was briefly
unavailable. Also fixed: a client's intro video paused its state stream long enough (5 s) for the host to
remove and respawn its puppet; the threshold is now 15 s, and games disconnect actively at shutdown.

## 2026-10-17: stage 5, world items (own mod)

Decisions (shared vs per player): each hero's inventory belongs to that player. Items lying in the world
belong to the host: what one player takes is gone for everyone, and the first claim wins.

Observed:
- Around the start position in Ardea there are 34 world items within 30 m (potions, gold stacks, apples,
  bread, a silver cup, a teleport stone); `Entity::GetEntities()` returned 243 entities in range.
- Registered names around items include AI function `_AI_TakeItem` (Script_Game.dll RVA 0x4adb0), player
  state `PS_Normal_Take` (0x82240), scripts `OnItemReceived` (0xb60b0), `PlayerOpenLoot`, `PlayerOpenTrade`,
  `AssessTakeItem`, `SendPerceptionTakeItem`, `PickPocket`.
- Taking: `PS_Normal_Take` with the item as the hero's current target runs `_AI_TakeItem`, which ends in
  `PSInventory::InsertStack(item)`. Afterwards the item's GUID no longer resolves (it went into a stack). A
  bot can take items the player's way: `NPC.SetCurrentTarget(item)` and the task `PS_Normal_Take`.
- `OnItemReceived` is only called from `gCInventory_PS::TransferItemsTo` (inventory to inventory: trade,
  loot, giving), not when taking from the ground.
- `Entity::Decay()` removes a world item at once (its GUID stops resolving, it leaves `GetEntities`).
- A gold stack (`It_Gold_Stack_01`) arrives in the inventory as coins of another template; what a take adds
  must be read from the inventory, not from the world item's name.

Built: the mod wraps `_AI_TakeItem` for the local hero, notes the target's GUID first and reports a take
once that GUID is gone. A client claims at the host; the host removes the item (Decay) and tells everyone,
or refuses if it is gone already, and the client gives back what the take added. The host's own takes are
announced the same way.

Not covered yet: NPCs picking items up, items dropped into the world, chests and corpses (plunder), trade.

Stage 5 result: suite `stage5` (client take disappears on the host, host take disappears on the client, both
grabbing one item leaves it with exactly one player, a take of an item already gone on the host is refused
and given back, an untouched item stays) passed 3/3; negative control (item sync off on the client) fails as
required; stage 1 to 4c suites still pass.

## 2026-10-17: stages 5b and 5c, plunder and trade (own mod)

Observed:
- After the opening raid, chests near the start had the same contents on host and client (static world
  data), but every orc corpse differed: plunder is rolled per machine (e.g. host It_Gold:48, client
  It_Gold:47 plus an axe). On the client, mirrored corpses also kept their weapons.
- `OnItemReceived` is only called by `gCInventory_PS::TransferItemsTo` for `Book_CPT` items, so it is no
  general hook for transfers.
- `PlayerOpenLoot` / `PlayerOpenTrade` (Script_Game.dll RVA 0x6bfd0 / 0x6c4c0) record the partner, switch
  the GUI page and put the player into `PS_HUD`; the transfers themselves come from the screen
  (`TransferItemsTo`). `PlayerOpenTrade` also calls `PSInventory::StackTradingGold` on the trader.
- After a client rewrites its copy of an inventory, stack order differs from the host's; refer to stacks by
  item template, not by index.
- Every NPC carries a `Fist` item; it is no trade good.
- Setting `ZS_RagDollDead` from outside on an NPC in mid-fight crashed the host once (access violation in
  Script.dll+0x13ea3, right after `gCQuestManager_PS::OnNPCKilled`). QA now kills with a real lethal hit
  (hit points to 1, then a weapon hit judged by the game). The host still sets that task when a client's hit
  kills an NPC (stage 3b): a known risk.

Built: the host sends the contents of chests and corpses near any player (and of a trader a client deals
with) whenever they change; a client overwrites its copy. A client that opened loot or trade sends what its
player changed as one transaction (container-side deltas: item and gold together for a purchase); the host
applies all of it or none, and on a refusal the client undoes the change to its hero, including changes made
since that were not reported yet. Found by the race test and fixed: contents arriving from the host between a
client's take and its next poll overwrote the take unreported, and both players kept the item.

Results: suite `stage5b` (corpses show the host's contents, client and host takes show on the other side,
two players taking one stack, a refused late take undone) passed 3/3; its negative control fails; earlier
suites still pass. Suite `stage5c` (the client sees the host's goods, a purchase moves item and gold on the
host, a refused purchase is undone) passed in its first run; series pending.
- Item names are not unique identifiers: Diego carried `Appow:20` and `Arrow:20`; recreating his inventory on
  the client by item name produced `Arrow:40`. Containers are now synced by item template ID (the template's
  property ID, identical on every machine), with the name kept only for logs.

Results (2026-10-17): suites `stage5b` and `stage5c` passed 3/3 each with template-ID keyed contents; both
negative controls (container sync off on the client) fail as required; stage 1 to 5 suites still pass.
Not covered: the real trade and loot screens (the bot uses PlayerOpenLoot/PlayerOpenTrade and inventory
transfers), selling to a trader and its price rules, NPCs picking items up, items dropped into the world,
item quality (contents are recreated with quality 0).

## 2026-10-17: killing an NPC on behalf of a client (stage 3b follow-up)

Observed (Script_Game.dll, decompiled privately; runtime checks in the test copies):
- The hit judgement (AssessHit) caps the damage of any attacker but the player and sets it to 0 when it would
  kill (some victim types excepted): a real hit by a client's puppet left an orc standing at 0 hit points.
  Only the player's hits reach the death branches (`ZS_RagDollDead` when `IsDeadlyDamage`, otherwise
  `ZS_Unconscious`, `ZS_LieKnockDown`, ...).
- The crash after the QA kill: an access violation in `Entity::AttachTo` (Script.dll+0x13e80) called from the
  engine callback script `GetCombatMoveLength` (Script_Game.dll RVA 0xaa270) in another NPC's combat move,
  one frame after death tasks had been set by hand on fighting raiders. Cause not proven.
- Scripts around dying: `Kill` (RVA 0x1bc30: victim first, killer second; quest manager `OnNPCKilled`, story
  hooks, `GiveXP` for the player), `GiveXP` (0x4e160), `CanBeKilled` (0x2da00), `OnCheatKill` (0xab020: the
  console kill; self = the one who kills, other = the target; stops the target, clears its combat target, sets
  `ZS_RagDollDead`, or `ZS_Dead` for some species). With the arguments the other way round it did nothing.

Changed: when a client's hit would kill, the host sets the NPC's hit points to 0 and kills it with
`OnCheatKill` (the client's puppet as killer), then tells that client, which rewards its own hero with
`GiveXP(hero, NPC, -1)` (first test: 0 -> 200 experience). Before, nobody got experience for a client's kill.

## 2026-10-17: outside review (Claude models via API), hypotheses to test

Two questions went to Claude models through the API (packages and answers stay in the private workspace).
The crash question was declined by Fable 5.1 (refusal fallback routed it to Opus 4.8, consistent with the
cyber category); the design review was answered by Fable 5.1. Their points are hypotheses until tested:

- [Guess] Host authority has a radius: Genome streams sectors and processes NPCs only near the local camera,
  so a client far from the host has no host-side NPCs to mirror. Test: client hero 600/1500/4000 m away.
- [Guess] The ESC menu and inventory pause the session; if `Process()` stops, the puppet goes stale and ENet
  may drop. Test: open them on each side for 30 s and count Process calls.
- [Derived] Runtime-created entities (spawns, drops, arrows, summons) get different GUIDs per machine. Test:
  diff NPC/item GUID sets near both heroes once per second during the raid and after a drop.
- [Derived] Hits applied by the host through `AddHitPoints` do not trigger the NPC's perception: no
  retaliation, no crime. Test: a client hits a peaceful NPC; watch its task and target on the host.
- [Derived] A client hero at 0 hit points is unhandled (vanilla: game over); puppets are immortal.
- [Guess] Install parity: alternative balancing, PUP and patch options change damage and XP; hash them at join.
- [Guess] Pointer swaps and AI wrappers may not survive a host `LoadGame`.
- [Crash answer, inference] The freed entity in the GetCombatMoveLength crash was most likely the combat
  target of a surviving fighter, killed from outside in the same frame together with others; suggested
  mitigations: one kill per frame, clear references to the victim (but clearing other NPCs' targets crashed
  here before), reset the script admin's SPU proxies after our own CallScript calls.
- Suggested save design: the host's native save is the campaign; clients load a copy of it and the mod
  restores each player's hero from a per-player record kept in a sidecar file.

## 2026-10-17: exploration 1, how far host authority reaches (radiusdiag)

Observed (host stays at the start in Ardea; the client moves away):
- `Entity::GetNPCs()` and `Entity::GetEntities()` only list what is processed around the local hero: about
  20 NPCs within ~45 m and ~240 entities at the start. Yet 405 of the 414 owners of dialog options resolve
  by name (`Entity::GetEntity`) on the host, with positions up to 3.3 km away: far entities exist, but are
  not in the processing lists.
- Moving the client hero with `Entity::MoveTo` does not move the processing area: 30 s later the client still
  processed the Ardea NPCs (now 0.5 to 3.3 km away), and the NPC next to the hero had no task.
- With the game's own `Entity::Teleport(target)`, the client processes the new place within 5 s: 12 to 32 NPCs
  with tasks (`ZS_HangAround`, ...). At 3.3 km the guard Sven reacted to the hero (`ZS_ReactToZone`, then
  `ZS_Attack`). None of them was mirrored: on the host the same NPCs exist but have no task and do not move
  (one, Fadi, shifted 1.4 m in 30 s; unexplained), and the host's processing lists hold nothing near the
  client.

Derived: as soon as players part, each machine runs the NPCs around its own hero with its own AI, unsynced,
and the host's copies of distant regions are frozen. Host authority by mirroring only holds while players
stay within the host's processing radius. A campaign with friends needs regional authority: the machine that
processes an NPC owns it, the host records the results.

Also: QA moves for far places must use `Entity::Teleport`, not `MoveTo`.

## 2026-10-17: exploration 2, inventory and ESC menu (pausediag)

Observed (each side opened the page for 25 s through `gCGUIManager::OpenInventory` / `gCSession::OpenMenu`):
- Inventory: the session is not paused (`IsPaused` 0); the mod's per-frame work, the network and the other
  side's puppet stay fresh (state age 17 to 69 ms); the host's NPCs keep moving.
- ESC menu: the session is paused (`IsPaused` 1), but the mod's engine module keeps running: its frame counter
  keeps rising, state stays fresh (35 to 61 ms), the puppet stays. The review's worry that menus stop the mod
  and drop connections does not hold for this setup.
- Not measured yet: whether the host's NPCs freeze while the host sits in the ESC menu (likely, since the
  session pauses); co-op should then forbid or share the pause.

## 2026-10-17: exploration 3, the game's own save and load (savediag)

Native save/load in Game.dll of this build (Steam 1037023 + CP 1.75.14), found with Ghidra; the mod checks
the prologue bytes before calling, so another build is refused instead of crashing:
- `gCHUDFileManager` singleton getter at RVA 0xa92d0; `SaveGame` at 0xa9e90 and `LoadGame` at 0xa9a10, both
  `__stdcall(this, bCString const* name)` (a first try as `__thiscall` made the game look for an empty name).
- A save writes `<name>.g3savcpx` (~66 KB) and `<name>.g3savcpxdat` (~2.1 MB) into the profile's save folder.

Observed (three fixes needed, then one full pass):
- Loading a slot that does not exist leaves the game hanging. Guard: `gCWorld::ExistsSaveGame` first.
- A save taken while the client's puppet (a `PC_Hero` copy) exists cannot be loaded: FATAL
  "current world has more than one hero!". The mod now removes all puppets before saving; they come back
  from the next network update.
- `LoadGame` activates the new world without a WorldDeactivate first, so the mod crashed on entities it
  still held from the old world. It now forgets all held entities on WorldActivate as well.
- After that: the host saved, loaded its own save, the mod's function hooks still pointed into the mod DLL,
  the client stayed connected and its puppet reappeared. Then the client loaded a copy of the host's save:
  afterwards both sides saw each other and had 16 NPCs in common; both games shut down cleanly.

Not measured yet: whether quest and item state really match after the client loads the host's save (only an
untouched quest was compared, 0 on both sides), and how each player's own hero (level, inventory) survives
when every player loads the host's world.

Derived: the review's save design is feasible in principle: the host's native save is the campaign, clients
load a copy of it. Each player's own hero still needs a per-player record restored after the load.

## 2026-10-17: outside review 2, architecture for players who split up (hypotheses to test)

Asked Fable 5.1 (answered itself) to critique the plan "soft leash first, then regional authority with the
host as record keeper, no world server". Its points, all unverified until a test runs:
- [Inference] No world server: Genome has one processing area around the local hero and no headless mode;
  a server would be one more game with one area. A separate *ledger* process (quests, claims, kills, clock)
  could be split out later for host migration, but that is not world simulation.
- [Inference] Split simulation from the ledger: AI, positions and damage belong to whoever processes the NPC
  (the "owner"); item claims, container transactions, quest/info state, kill records and the clock stay with
  the host for every region. Stages 4 to 5c then stay valid, since they only need GUIDs to resolve.
- [Inference] "Sticky" ownership instead of "host wins in overlaps": the first machine that processes an NPC
  keeps it until the NPC leaves its processing list (with ~1 s hysteresis) or it disconnects or pauses. This
  avoids handovers in mid-fight, where this mod's crashes came from. Needs symmetric mirroring (every machine
  sends what it owns, mirrors the rest).
- [Inference] A world journal at the host (keyed by GUID: dead/down + killer, taken, contents, routine
  changes), applied lazily when an entity enters a machine's processing list; eager writes to frozen
  entities only if a test shows they are safe and saved. Mimic the native form of a dead NPC (compare
  property sets before and after a real kill) instead of inventing one.
- [Guess] NPCs may be re-placed at their routine position when they enter processing (Fadi's 1.4 m shift);
  then a handover snapshot must be applied after the NPC is processed, and positions need no streaming into
  frozen copies.
- [Inference] Dialog commands that change the world (enable/teleport an NPC, AI mode, guard status, scripted
  spawns) run only where the dialog was held; marking `InfoGiven` on the host probably does not replay them.
  Check the `gCInfoCommand*` classes. Flagged as the biggest gap not in these notes.
- [Inference] Leash: a pull-back does not buy consistency (a 35 m leash with a 45 m radius still leaves NPCs
  the host does not process inside the client's area) and is bad in fights; a warning is enough.
- Proposed experiments: E1 processing area and re-activation (where an NPC reappears), E2 writes to frozen
  entities and the native save, E3 runtime GUID diff, E4 a native kill by a far client, E5 clock, E6 host
  in the ESC menu as ledger, E7 dialog commands.

## 2026-10-17: exploration 4, install parity (suite `parity`)

The mod now fingerprints, at startup (94 ms): Game.dll, Script.dll, Engine.dll, every DLL in `scripts`, the
rules archives (Templates, Infos, Quests: full content), the other `Data` archives (name and size only), and
the options: the engine's own setup struct (`eCApplication::GetEngineSetup`: Difficulty, AlternativeBalancing,
AlternativeAI, MinHitDuration, XPModifier, QuickLoot) plus the `[Game]` lines of `Ini/ge3.ini` (for
ExtendedContent). The client sends it with its hello; the host refuses a different one and names the parts.
Observed: the engine reported `difficulty=Medium alternative_balancing=1 alternative_ai=1 min_hit_duration=6
xp_modifier=100` for this install, matching ge3.ini, so the struct offsets of the SDK fit this build. With the
client copy's `XPModifier=150` the engine reported 150 (the game reads `<game dir>/Ini/ge3.ini`) and the host
refused it: "install differs from the host: options"; the client stops reconnecting after a refusal. Passed
3/3; negative control (option unchanged, so no refusal) failed as expected.
Not covered: loose files in `Data` subfolders, custom difficulty sliders (where stored: unknown), options
changed in the menu after joining.

## 2026-10-17: exploration 5, writes to NPCs the host does not process (farwritediag, Fable's E2)

Host alone at the start; three NPCs 0.8 to 1.1 km away killed the way the host kills for a client (hit points
to 0, `OnCheatKill`), one more moved 4 m with `MoveTo`; then native save and load; then the game's own
teleport to each.

Observed:
- Unprocessed NPCs report hit points 1 and no task. After the kill: hit points 0, not dead, no task; that
  survives 15 s, save and load.
- On arrival (processed again) all three were alive at full health (600/700) with their routine task
  (`ZS_Boss`, `ZS_HangAround`, `ZS_WaitForFreeDestination`). A kill on an unprocessed NPC does not hold.
- The same kill a few seconds after arrival works (`ZS_Dead`, dead=1) for all three, and they stay dead when
  the host moves 1.3 km away and back into the 200 m ring.
- Moves: in one run the moved NPC (`ZS_HangAround`) stayed at the new place through save, load and
  processing; in the next run another (`ZS_Guard`) was back at its guard post (0.8 m from where it started)
  when processed.
- NPCs 200 to 240 m away were re-placed (moved a little, e.g. 0.6 to 1.4 m) without getting a task: there
  is an outer ring where the engine places NPCs by their routine before their AI runs.

Derived: results from a far region cannot be written into the host's frozen copies. They must be kept in a
journal (by GUID) and applied when the NPC enters a machine's processing list, where the existing kill path
works. Positions need not be streamed into frozen copies; routines place NPCs on activation.

## 2026-10-17: stage 3c, experience goes to whoever earned it

Found while rerunning stage 3b: client experience grew without our reward (and varied 100 to 250 with it).
Diagnostic (host kills the raiders with its own hits, the client never hits): host +1650 XP, client +1250.

Observed (Script_Game.dll, Ghidra): the `GiveXP` implementation is called directly (not through the script
admin) from the NPC death script and the knock-out script. Both reward the player when the killer is the
player or its party leader and the NPC's `gCNPC_PS::DefeatedByPlayer` is not set yet, then set it. On the
client, mirrored NPCs die through the death task, so every mirror that died paid this client.

Changed: a client sets `DefeatedByPlayer` on every NPC while it mirrors it (restored on release). The host
already rewards the real killer (`SendKilled` -> `GiveXP` on the killer's machine).
- Marking only once the host reported "down/dead" left 500 XP: hit points arrive before the death flag.
- After marking from the start, the remaining +500 came 44 ms after the host's quest `Ardea_Revolution`
  changed: the client's quest manager pays quest rewards when the synced status arrives. Quest rewards for
  every player are kept on purpose (usual in co-op); the stage 3c suite turns story sync off to measure kill
  experience only.
- Suite stage3c: 3/3 passed (client XP unchanged with 11+ dead mirrors, host XP grew); negative control
  (`xpguardoff`) failed as expected (client +200).
Also fixed in suites: stage 3b now accepts a raid kill by someone else between the client's claims when the
applied claims could not have killed (7 x 58 < 435 hp), and requires no client reward then; stage 5's race
accepts "already gone" for the slower taker.

## 2026-10-17: static analysis by a second agent (Codex): dialog commands, processing range, clock, death, hero

Static analysis only (Ghidra read-only, SDK); reports stay in the private workspace. Points checked against
this week's runtime results are marked.
- Processing range: the engine's "ROI" sphere, radius from `ge3.ini` `[Engine.Setup] Entity.ROI` (4000.0 =
  40 m in both test copies, checked), console `EntityROI`, `eCEntityAdmin::SetROISphere`; rebuilt with a
  2.5 m movement threshold and entity bounding boxes, which fits the ~45 m measured (checked against
  exploration 1). Its centre is the active camera, re-read every frame, so a second centre is not possible.
  `Entity::Teleport` moves the camera with the hero; `MoveTo` does not (fits exploration 1).
  The SDK declares `eSSetupEngine::Entity_ROI` as int, the binary uses a float. Parity does not hash
  `[Engine.Setup]` yet.
- Death: `Entity::IsDead()` is `AIMode == 9`, `IsDown()` is 8. `OnCheatKill` only stops the NPC and sets the
  task `ZS_Dead` or `ZS_RagDollDead`; the script `Kill` (run by those tasks) sets AIMode 9 and does the
  bookkeeping: quest `OnNPCKilled`, `GiveXP` guarded by `DefeatedByPlayer`, plunder generation, party and
  interaction clean-up. This explains exploration 5 (a far NPC never runs the task, so it never dies) and
  confirms stage 3c's finding. `ZS_Dead` with AIMode already 9 takes a fast path without `Kill`.
  `Entity::Kill()` destroys the entity (fits the earlier "body vanished" result).
- Knock-out runs a separate helper (AIMode 8, quest `OnNPCDefeated`, same XP rule).
- Dialog commands: there are no `gCInfoCommand*` classes in this build (Fable's guess was wrong); each info
  script line is a string command with up to two entities and two ids, dispatched by `IS_Running_ProcessInfo`
  in Script_Game.dll over 69 commands (`ECommand` in the SDK). `gCInfo_PS::SetInfoGiven` only stores the
  flag, so syncing InfoGiven replays nothing. World-changing commands include Attack, OpenDoor, SetClass,
  SetPAL, Sleep, the Set*Enabled dialog flags, SetRevolution/RaidEnabled, Set*Point, SetRoutine,
  SetGuardStatus, MoveTo, SetSectorStatus, StartRevolution, Spawn, Humanize, JoinNPC/JoinPlayer, GiveAll,
  ResetCrime, ModReputEnc. Per-player: GiveXP, SetGameEvent/ClearGameEvent, SetChapter, ModReputPol.
- Runtime IDs: entity IDs are random at construction; the exported, protected `eCNode::SetID` re-registers
  an entity under a new ID.
- Clock: `World_MCP` entity, clock property set (Year, Day, Hour, Minute, Second, Factor) with setters that
  keep the internal clock consistent; "world time" floats are HH.MM, not decimal hours; `PlayerSleep` jumps
  the clock, `PS_DuskToDawn` speeds it up (factor 3840).
- Hero record: level and XP, learn points, the full attribute map (base and modifier separately), inventory
  stacks with quality and learned/activation data, equipment and quick slots, political fame and crime
  arrays, game events, chapter; city fame and crime live on the enclave entities, not on the hero.
- Correction (same day): our reward never worked. `GiveXP` takes self = the NPC and other = who earns it
  (as the death script calls it); we passed them the other way round, it returned 0 and gave nothing
  (`givexpdiag`: result 0; swapped: result 1, +50 per slave). All client experience measured in stage 3b
  before came from the mirror's own death script. With the order fixed the killer gets it once (3b: 0 -> 100
  for the orc). Stage 3b's check "client experience grew" only became meaningful once mirrors stopped paying.

## 2026-10-17: stage A, no game over in co-op (player death and knock-out)

Exploration (herodiag, herodeathdiag) before the change:
- A client's hero took forwarded hits as bare `AddHitPoints`: at 0 hit points it kept walking (PS_Normal,
  AIMode 0) for 30 s and more: effectively immortal.
- The host's hero hit by an orc through the game's hit judgement fell at 0 hit points (`ZS_LieKnockDown`,
  AIMode 8 "down", state `ZS_Unconscious_Loop`) and got up after 5 to 10 s with 20, then 30 hit points.
- The death path (the kill cheat with an orc as killer): `ZS_Dead`, AIMode 9, `ZS_Dead_Loop` on either
  machine; the session kept running, unpaused, the other side still saw the puppet, but the player never got
  up (single player would need a load). Static: the script `Kill` prints `GO_GameOver` (message type 13)
  when the victim is the player.

Changed:
- `downed.cpp`: when the AI wrapper is about to run `ZS_Dead` or `ZS_RagDollDead` for the local hero, it
  stops the hero and sets the game's knock-out task `ZS_LieKnockDown` instead (Kill never runs: no game
  over message, no quest/XP side effects of a hero death).
- Player hits forwarded by the host now carry the attacker (NPC GUID, or the player id when a hero hit);
  the client judges the hit against its own hero with the attacker's weapon (`OnDamage` + `AddHitPoints`),
  so armour, stumbles, knock-outs and deaths are decided there. Protocol 15.
- Puppets take on the sender's AniState (sent with the player state, protocol 16) before replaying its
  motion: with the motion name alone the host's puppet kept standing while the client lay knocked out
  (StartPlayAni ignored the lying motions in the standing state; an earlier reading of the log that "the
  puppet played them" had counted the reported, not the played motion).
- Suite stageA: an orc on the host beats the client's hero (12 judged hits -> down at 0 hp -> up with 60);
  the host shows the client's puppet lying (its played motion is `Hero_LieKnockDown_..._Ambient_Loop`); the kill cheat on each
  machine ends in a knock-out and the hero gets up; the session stays intact. Negative control
  (`downedoff`): the hero stays dead.

Not covered yet: monsters attacking a downed hero (no monsters near the start), whether humans rob a
knocked-out hero, a revive by the partner (today the game's own timer gets the hero up), the hero's death
outside co-op (the redirect is always on while the mod runs).

## 2026-10-18: stage A2, a knocked-out player waits for a partner (owner's choice)

Static analysis (second agent, private report): the hero's knock-out loop `ZS_Unconscious_Loop` heals +10 hit
points per second and calls `ContinueRoutine` once the task time passes 5 s (the hero's routine `Rtn_Player`
then stands it up); `ZS_Unconscious_End` is only the NPCs' way out.
Changed (downed.cpp): while a partner could help (connected, same world, not down), the knocked-out hero's
task time is held at 0 and its hit points at 1; a partner within 2.5 m for 3 s stands it up
(`FullStop`, a quarter of its hit points, `ContinueRoutine`). If nobody can help (alone, everybody down,
partner gone), the game's own timer runs again. Player states carry a "down" flag (protocol 17).
Messages through `GUI2::PrintGameMessage` (hud.cpp; static analysis: type 13 is only a red text, not a game
over; ~6 s, centred): "<Name> liegt am Boden ...", "Du bist niedergeschlagen ...", "<Name> hat dir aufgeholfen."
Suite stageA2 (3/3; negative control `holdoff`: got up without help, failed as expected): the client stays
down 20 s and more with the host 10 m away; the host is told; the host next to it -> up with 50/200 hp; both
down -> the first one up by itself (released), the other waits and is helped up.

## 2026-10-18: stage B, distance warning

range.cpp: a client more than 35 m from the host gets "Zu weit vom Host: Hier wird nichts abgeglichen."
(orange), the host "<Name> ist weit entfernt (N m)."; under 25 m both get an all-clear; a reminder every
60 s while far. Distances from the player states; another world counts as far.
Suite stageB (3/3 + negative control `rangeoff`): client moved 60 m away and back.
Visual check: the harness now takes screenshots of the game windows (qa/shot.py, PrintWindow, PNG); the
orange message is readable on both screens (the first, longer client text ran across the whole screen and
was shortened). Seen in passing: in these runs a tutorial "Tipp" box stays open on screen.

## 2026-10-18: stage C, world journal for deaths

journal.cpp: every machine watches the NPCs it processes and does not mirror (its own surroundings); when one
dies there, it reports the death (GUID) to the host (protocol 18). The host records it and passes it on to the
other clients; its own deaths go to every client. A recorded death is applied on a machine only once that
machine processes the NPC, 0.7 s after it entered the processing list (the game's OnEnterProcessingRange resets
hit points first), with hit points 0 and `OnCheatKill`, the killer being that player's puppet (no experience
on the applying machine). The host never mirrors an NPC with a pending death; a client does not drive a mirror
that is dead locally but alive in the host's snapshot.
Suite stageC: (1) the client kills Zakosh 700 m from the host; the host has it pending, alive and frozen;
after the host teleports there it is dead (`ZS_Dead`). (2) The host kills a slave and leaves; the client,
arriving alone (not mirrored), finds it dead through the journal. Negative control: `journaloff` on the client.
Limits: deaths only (no items, containers, knock-outs); the journal lives in memory and is not yet saved with
the host's save (stage D); the game's own respawn of animals after days applies as usual.
- Correction (stage C, same night): applying a recorded death with the kill cheat ran the game's death script
  `Kill` on the applying machine as well: the client earned experience for the host's raiders (800 XP in
  stage 3c) and its quest manager completed the raid quest on its own (+500). The same held for mirrored deaths
  (task `ZS_RagDollDead` -> `Kill`), which had passed 3c only by timing. Both now set the death as a state:
  hit points 0, AIMode 9, then `ZS_Dead`, which with AIMode already 9 only lays the body down (static analysis).
  Quest progress, experience and plunder come only from the machine where the death happened (shared through
  story sync and the containers). Cost: mirrored NPCs no longer fall as ragdolls; they lie down at once.

## 2026-10-18: stage D1, the journal and puppets with the game's own save

persist.cpp hooks Game.dll's `gCHUDFileManager::SaveGame` and `LoadGame` (both start with 9 bytes of complete,
position-independent instructions; a 5-byte jump to the mod, the 9 bytes run from a trampoline; prologue bytes
checked first, other builds refused). Every save, from the menu as well as from QA, now removes puppets first:
before this, a save from the game's menu with a remote player present would have held a second hero and
aborted on load ("current world has more than one hero!", exploration 3). After a successful save it writes
`<save>.g3t` (the journal, one line per death); loading a save replaces the journal with that file's (none if
missing). Suite stageD1: client kills a far NPC, host saves with the puppet present, loads, the death is still
recorded and is applied when the host gets there. Negative control `persistoff`.
Not verified: that the menu's save button goes through `gCHUDFileManager::SaveGame` (assumed; the hook sits on
the function the QA save also calls).

## 2026-10-18: stage D2/D3a, each player's own hero

hero.cpp writes and reads a hero record: level, experience, learn points, attribute base values and maxima
(tags STR DEX INT ALC SMT THF HP MP SP), every inventory stack (template ID, amount, quality, equipped slot,
category; learned skills are taught again). Restore: unequip and delete the stacks (not skills), create and
equip the recorded ones, then levels, experience, learn points and attributes.
- stageD2 (host alone): record, then a sword, experience and a wound change the hero; loading the record gives
  exactly the recorded summary back (level, xp, lp, attributes, inventory hash). Negative `herorestoreoff`.
- D3a: when the host saves, it tells every client (protocol 19); each client writes its own record for that
  save name; a client loading that save gets its own record applied 3 s after the world runs.
  stageD3a: the client's hero (sword, +150 XP) differs from the host's; host saves; the client loads a copy of
  the host's save and has its own hero (identical summary), not the host's. Negative `herorestoreoff`.

Not covered yet: the save travelling to the client by itself (copied by the test), a first-time joiner without
a record (gets a copy of the host's hero), position, quick slots, active effects, political fame and the
city reputation on enclave entities.

## 2026-10-18: stage D3b, joining a campaign over the network

The host's current campaign is the save it last loaded or saved. When a client connects (only then: a save
taken later never pulls a connected player into a load), the host sends that save's three files
(`.g3savcpx` ~66 KB, `.g3savcpxdat` ~2.1 MB, `.g3t`) in 16 KB reliable ENet chunks with sizes and CRC-32s
(protocol 20). The client checks them, stores them as `G3T_host_<name>.*` (never over one of its own saves;
its hero records use the same name) and loads that save by itself; its own hero record, if any, goes on top.
First time in a campaign: a hero already in play (new game started before joining) is kept as the player's
record; from the main menu there is none and the player gets the copy of the host's hero in the save (told on
screen).
Suite stageD3b: the host takes a potion from the world and saves; the client starts and stays in the main
menu; 0.4 s after start the campaign has arrived, it loads, the client runs in G3_World_01, the potion is gone
there too, the host sees the client. Negative control `campaignoff` (host sends nothing).
Also fixed in the harness: a second game only starts after the first released its single-instance lock
(`releaselock`, as join_both does); test files of earlier runs are removed first (a stale sidecar had made
D1's negative control pass).
Repeats (same night): D3b 3/3 + negative control, D3a 3/3 + negative, D1 negative control fails as it should
after the stale-file fix; full regression of all earlier stages green on the D build.

## 2026-10-18: stage G, one time of day

clock.cpp: the host sends its game clock (World_MCP clock property set: timestamp, year, day, hour, minute)
every 5 s (protocol 22); a client more than 120 game seconds off (the clock runs at factor 12, so one update
interval is 60 game seconds) is set through the clock's own setters (`PSClock::SetYear/SetDay/SetHour/
SetMinute`). Suite stageG: the host sets its clock six hours on (12:07 -> 18:07); 12 s later the client shows
18:10 like the host (2 game seconds apart). Seen in passing: a client that started its new game later was
already corrected twice when it joined. Negative control `clockoff`.
Consequence: sleeping on a client is undone by the host's clock within 5 s (sleep is host-only for now).
Stage G results: 3/3, negative control (`clockoff`: 21599 s apart) failed as expected; sample regression of
the network-near stages (2, 3a, 3b, A2, B, C, D3b, 4, 5b, savediag) green.

## 2026-10-18: stage G2, a sleep moves time on for everyone

A client whose own clock jumps more than 1800 game seconds within one frame (a sleep, a wait) sends that time to
the host (protocol 22); the host moves its clock on if the request is later than its own (never back) and sends
it at once; the client's own corrections from the host do not count as a jump. Suite stageG2: the client's
clock jumps 12:08 -> 18:08; within 8 s the host is at 18:10 like the client (1 s apart). Negative `clockoff`.
Not covered: the real bed/sleep dialog (the jump is made with the QA clock command; static analysis says
`PlayerSleep` jumps the clock the same way), the hit point refill of sleeping (stays on the sleeper's machine).

## 2026-10-18: stage H, dialog effects on the world

Exploration (infodiag): the AI wrapper sees `IS_Running_ProcessInfo`, which runs each dialog script line on the
machine holding the dialog (its processing unit is World_MCP's). `PSInfoManager::GetCurrentCommand()` on the
world entity gives the line: command, entity 1 and 2, id 1 and 2. The client's Diego dialog in stage 4c ran
`Say` x4, `SetGameEvent Diego_WarIsLost`, `End`: the event was set on the client's hero only.
- H1: story events (the hero's PlayerMemory.PlayerKnows, set by SetGameEvent, cleared by ClearGameEvent) are a
  third story-sync domain next to quests and infos (protocol 23); cleared events travel as 0. Suite stageH1:
  set on the client -> known on the host, cleared -> gone, set on the host -> known on the client. Negative:
  story sync off.
- H2: after a dialog command that changes its first entity (SetClass, SetPAL, Set*Enabled, Set*Point, FullStop,
  SetRoutine, SetGuardStatus, MoveTo, SetRevolution/RaidEnabled, StartRevolution, Humanize, JoinNPC, JoinPlayer),
  the machine that ran it reads that entity's resulting state 0.7 s later (dialog flags, guard status, class,
  political alignment, immortality, routine name, position, enclave raid/revolution/status) and sends it
  (protocol 24); the host passes it on; every machine sets its copy (routine change restarts the routine unless
  the NPC is mirrored; position only for unmirrored copies). SetSectorStatus sends the sector's resulting status.
  Passing on the result instead of the command avoids interpreting argument formats.
  Not covered: Attack, Spawn, Give/Grant/GiveAll/Erase (inventories), OpenDoor, party membership of JoinNPC /
  JoinPlayer (only their state fields), ResetCrime, reputation.

## 2026-10-18: soak test (25 min of mixed player actions)

New suite `soak`: host and client do a seeded random mix of walking (AI goto), parting 60 m and meeting again,
hits, pick-ups, falls with help, dialog results, sleeps, saves and loads, with a health check after each action
and the working set of both processes logged.
First run: the host crashed after about a minute of actions: EXCEPTION_ACCESS_VIOLATION in Game.dll at RVA
0x117C09, `gCNavZone_PS::DetectCollisionCircleExit` called with a null nav zone (ECX=0), while the host's hero
ran an AI goto and the client's puppet walked after the client had jumped 60 m away and back. A 20-cycle repro
of exactly those two actions (`navdiag`) did not crash. Cause not proven.
Found while reading the log: the host had taken full NPC states (including positions) that the client sent
after dialog results (simulated in the soak), while the client was away with stale copies; the host then put
its own walking NPCs back to old places. Fixed: positions travel only after a MoveTo command (protocol 25), and
the enclave fields are written through gCEnclave_PS's members. (Every run since stage 1 also logs eight
"Cannot get property 'Status' in propertyset 'gCEnclave_PS'" from the game itself; not related.)
Follow-up (same night): the crash reproduces with the host alone, no client and no puppet (`gotodiag2`: the
hero sent by the game's own AI goto, `Entity::StartGoto`, to random points of a 15 m circle in Ardea). A trace of
the walker's navigation showed its zone entity (gCNavigation_PS +0xac) as `Nav_NavZone 1 Edge` just before;
`gCNavigationMap::DetectEvades` asks that entity for its nav zone and calls DetectCollisionCircleExit on the
null result. So it is not a co-op fault but a goto to arbitrary points (a QA action; the game itself sends the
hero only to valid interaction points). Whether the unmodded game crashes the same way was not tested (the bot
needs the mod). The soak's walk action now uses small direct steps instead.
Soak result after that: 22 minutes, 320 actions (102 step bursts, 40 hits, 34 loads, 28 saves, 27 pick-ups,
24 sleeps, 23 falls with help, 22 dialog results, 20 partings), no failed action, both games running and seeing
each other throughout, clean shutdowns, real saves untouched. Working set: client steady (1006 -> 1021 MB),
host 1150 -> 1220 MB after warm-up across the 34 loads (not shown to be a leak; to watch in longer sessions).
Long soak (55 min, 738 actions: 232 step bursts, 101 hits, 75 loads, 59 saves, 60 sleeps, 57 pick-ups, 55 falls
with help, 53 partings, 46 dialog results): passed, no failed action, clean shutdowns. Working set: host
1144 MB after warm-up, 1260 MB at the middle, 1246 to 1279 MB at the end (levels off); client ~1000 to 1015 MB.

## 2026-10-18: stage H3, the rest of the dialog effects; puppets could not be hit

Scan of every dialog option's script (`infoscan`): near the start Milten and Diego give items (Give), Hamlar
GiveAll, Topork challenges to a duel (`Attack npc player Duel`, quest Topork_Duell), Lester joins
(`JoinPlayer npc true`, needs the story event GetLester). `Spawn` is used by no dialog at all; `JoinNPC` neither.
- Items (Give/GiveAll/Erase): the NPC's side is mostly created on the spot; whoever holds the dialog gets the
  item, the other players do not, which is the co-op behaviour wanted. The NPC copies' inventories are not synced.
- Fights (H3b): the dialog command Attack runs the perception script `AssessTarget` (self = attacker, other =
  target, args = gEAttackReason: Duel 17, Enemy 12, ...; static analysis of Script_Game.dll). A client holding the
  dialog of a host-owned NPC sends it to the host, which calls AssessTarget against the client's puppet once the
  dialog hold ends (protocol 26). Suite stageH3b: the client holds Topork's duel option (another orc, Hamlar, cut
  in with a threat in one run: the test talks again until the partner is Topork), the host's Topork attacks.
- Found on the way: **host NPCs could never hit a client.** Topork attacked the puppet (state ZS_Attack_PowerAttack,
  target = the puppet) for 20 s without one hit: puppets were spawned with collision off (stage 2, when they were
  snapped every frame), so no weapon touched them. With collision on, 13 hits in 20 s reached the client's hero.
  Until now every host-to-client hit in the suites came from the QA command.

## 2026-10-18: stage E, regional ownership (NPCs far from the host; companions)

Every NPC is simulated on one machine, its owner; the others mirror it (protocol 27 to 29).
- E1: a client sends the NPCs it processes and nobody sends it (its own region) to the host, which passes them on;
  whoever simulated an NPC first keeps it (a client's snapshots of an NPC the host is sending are refused; the
  host stops sending an NPC it mirrors). The host drives client-owned mirrors only while it processes them.
  When the owner stops sending (it left), the mirror goes stale after 3 s and the other machine simulates it.
  Suite stageE1: the client at Zakosh owns 37 NPCs; the host arrives and mirrors 35 of them (owner 2) instead of
  running them twice, positions agree (median 0 cm, walking NPCs up to 2.6 m); the client leaves and the host
  takes all 35 back. Negative: `ownoff`.
- F/E2: one set of hit rules on every machine: a hit by a body simulated elsewhere (puppet, mirrored NPC) is
  dropped (its owner decides it); our hero's hit on an NPC simulated elsewhere goes to its owner, who applies it
  and kills (a kill pays our hero through SendKilled); a hit on another player's puppet goes to that player and is
  judged against its real hero (both directions now, relayed by the host). Suite stageE2: in the client's region
  the host's hero kills a client-owned NPC in 11 hits on the client and gets the experience; client-owned NPCs
  attack the host by themselves and their hits reach the host's hero (8 to 10 per run). Negative: host filter off.
  The kill's "killer" is the NPC itself now, not the sender's puppet: a puppet removed right after a kill (its
  player left) crashed a client in eCEntity::QueryEntityProxyInternal from the dying NPC's states.
- E3: companions. The game's follow state (ZS_FollowPlayer_Loop) follows `GetPlayer()`, the local hero. A client
  that recruits a host NPC (JoinPlayer true in its dialog) takes it over: it stops mirroring it (own AI again,
  ZS_FollowPlayer), the host stops sending it and mirrors the client's snapshots; JoinPlayer false or the player
  leaving gives it back. Suite stageE3: the client recruits Lester; the host shows him as the client's (owner 2);
  the client walks 12 m on and Lester follows on both machines. Negative: dialog results not passed on.

Not covered yet: NPC-vs-NPC fights across owners (dropped), a companion after a save/load, 3+ players.
- A puppet removed while NPCs fight it crashed a client twice in stage E2 (host leaves mid-fight; access violation in
  SharedBase under an AI state run through our wrapper, Game.dll+0x169724 in the stack, right after "puppet 1
  removed"). Fix: before destroying a puppet every NPC drops it as current target, alternative target, current/last
  attacker and spell target, and the ones that were after it restart their routine (remote.cpp Forget). Lesson for
  any co-op mod: destroying a proxy body needs the same cleanup the game does for its own entities.
- Claim rules after a static review of the E1-E3 diff (second agent): the first player to recruit an NPC keeps it,
  a second claim is refused (host -> that player, WorldState kind 6); only the claimer's snapshots of a claimed NPC
  are taken; giving it back (JoinPlayer false) drops the host's mirror at once and the client stops sending it for
  5 s; a claim without snapshots for 15 s (or whose player left before its first snapshot) ends, and a connected
  player is told (kind 6). stageE3 now also hands Lester back and checks that a silent claim is taken back.
- E4, NPC against NPC where regions meet (protocol 31): a hit by one of our NPCs on an NPC simulated elsewhere goes to
  that NPC's owner (HitNpcMsg carries the attacking NPC's GUID), who applies it with its mirror of the attacker as
  the attacker; a lethal one kills like a player's claim but pays no one (marked DefeatedByPlayer first). The hit
  path is the AI combat loop (Game.dll gCScriptProcessingUnit::sAICombatMoveItlLoop -> AddHitPoints) or touch damage
  (gCTouchDamage_PS::TriggerTarget -> gCEntity::DoDamage), both end in AddHitPoints where our filter sits (static
  analysis, second agent). Suite stageE4: the host's Lester, moved into the client's region, attacks the client's
  Zakosh (AssessTarget, reason Enemy): Zakosh 600 -> 325 hit points on the client, the host's mirror follows, and
  hits by the client's NPCs on Lester come back to the host. Negative: `npchitsoff`.
- Second static review (second agent), fixed: a pending dialog fight is dropped when anyone joined meanwhile (player
  ids are reused slots), its player left, or the world changed; a client's companion claims end with its connection
  and every claim ends with a world change or load; an NPC hit whose attacker cannot be resolved on the owner is
  dropped instead of being attributed to the victim.
- Clock race (stage G2 failed once in the final regression): a host clock update handled in the same frame as a
  client's sleep jump, before the per-frame sleep check, set the client's clock back and the sleep was lost. The
  sleep check now also runs when an update arrives, and for 5 s after asking the host to move time on the client does
  not set its clock back.
- QA lessons from the final regression: a test assumption from before regional ownership (all NPCs host-owned)
  failed stage 3a once (a scout first processed by the client stays the client's); since puppets collide, the opening
  raid hurt the client between two hero dumps (stage D3a now clears the raid first); random loot left a corpse empty
  (stage 5b now picks one that holds something); the QA teleport over 700 m once never returned in the game
  (client hang without exception, same family as the AI goto crash: QA tool, not the mod).
- Quests and companions, static findings (second agent, Ghidra): gCQuest_PS::OnNPCKilled counts kills by the victim's
  name (DeliveryEntities / DeliveryCounter / DeliveryAmounts), checks no killer and has no guard against a second
  call; JoinPlayerSlavery sets leader = GetPlayer(), Waiting = false, PartyMemberType = Slave (3); gCParty_PS is saved
  (GetSaveGameRelevance = 2). Consequences for co-op: a kill in a client's region counts only in that client's quest
  manager unless the host counts it; a companion recruited on a client is nobody's companion in the host's save.
- Third review (second agent) on the committed state, assessed for two players:
  - Companions are matched to players by name. With two players there is a single client, so a name clash cannot
    happen; a client who changes its name between save and join loses the companion (documented).
  - A pending dialog fight dropped by an unrelated join only matters with 3+ players.
  - The sidecar is read before the native load returns; if a load failed, journal and companion state could come
    from the failed save. Unlikely, left as known.
  - A host quest credit can be lost if the NPC cannot be resolved on the host when the death report arrives. All
    NPCs of the world resolve by GUID even unprocessed (journal relies on it), so this is low risk.
  - stage5b skipped its late-take check silently when no corpse with loot was left; it now fails instead.
- Stage Q found a crash: counting a client's kill for the host's quests while the host's copy of that NPC was still
  alive (the last raider of the opening raid) completed the raid quest with the NPC standing; the host crashed in
  Script.dll two seconds later (stage E5, once). Now the host counts only once the NPC is dead on its side too (an NPC
  the host does not process is counted at once, it is dead when the host gets there).
- Final review of Q/E5 (second agent), known edge cases left for after the first human test: sidecar state is
  taken before the native load returns (no rollback on a failed load); a pending quest count is not saved in the
  sidecar; a death report for an NPC the host cannot resolve is not retried; the host gives a saved companion away
  before the client confirms adoption (if adoption fails the claim times out and is offered again, 15/20 s);
  companions are matched by display name.
- 2026-10-18: claim issue published by the owner: https://github.com/cranebeaker/coop-anything/issues/16

## 2026-10-18: first human look (owner, two windows on one PC) — what the bots missed

- The host's enemies never chose the client's puppet as a target (0 of ~40 target choices measured afterwards with
  a per-NPC target dump); the client could not be hurt by them. Every earlier "NPC hits a client" test had forced
  the attack (QA aggro, dialog duel). Fix under test: the puppet joins the local hero's party (as companions do);
  enemies then target it and hit the client.
- The client saw no fights: mirrored NPCs walked unarmed, without fighting stance, and their attack motions did not
  show. Mirrors' inventories are empty (their own AI, which fills an NPC's equipment, never ran; AssureAll froze the
  client), so held weapons must be created and put in hand (HoldStacks) from the owner's snapshot; stance (AniState)
  travels too. Weapons now match; attack motions still mostly fall back to the stance's ambient loop (open).
- Lesson for the playbook: bot QA must also check what the other player sees (animation, held items, stance) and
  unscripted AI behaviour (does an enemy choose a remote player by itself), not only replicated data.

## 2026-10-18: why enemies ignored the remote player, and the fix (stage V1)

Static analysis (second agent, Ghidra, Script_Game.dll; details private in the workspace):
- NPCs pick targets in a scanner called from `AssessActivePerception` (RVA 0x295F0 / 0x92F0). It walks
  `Entity::GetNPCs()`, asks the script `GetAttitude(self, other)` **by name** for each NPC and keeps the nearest
  entity per attitude (Hostile/Predator are attacked via `AssessTarget`, Angry threatened, Panic fled from).
- The player is special only by identity with `Entity::GetPlayer()`: a fixed slot (attacked when `IsUpset`) and
  its own attitude branch (`GetAttitudeToPlayer`: crimes and fame per faction). A second `PC_Hero` NPC falls back
  to political alignment and species tables. For orcs vs. a human with alignment None both give 0: never a target.
- Party membership (leader = player) has no branch in `GetAttitude`; companions are judged by their own faction.

Observed (QA line `NEARX end`): the puppet is in `GetNPCs()`, alignment 0, species 0 (human), AI mode 10.
Fix: wrap the script `GetAttitude` (swap `gSScript::m_funcScript`); when `other` is a puppet, call the original
with the local hero instead. The puppet no longer joins the hero's party (it put it into the follower list).
Result: with the wrapper orcs chose the puppet in every run; with it switched off (`attitudeoff`) in none.
New problem: an orc that targets an unarmed puppet often only turns and walks (`ZS_Attack_TurnLeft/Right`,
`Goto`, `GoBack`) and never strikes (1 of 7 runs with a hit after placing the client next to a free orc); with
the client holding a sword 2 of 3 runs had hits. Direct (not by-name) calls of `GetAttitude` (29 functions,
e.g. in `OnAttack`) still see attitude 0 for the puppet; whether that or the target's stance causes the waiting
is under investigation.

## 2026-10-18: attack motions on mirrors and puppets (stage V2): the motion owner

Static (Ghidra): the game's combat moves (`gCScriptProcessingUnit::sAICombatMoveStart`, Game.dll RVA 0x16ABB0)
play on motion slot 0 and then call `eCVisualAnimation_PS::SetMotionOwner(0, 5)`; script animations
(`sAIPlayAniStart`) set owner 4; `Entity::StartPlayAni` (Script.dll RVA 0x166F0), which the mod used to replay a
received motion name, sets owner 6. The owner enum is not named in the SDK.
Observed (stageV2, client counters, two runs each): after `StartPlayAni` the motion is on slot 0 at once in
99.7 % of starts, so the name is found. 150 ms later it still shows in 11-12 % of starts with owner 6, and in
93-94 % after setting owner 5 right after `StartPlayAni` (716/764, 849/916). So something outside the AI states
replaces slot-0 motions whose owner is not "combat" with the stance's ambient loop; owner 5 keeps them.
Follow-up (same evening): the waiting was the test's fault. The suite had put the client 2.5 m beside a free orc
at a fixed offset, which could be inside a wall or on a roof; the orc then only walked and turned. Placed 1.5 m
from the orc on the line towards the host's hero (walkable ground), the orc struck the unarmed puppet in 4 of 4
runs (HackAttack, PowerAttack, QuickAttack) and the hits reached the client; negative control (`attitudeoff`)
0 targets. The wrapper now jumps from the native `GetAttitude` (Script_Game.dll RVA 0xE3B0, 6-byte prologue
`55 8B EC 83 E4 F8`), so the 29 direct callers see the puppet as the hero too; the script-table entry points to
that same function. Side effects on those callers (damage rules, comments, prices) are being reviewed statically.
Regression after the fix (32 suites): three checks assumed enemies ignored the client and were adjusted (count only
the client's swings that land; pick an orc the machine simulates itself; ignore the bare-hand item `Fist`, which
an unarmed NPC holds and which is never plunder). All 32 green afterwards. Package G3Together-2d73cde.

## 2026-10-18 (late): owner's second look — T-pose, and attacks done the game's way

- The owner saw T-posing orcs on the client. The earlier V2 fix (slot-0 owner 5 after `StartPlayAni`) kept the
  motion name on slot 0 while nothing ran: with `IsMotionRunning(0)` as the measure, ~80 % of the client's mirrors
  had no motion running (host 1-6 %). Lesson: measure whether a motion runs, not which name is on the slot.
- No owner value works by itself (1, 3, 4: 0 of ~130 host attacks running on the client; 5 freezes): owner 5
  motions are advanced by the SPU's combat-move loop (`sAICombatMoveItlLoop`), which nothing drives for a
  `StartPlayAni`.
- Fix: replay attacks, parades and staggers as the game's own combat move: `gCScriptProcessingUnit::
  sAICombatMoveInstr(new sAICombatMoveInstr_Args(self, nullptr, action, phase), &routine->GetSPU(), false)` on
  the mirror, action and phase parsed from the motion name (`Orc_Stand_None_2H_P0_PowerAttack_Hit_...`), no
  target. A separate `Recover` is left to the running move (starting one faulted now and then). Result: client
  mirrors without a running motion 1-4 %, 37-55 % of the host's attacks seen running on the client (was 0).
- The players' own swings (puppets) the same way: with the raid cleared, 14/15 and 15/15 swings seen running on
  the other machine, 0/15 without. Bot swing: set the hero's task `PS_Melee` and state `PS_Melee_Attack`, as
  `OnPlayerAction` does for a key press.

## Bows and spells, ranged QA tools checked (agent B, 2026-10-19, pair 2, run 20261019-181053-together-p2)

Observed (`rangeddiag`, branch agent-b at 9deb53d, both directions):
- `inventory`, `ready`, `aimat` (Target), `mana`, `flying` answer as intended: bow plus arrows held, spell
  `It_Spell_Meteor` held (use 15), body turned to the target at 12 m, focus set, mana 200/200.
- `act PS_Ranged-PS_Ranged_Aim` / `PS_Ranged_Shoot`: the task and state are set, and one arrow per shot appears on
  the shooter (projectiles 1, 2, 3; quick bar arrows 50 to 47). The target is never hurt (200/200 on all
  machines, both directions), no experience. The watcher never has a projectile (0 while the shooter has 3).
- `act PS_Magic-PS_Magic_Loop` then `PS_Magic_Cast`: state set, but no meteor (`Smn_*` 0), no new projectile,
  mana unchanged at 200: the spell was not cast.
- Screenshots: the tutorial hint box "Tipp: Die Orks wurden besiegt" is open on both machines during the casts;
  on the client the hero still visibly holds the sword after `ready It_Spell_Meteor`; the host's camera faces
  the sky.

Derived, not tested: the player path aims with the camera, not with the body or focus, so arrows fly where the
camera looks (explains the misses). Whether the open hint box or the missing cast animation blocks the cast:
unknown. The state names used exist in Script_Game (`PS_Ranged_*`, `PS_Magic_*`), and so do `_AI_Shoot`,
`_AI_Cast`, `_AI_Cast_Begin`, `_AI_Cast_Execute`, `_AI_StartCastPhase`, `MagicProjectile`, `MagicMeteor`,
`MagicSummon*`: next step is the NPC path.

## Bows and spells, step 2: shooting and casting by the game's scripts (agent B, 2026-10-19, pair 2)

Static (Script_Game, details in the private findings): the player states and the NPC AI end in the same code.
`_AI_StartCastPhase` phase 5 runs the spell's `FuncOnCast`, else by `SpellType` `MagicProjectile`/`MagicInstant`/
`MagicSummon`, with self = caster, other = target, int = inventory stack; mana is paid in phase 2. The arrow is shot
by the script `Shoot` (PS_Ranged.cpp, also called by `ZS_Attack_Loop`): it spawns a new arrow at the hand, and only
if the shooter is `Entity::GetPlayer()` aims it at the camera's screen centre (physics path); for anyone else it
flies in a line with `SetTarget(other)`. So a puppet replaying `Shoot` takes the NPC branch by itself.

Observed (suite `rangedbot`, runs 20261019-182204 to -190738-together-p2, both directions):
- Meteor (`castat`: spell and target on the caster, minimum mana, then FuncOnCast): `Spl_Meteor`, FuncOnCast
  `MagicSummon`, spawn `Smn_Meteor`, 130 mana. In every run, both directions: target killed on the host (its
  owner), the caster got +50 XP, also the client's meteor on a host-owned villager. The watcher never has the
  `Smn_Meteor` entity (0 while the caster has 1).
- Fireball (`MagicProjectile`, `Mis_Fireball`, 15 mana): host's killed the target; client's did not hurt it.
- Bow (`shootat` = script `Shoot`, then the arrow re-aimed, left alone or replaced): reload and quiver work
  (50, 49, 48). The arrow usually stops within 1 to 2 m of the hand, even left on the camera's aim. Hits seen only
  from the host (3 hits and a kill in one run, a kill in another). Switching the held bow's collision off changed
  nothing. Cause: unknown.
- Side finding: on the client, orcs the host had killed were seen standing upright with weapons in two screenshots
  (NPC list on both machines: dead, `ZS_Dead`/`ZS_RagDollDead`).

## P1 summons, the meteor first (agent B, 2026-10-19, pair 2, commit 3b91686, protocol 36)

Observed: `Smn_Meteor` is a short-lived entity (about 1 s) spawned by `MagicSummon` at the target's pose, property
sets ScriptRoutine, Interaction, Damage, Effect2; its Interaction owner is the caster. Its hits reach AddHitPoints
with the meteor as source (other), so the combat filter's attacker (the source's owner) is the caster.
Approach: the caster's machine wraps `MagicSummon`, finds the new summon a frame later (template name, owner =
caster), gives it a net ID (caster id << 24 | counter) and announces it (spell item, template, pose, target GUID);
the end is announced too. The other machine replays `MagicSummon` on the caster's puppet holding the same spell, so
the copy belongs to the puppet and its hits are dropped by the filter's rule 1 (hits by puppets). Without the
target there, the template is spawned at the caster's pose. A copy is removed 1.5 s after the caster's ended.
Suite `stageP1` (client casts, then host, at a villager the host simulates): 3x green (DLL 37bbc731...): copy bound
on the watcher, owned by the puppet, none of its hits applied (seen blocked on slaves, Milten, Hamlar, the orc boss),
the target lost hit points once on the host, a kill gave the caster +50 XP. Negative control `summoncopyhero` (copy
owned by the watcher's own hero) red: the copy killed the target on the host a second time (delta -2548 applied).
stage2, stage3a, stageA, stageT green on the same DLL.
Not covered yet: long-lived summons (skeleton, golem, demon: NPCs that need movement mirroring), late join or a
watcher loading the area after the cast, save/load while a summon exists.

## 2026-10-19: agent A, merges and regressions (summary)

### Observed

Bows, spells and P1 (agent B) are in the sections above.

- V5 (commit `7ed76c4`): `GetAttitude` answers Panic when self flees and its target is
  other. The wrapper had replaced the puppet before that check, so a fleeing NPC judged it
  hostile. Now left alone; `stageV5` 3/3, negative control `fleekeepoff` failed.
- Mirror motions (commit `6a768af`, diagnostic `syncdiag`): gestures and knock-downs
  started with `StartPlayAni` were replaced by the stance within 150 ms (4 of 91 and 4 of
  77 still showing). As play-ani instructions on the body's script unit: 98 of 98 and 54
  of 55. Village mirrors with nothing running: 1 to 5 of 80 to 100 (before 19 to 26).
- A2 redone to the owner's rules (commit `b2d6c54`): help costs one healing potion; the
  only health potion template found at runtime is `It_Potion_Health`; nobody able to help
  for 5 s means death as in single player. `stageA2` 3/3, negative control `potionoff`
  failed. In the second run a hit replaced the death task; it is now set again.
- Full regression (series `20261019a`, 37 suites): 36 passed. `stage2` failed once: the
  client's puppet ran `ZS_HangAround` on the host for about 2 s and turned after
  staggering. Seen in 1 of 15 runs on that DLL, never in 80 older runs.
- Transformations (commit `bce0585`): after `StartTransform`, `Entity::GetPlayer()` is the
  creature (`Wolf`, `Bloodfly`) while `gCSession::GetHero()` stays the hidden human body;
  the mod had sent the human. `stageT` 3/3 (Wolf twice, Bloodfly once), negative control
  `shapeoff` failed.
- Two agents in parallel (commits `02d3c1c`, `c5c84cb`, `9deb53d`): a second pair of test
  copies with port 34568, its own QA lock and run folder; each agent in its own git
  worktree and branch. The second pair's games exited with code 0 at start because a game
  of the other pair held `GenomeMutex`. Fix: every game releases its handle as soon as
  its bot answers, and an early exit is retried. Parallel probe: 5 of 6 green; the red one
  was the known `stage2` puppet turn.
- Invariant monitors (commit `05444a6`): report-only checks at the end of every
  two-player suite. Self-test `invariantself` reports `double_sim` when the client stops
  taking snapshots (`20261019-175827-together`, `20261019-194507-together`); negative
  control reports nothing.
- Full regression with monitors (runs `20261019-180209-together` to
  `20261019-191307-together`, 38 suites): 37 passed; monitors found no lasting violation.
  `stage3c` failed on a test assumption from before stage E (a client may own raiders and
  complete the raid quest itself); test adjusted, then passed, negative control failed.
- P3a (commit `f6b0510`): a kill by a player's companion or summon across machines pays
  that player (Lester kills the fisher, client 500 to 550 XP). `stageP3a` 3/3, negative
  control `partyxpoff` failed.
- Stage L (commit `f6b0510`): a key puts the hero 1.5 m behind the partner; 3/3, negative
  control failed. Owner's decision: a soft leash instead of a hard distance limit.
- Pre-package regression (series `20261019-pre-package`): 8 of 9. `stage3b` failed in
  `20261019-195533-together`: the orc parried the test hit (`ZS_ParadeStumble`, 0
  damage). The test now swings up to 3 times; rerun passed, negative control failed.
- Runtime trace (commit `7bc88a1`, branch of agent A, not in `master` at the time of
  writing): hooks the engine's motion start and the game's task setters, logs body,
  motion or task and caller as module+RVA. `tracediag` 3/3. Most mirror motions come from
  the game's locomotion code; tasks on mirrors come through `AISetTask`.
- `master` = `dff5cfa` (P5 monitors, L, P3a, P1). `stageP1`, `stageP3a`, `stageL` passed
  on the merged DLL.

### Derived

- When a test fails on game behaviour (an NPC parries), repeat the action as a player
  would; do not loosen the assert.
- Static reading cannot say which caller sets a task on a mirror at runtime; a trace of
  the setters with caller addresses can.

### Unknown

- Cause of the rare puppet `ZS_HangAround` turn.
- Why host-killed orcs sometimes stand upright on the client.
- Why bot arrows stop near the hand.
- Result of the full regression of `master` `dff5cfa` on pair 2 (started at the time of
  writing).
- Whether the parallel load makes the puppet turn more frequent.

## 2026-10-19 late: runtime trace and the standing dead orcs (agent A)

Observed:
- A runtime trace (G3 Together `trace on`, file `g3trace.log`) hooks Engine `eCVisualAnimation_PS::PlayMotion`
  (RVA 0x30860) and Game `gCScriptRoutine_PS::SetCurrentTask` (0x66100) / `AISetTask` (0x164350) after checking their
  prologues, and logs body, motion or task and the caller as module+RVA. Suite `tracediag` 3/3, negative control (no
  file without the command) correct.
- Host, `ZS_Dead`: plays `<skeleton>_Stand_None_None_P0_LieDead_Begin_N_Back...` (caller Game.dll+0x16b485), then
  `<skeleton>_LieDead_None_None_P0_Ambient_Loop_N_Back...` about every 400 ms (Game.dll+0x16a90c).
- Client mirror, `ZS_Dead` after setting AIMode 9 first: no motion at all; locomotion slots 1-3 kept playing walk and
  stand motions on the corpse. 0 of 4 lying in the negative control.
- Fix (lay the mirror down: stop movement and slots 1-3, play Begin, then the loop in AniState 19 as a play-ani
  instruction): suite `deaddiag` 3/3, 12 of 12 lying; regression of the merged state 15/15.

Derived:
- Bodies off screen are not animated: their bone matrices keep an old pose, so bone height is only proof for bodies in
  view.

Unknown:
- Whether other species (animals, humans) die the same way on the client; only orcs of the start village were tested.

## P2 arrows, first version (agent B, 2026-10-20, pair 2, commit ce6c5c7 on agent-b, protocol 37)

Static (Script_Game and Game.dll, details in the private findings): the player's PS_Ranged calls the script `Shoot`
directly, not through the script table, so it is hooked in place (address taken from the table entry, prologue
checked first). For the player `Shoot` sets ShootVelocity = the held arrow's DamageHitMultiplier x 200 and aims at
`PSCamera::GetScreenCenterPickPoint()`. It turns collision off only with the shooter and one more entity, not with the
hero's body-part entities. `gCProjectile_PS::OnTouch` disarms a projectile at its first touch (collision group 6).

Approach: the shooter's machine announces its own arrow a frame after it shows up (template, pose, TargetDirection,
ShootVelocity, path style, touch behaviour, damage, target). The watcher spawns a copy from the template that belongs
to the shooter's puppet and shoots it the same way. The combat filter's rule 1 then drops its hits, as with the
meteor copies.

Observed (suite `stageP2`, runs 20261020-083728 to -091232-together-p2):
- Every arrow the shooter's game spawned was announced and copied (6 of 6 per green run). Copies are owned by the
  puppet. Copies flew like the arrow: 0 to 16 degrees apart, similar distance, except where the watcher's world
  differed (the client still showed some of the host's dead orcs standing; `ce6c5c7` does not contain agent A's
  dead-mirror fix `9acdaf5`, checked with `git merge-base --is-ancestor` on 2026-10-21).
- Negative control `shotmirroroff`: no copies, red as intended.
- 1 of 3 runs green. In the red runs the shooter's own arrow failed:
  - The bot shoots from the idle stance, where the new arrow starts inside `Body_Player`/`Head_Player` and stops at
    once. Drawing first (state `PS_Ranged_Aim`, 2 s) mostly avoids that and lets the game set the draw (multiplier
    0.74).
  - In one run the QA scan reported `spawned=0`: it found no new arrow of the shooter within 2 s. That is the scan's
    count, not the return value of `Entity::Spawn` (see experiment 0 and agent C's arrow note below).
- No arrow (the shooter's or a copy) ever hurt an NPC. Arrows on the physics path passed within 10 cm of a target's
  chest and flew on, without a call to AddHitPoints. Cause: unknown.

Derived: the idle-stance failures are the test shooter's. For the `spawned=0` run it is unknown whether the arrow
existed; the mirroring announces only arrows it finds, so that run does not separate the shooter from the mirroring.

Not covered yet: exactly-once in a run (needs an arrow that hits); crossbows; projectile spells (MagicProjectile);
late join.

## 2026-10-20: the rare puppet turn (agent A)

Observed:
- Trace with call chains (frame-pointer walk from the `AISetTask` hook) and a lookup of the script admin tables by
  address: after a hit reaction (`ZS_Stumble`, `ZS_Slide`) the game runs the script `ContinueRoutine`
  (Script_Game.dll RVA 0xa5ea0) for the puppet; its `SelectRoutineTask` (0xa9440: arena, `ZS_FollowPlayer` or the
  routine) set `ZS_HangAround` every frame for about 3 s and the puppet turned on the spot.
- Wrapping the `ContinueRoutine` entry of the script table and not running it for puppets (commit `a4da3aa`, DLL
  `b0ec5c94…`): `ZS_HangAround` on puppets at least once in 63 of 63 traced runs before, 0 of 20 after; `stage2` 19 of
  20 (the red one is a different case, still open: a client running under orc hits, the puppet one to two metres
  behind).
- The 63-run count does not separate the short `ZS_HangAround` at puppet creation (an earlier trace that morning saw
  it replaced after 16 to 350 ms) from the 3-s chain above. The visible turn was rarer: that day's `stage2` series
  before the fix had 2 of 10, 1 of 15, 3 of 15 and 4 of 20 red runs, and not all of them were the turn (test errors,
  below, and one run with the client under orc attack).
- Two earlier "puppet does not reach the step" failures were test errors: orc hits pushed the client's hero 1.3 m on,
  the puppet followed it exactly.
- Cutting the stumble motion off on puppets made the game restart it every frame (up to 257 times); reverted, the
  stumble is left as it is (cosmetic, direction may differ).

Derived:
- Hit reactions on a puppet are judged by the host's own damage script (Script_Game.dll RVA 0xb4f50, call chain from
  Game.dll+0x16093b), which also starts a stumble there.

Unknown:
- Whether `ContinueRoutine` is also reached without the script table. Direct native callers were not checked, and
  the wrapper covers only table calls (as with `Shoot`, which had to be hooked in place).

## Why the bot's arrows never hurt NPCs: experiment 0 (agent B, 2026-10-20, pair 2, run 20261020-122311-together-p2)

A QA hook on `gCProjectile_PS::OnTouch` (exported by Game.dll) logs every touch. The property set the handler checks
before calling the arrow's `FuncOnTargetHit` is `eEPropertySetType_NPC` (30): a hit means "the touched entity is a
figure". The contact type is the property set that produced the contact: 14 = CollisionShape, 100 = Animation
(bone hit zones).
Observed:
- The host's arrows (4 shots at a villager 5 m away) touched a treasure chest (`G3_Object_Interact_Animated_Chest_01`),
  unnamed geometry and then a house wall, and never reached the target. The test's spot finder only avoided figures,
  not objects.
- The client's arrows touched the shooter's own sword and then the shooter's own spine
  (`PC_Hero type=100 bone=Hero_Spine_Spine_1`) and stuck there. In this run that went with `spawned=0` readings: a
  stuck arrow is moved under what it hit and drops out of `Entity::GetEntities`, so the QA scan does not find it.

Derived:
- Bone hit zones do register arrow contacts. `Shoot` turns off collision with the shooter's shape only, not with its
  bone hit zones. A bot shooting from the idle stance can therefore hit itself.
- The NPC-side hypotheses (tunnelling, inactive hit zones on passive NPCs) are not needed to explain the misses in
  this run. They are not ruled out for the arrows that passed within 10 cm of a target and flew on (P2 arrows above).

Unknown:
- Whether the self-hit hurts the shooter; why the client's line from hand to target crossed its own back.

## P2 spell projectiles, the fireball first (agent B, 2026-10-20, pair 2, commit ba72045 on agent-b, protocol 38)

`MagicProjectile` is called by name (`_AI_StartCastPhase`), so its script-table entry is wrapped. The caster's
projectile (`Mis_Fireball`, line path with the target set, speed 100) is announced like an arrow, with the item cast
from. The copy gets that item's spell, as the original does from `MagicProjectile`; what it hits may read the spell.
Observed (suite `stageP2fire`, 3 green runs): host and client fireballs reached Hamlar at the first try (they home).
The copy touched the same figure on the other machine. The host applied each fireball once: the host's directly, the
client's as a claim. Negative control `shotcopyhero` (copy owned by the watcher's hero) red, the copy's hit counted
too.

Arrows in comparison (suite `stageP2hit`, target Hamlar, commit `875d676` on agent-b, DLL `f3c6e4f7…`): in every run
where an arrow reached the target, the host applied it exactly once (the host's arrow directly, the client's as a
claim); the copy touched the same spot and did not count. 2 of 4 runs green: one run had no host arrow reach the
target in 8 tries, one was aborted (the host hung after a teleport, no crash report). Negative control `shotcopyhero`
red (double hit). A later rerun was green, 3 of 5 in all: not yet three green runs in a row. Crossbow bolts did not
reach the target in the village, so exactly-once is not shown for bolts.

## 2026-10-20: static NPC spells, world callbacks and crash signatures (agent C)

Static only, mod source baseline `d4dc47e`; no game processes, installations or runtime tests.
Evidence was read from isolated Ghidra project copies and existing QA logs. Detailed decompilation and
machine-specific logs remain private. Source anchors below are DLL RVAs or mod source paths.

Observed in code:
- `NpcRecord` has HP/pose/animation/equipment, not NPC status clocks or party type. AI state filtering
  (`src/ai.cpp`) does not generally stop normal scripts such as OnTick/OnEnterProcessingRange.
- `MagicHypnosis` (`Script_Game+0x543b0`) starts `ZS_MagicSleep`, whose NPC loop exits above TaskTime30;
  the separately registered Hypnosis loop has a different limit and is not selected by that cast.
- `CanControl` (`+0x549a0`) requires caster == local GetPlayer. Control/taming use helper `+0x54770`
  to dismiss Controlled party members and kill Summoned members before adding the new Controlled member.
  `MagicCallAnimals` (`+0x54160`) redirects existing animals; it does not spawn followers.
- Poison sets bit4 and a world-clock timestamp, then OnTick supplies HP loss. Disease sets bit8 and
  stamina0. Status replication must account for active callbacks, not just visuals.
- Mist uses one global bit64 per NPC; a new mist retires existing mists, and ending/processing exit
  clears status broadly. Independent full simulations of visual copies would not preserve this lifecycle.
- Theft/crime callbacks (`+0x60930`, `+0x60670`) check the local player explicitly. Enclave transitions
  (`+0x602d0`) change crime/fame/member fields in addition to status. Native EnterArea quest handling
  (`Game+0x158e20`) increments DeliveryCounter, which the existing quest-status sync does not carry.
- Natural respawn (`Script_Game+0xac3c0`) depends on calendar age, random selection and local player/
  neighbour distances. A death-only journal has no explicit generation for a new life of the same GUID.

Corrections to old crash hypotheses:
- `Script.dll+0x13EA3` (inside `Entity::AttachTo`) is `mov edx,[eax+0xA0]`, after reading the passed entity's vtable. The passed entity
  is nonnull and already passed a null check; ECX0 in the exception context is not the failed dereference.
  Five archived instances have invalid vtable values. The outer caller/lifetime violation remains unknown.
- `Game.dll+0x2BD1F3` is inside the data string `CurrentAttackerEntity` in `.rdata`, not a teleport function.
  It was the faulting EIP of a host crash (2026-10-20 13:10, pair 2, once, right after a test-bot teleport, which is
  where the teleport reading came from); ESP `0x0019E849` was not 4-byte aligned and EBP was `0xAA101779`.
  A corrupted return/function pointer or earlier stack corruption is a candidate, not a proven cause.
- The diagnostic stack scan yields module-address candidates, not an unwound call chain; record stack
  offsets, raw exception access kind (including execute) and relevant input pointers before inferring a cause.

Derived next steps: route spell intent to each NPC's simulation owner with real player identity and spell
context; apply results once; preserve status/party/lifecycle state through ownership transfer and loading.
Test host/client/simultaneous casts, ineligible targets, duplicate intents, expiry and re-entry. Replaying
an original single-player script with a puppet is not automatically equivalent to the real caster.
These are implementation/test proposals, not new claims of working co-op features.

Cross-check: OnTick calls AddHitPoints directly (`Script_Game+0xb11ab → +0x35b50`), bypassing
the mod's script-table-only damage wrapper. Mirrored poison/burning therefore needs its own tick gate
or a visual-only representation; the current damage filter is not a general safety net for status effects.
Afterwards (commit `2a9e983`, 2026-10-20 21:47, per its message): poison and disease on another machine's NPC are
cast on its owner, and OnTick is skipped on mirrors and puppets. Not re-tested in this write-up.

## Agent C second static assignment (2026-10-20, request labelled 18:00)

Private evidence is in `g3-workspace/re/findings/*-c.md`; no game was started, installed or stopped,
and no mod source was changed. These are static findings and interpretations of archived runs.

Observed (code and archived runs):
- Entity GUID reassignment is supported by the exported `eCNode::SetID` (Engine.dll RVA 0x20DAB0).
  It removes the old SceneAdmin map key, assigns the new 20-byte `bCPropertyID` by value and
  registers the entity again. Existing keys are overwritten without an identity check.
  Existing proxies retain their stored IDs. See `entity-guids-c.md` for ABI, raw prolog bytes,
  save implications and a direct-call proposal.
- Hailstorm and Meteor both reach native `DoLogicalDamage` before `AddHitPoints`. Hailstorm chooses
  targets independently in each simulated cloud. In the failed run `20261020-165728-together-p2`,
  the observer's cloud judged a low-HP target while its HP delta was rejected, then the target died
  without the remote claim/XP completion. `AssessHit` can schedule death before the filter;
  RagdollDead writes HP zero directly.
- Normal orc death leaves a corpse; it does not immediately delete the entity. Script Entity wrappers
  copy raw entity/property-set pointers, unlike native proxies. QA GameHit calls OnDamage directly
  into AssessHit and does not update LastInflictor through DoLogicalDamage.
- ArmyOfDarkness has 6/8/10 spawn attempts (equal Skeleton/SlaveZombie groups at INT thresholds
  250 and 300), after clearing the previous summoned group. OnTick kills nonwaiting Summoned
  creatures farther than 4000 world units from the local player. Native party data is save-relevant.
- OnHit uses attacker Self and CurrentTarget; OnDamage uses victim Self and damage-source Other.
  Reset directly sets HP/SP/MP to maximum. Ghost combines local input, camera, focus and movement.
  DuskToDawn changes the clock factor to 3840, console time scale and finally resets player values.
- The 21 AI names that the runtime inventory listed without an original address (444 state names,
  423 with an RVA) are resolved: 19 function definitions in the private Script_Game project, two in
  Script_paru. Original registration pointers and real bodies are recorded in
  `missing-ai-complete-c.tsv` and `missing-ai-c.md`. Hold/Use/Equip functions can create, split or
  delete inventory stacks; SayBossNews/Bribe can change crime/combat state. LieKnockDown itself
  only advances the coroutine.
- Personal hero restoration currently preserves existing Skill stacks and teaches missing ones;
  it forms a union with skills already present in the loaded campaign rather than an exact
  replacement. The record does not fully encode learned/activation state. See `xp-attributes-c.md`
  for the complete kill/quest/dialog/teacher/companion/summon/consumable/perk path comparison.

Derived:
- Rekey an entity before binding party/target/owner references to it. The mod's `missing` mirror
  cache needs an explicit retry after a delayed spawn.
- The Hailstorm run strongly supports a pre-filter authority defect, not a demonstrated owner-ID
  loss: a filter in `AddHitPoints` alone does not stop every death.
- Later ragdoll code can inspect an older inflictor after a QA GameHit.
- The mod's slavery adoption path is not a faithful restore of Summoned type 5. Ordinary clock
  snapshots do not represent the whole DuskToDawn transaction.
- A joining client loads the host's save, whose hero is the host's, so learned skills of the host's
  hero stay on the client's hero after the restore (static; not reproduced at runtime).

Unknown:
- The exact death attribution in the Hailstorm run: the run has no Task trace and had active orcs.
- The actual caller of the old AttachTo fault (`crash-lifetimes-c.md` lists ring-buffer/AV
  measurement points). None of the 21 AI entries is newly runtime-tested.

## Agent C: actor identity and social world reactions (2026-10-20)

Static follow-up, mod baseline `d4dc47e`. Additions on that baseline that C checked as already
implemented (not written up elsewhere in these notes; summaries from their commit messages):
A's F1 (`58b3a14`, NPCs react to the client's hits through AssessTarget with ReactToDamage) and
W1 (`96fd5d0`, doors and locks are the same for every player, world state kind 10), B's Charm
(`fae2751`, Charm on another machine's NPC runs there) and area-spell copy fix `5628884`
(`MagicAreaDamageTick` hooked in place, skipped for copies owned by a puppet). No game or runtime
test was started by C. Detailed private findings: `re/findings/social-identity-c.md`,
`social-crime-identity-c.md`, `social-player-context-c.md`, `social-mod-gaps-c.md`.
The concrete A/B work plan is `social-coop-auftraege-c.md`. Addresses are Script_Game.dll RVAs
unless a module is named.

Observed (code):
- SendPerceptionTakeItem (0x32840) calls CanReceivePerceptionFrom and AssessTakeItem directly,
  so a script-table-only perception wrapper does not cover those calls. The item is read from the
  actor's current target. Counting theft and deciding witnesses are separate paths.
- AssessPlayerWeapon (0x2F780) requires the local GetPlayer; AssessPlayerZone (0x2FCA0) uses
  `Entity::IsPlayer` (Script.dll RVA 0x14900), a pointer comparison against a cached player entity.
  AssessPlayerSneaking (0x2F5B0) has no direct player guard, but normal active perception invokes
  it from the local-player slot.
- ZS_ReactToZone_Loop (0x224C0) fetches GetPlayer again after its state timer and uses it for
  distance/Goto. ZS_GotoPlayerZone (0x30280) also fetches the local player's zone late.
- `gCNPC_PS::WriteSaveGame`/`ReadSaveGame` (Game.dll RVAs 0x144E90/0x144D00) explicitly carry
  player-related crime/fight memory and target proxies. `gCPlayerMemory_PS::WriteSaveGame`/
  `ReadSaveGame` (Game.dll RVAs 0x14E740/0x14E630) carry the four political arrays.
- `_AI_SayGuardNews` (0x3A3C0) sets/clears DIA_BRIBE, DIA_EXCUSE and DIA_PROVOKE on PlayerMemory;
  story.cpp currently shares PlayerKnows as world events. Guard severity also uses process-global
  values.
- talk.cpp holds are not full exclusive reservations: repeated Begin overwrites the holder, End
  does not verify holder identity, and host-local dialogs are not integrated.

Derived:
- Mapping a remote body to the local hero inside GetAttitude gives a shared attitude answer, but
  not a persistent actor context for crime, dialog or later AI states.
- Preserve the item identity before removal or focus changes and bind crime effects to the
  accepted inventory transaction. Do not generalize one player guard across all scripts.
- Correcting only an entry argument would leave the delayed identity dependencies above.
- Native save support is not live replication or a per-player ledger.
- Wrong dialog options for an uninvolved player are a runtime test candidate, not a reproduced
  failure. Do not blacklist all DIA_* blindly.
- Session identity, persistent participant identity, and the current visible body need distinct
  treatment across disconnect, transform, save and owner handoff.

The proposed first vertical test is a client taking owned property while the innocent
host stands nearby, followed by a correctly targeted warning and rejoin. Further work
covers complete combat feedback, dialog/quest conflicts, interaction cancellation,
world re-entry and coordinated time. All runtime acceptance remains with A/B on their
own two-player pairs; these are findings and test instructions, not new tested features.

## Agent C: outside review of cross-system transitions (2026-10-20)

An owner-authorized Fable 5.1 consultation reviewed abstract assumptions, then a
bounded set of versioned mod excerpts (A d2e331f8, B 760af83f). The completed code
review suggested testing rejected item transfers with lasting native side effects,
world/personal-save consistency, and reactions interrupted by puppet replacement.
These are hypotheses and proposed tests, not new runtime results.

Local static checks constrain those suggestions:

- The host's native world save completes before its Campaign notification makes
  the client write the personal hero record. Container changes are observed on a
  polling interval, so an empty pending set alone does not prove all local changes
  have reached the host. Test the reachable save/transfer order; do not claim an
  observed duplication from this ordering alone.
- Container replies and Campaign notifications use the same reliable channel.
  Do not invent arbitrary reordering of those messages. Existing container
  compensation must be included in the experiment.
- The transferred campaign's local name and hero-record prefix are consistent.
  The review's uncertainty about that naming is resolved by the complete local path.
- ItemTook is sent after the local world entity disappears. A proposed reverse
  loss case cannot be inferred merely from a multi-frame take animation.
- Two native quest completions can correctly award two eligible players. Count
  authorized effects per recipient and follow their cause through replication.
- Replaying a result can itself trigger native logic. Hash equality only covers
  selected fields at a comparable version. Neither proves semantic correctness.

Keep the existing distributed simulation and per-domain mechanisms unless an
experiment establishes a concrete reason to change them. Subsequent reviews should
carry the last answer, its limitations, the exact source baseline, and new evidence.

## Agent C: poison and disease implementation handoff (2026-10-20)

Static follow-up against B f1ad45e3, protocol 40; no new runtime claim. The previously
missing poison implementation plan is now in the private workspace findings.

Observed (code; Script_Game.dll RVAs):
- Native OnTick (0xB0EF0) calls AddHitPoints directly at 0xB11AB, then can select
  unconsciousness or death.
- Entry bytes 55 8B EC 83 E4 F8 are three complete instructions. The registered
  ABI is four-argument stdcall, RET 10h, despite three arguments in decompilation.
- Poison contributes -5 per executed callback, not a proven -5 per second.
  Disease initially zeroes stamina and has different player/NPC regeneration.
  Stamina (SP) is in neither NpcRecord nor PlayerState, so no SP state travels.
- Successful reapplication refreshes the world timestamp even if already poisoned.
  Two applications within the same world second can have identical before/after
  bits and timestamps. Both MagicPoison/Disease and AssessHit contain status
  application paths.
- Specific poison/disease cures target other; HealOther clears the statuses and
  heals HP, while ordinary Cure only heals HP in the examined body. B's current
  HealOther routing covers player targets, not foreign NPCs.
- OnTick's other=None and a later CurrentAttacker are not reliable stored spell
  provenance. IsMirrored currently becomes true only after body.pacified.

Derived and proposed (not tested):
- Blocking only the HP write leaves the later decision active. Proposed guard:
  native OnTick entry for foreign replicas, preserving the local controlled body
  and authoritative NPC simulation. Check the full build-specific fingerprint
  before patching.
- Observe the accepted status setter within the source context; extend HealOther
  target routing to foreign NPCs.
- Keep the source through the actual native Kill/Defeat completion. Proposed
  completion hooks preserve native actor/KO semantics and use one reward path with
  duplicate protection, including local and remote recipients.
- Guard the known ownership transition as well as body.pacified. Handoff needs
  status time, source and canonical defeated state, distinct from the mirror's
  temporary XP suppression flag.

Cleanup and reward integration were checked independently; a final review corrected
two assumptions, and the bullets above are the corrected versions: no SP transport
exists to rely on, and HealOther routing reaches player targets only. Native tick
cadence, complete cleanup coverage and the proposed hooks still require B's
isolated runtime tests. (B's `2a9e983` later skipped OnTick on mirrors and puppets,
see the first agent C section.)

## Agent C: death attribution, creature lifetime and first-frame arrows (2026-10-20)

Static analysis (C's fourth assignment) against mod commit 7595022 (protocol 40,
including DeedContext 32fbcfe: net.h records who did a deed and with what). No games,
builds, installations or mod changes. Detailed private findings: death-events-c.md,
creature-distance-c.md, review-b-c.md, arrow-spawn-c.md. Fixed source excerpts,
native call sites and hook bytes were checked; this is not runtime acceptance.

Observed (code and archived runs; Script_Game.dll RVAs):
- Native Kill (0x1BC30) and Defeat (0x27510) are two entry points suitable for one
  victim-scoped completion routine. Their direct callers include cleanup as well as
  combat states, which registration-table wrappers miss.
- OnCheatKill stops the victim and selects a death task but does not write
  CurrentAttacker. A caller passing victim as both arguments has not thereby
  replaced an older attacker.
- OnTick's summoned-creature distance branch exits before poison/burn damage,
  clears the party leader, but leaves CurrentAttacker. Native fall death explicitly
  clears CurrentAttacker.
- Kill can call NotifyEnclave, which can call Kill(None, another member). Native
  Entity::Kill is a different removal mechanism.
- One cast can kill several victims; one summon can kill repeatedly; KO and later
  death have different XP/crime effects.
- Client-owned creature snapshots stop beyond 40 m. Host claims bypass the normal
  three-second stale release, while the copy checker refreshes its timer from
  IsMirrored (pacified), not a fresh packet. Three passing `stageCreatureLife` runs
  (`20261020-185021`, `-185346` and `-185644-together-p2`) accepted six living
  mirrored ArmyOfDarkness creatures on the host after the client moved 50 m away:
  the suite rejects only `free`, not `mirrored`. This concerns summoned creatures,
  not the NPC hand-back of stage E1, whose client disconnects.
- Two TakeOver defects in the code: host takeover leaves the prior client claim,
  and failed takeover leaves a delayed 700-ms Follow task. No concrete UAF or
  overflow in the three numeric status reports was demonstrated.
- Arrow `spawned=0` is a world-list/owner scan timeout, not Entity::Spawn's return.
  Native early attachment and projectile OnExitProcessingRange -> Entity::Kill are
  distinct paths in the code.

Derived and proposed (not tested):
- Capture confirmed claim provenance before the delayed death state; validate
  victim authority and source ownership first. Cleanup reason must outrank stale
  hit/status context.
- Use victim-scoped reentrancy and completion identities rather than inheriting a
  single global killer. Separate source-action identity from completion identity,
  include life/session generations, and migrate the existing reward path rather
  than adding a second one. Process-local counters plus player names alone are not
  persistent deduplication identities.
- Send reliable creature endings independently of movement interest; clear
  replica/claim registrations together. Temporary summons must not be re-offered as
  persistent slave companions. `stageCreatureLife` needs to fail on a frozen
  `mirrored` copy.
- Record spawn, touch, attachment, range exit, decay and destruction per arrow GUID
  before changing behavior.

Unknown:
- The cause of the eight `spawned=0` shots in run `20261020-204556-together-p2`
  (`shots=8 announced=0 unresolved=8`, no touch log, no new arrow GUID). Experiment 0
  above shows one way it happens (self-hit, arrow attached to the shooter), but this
  run has no contact evidence.

The death plan received three independent cross-checks. Implementation and
isolated runtime acceptance remain with the gameplay agents.

## Agent C: release-day guard and transport review (2026-10-21)

Static release candidate fdb1229, compared with packaged baseline 64ad370.
No mod edits, builds, installations or new runtime acceptance. Private reports:
attachto-guard-c.md, release-review-c.md, hamachi-net-c.md; player-docs draft and
an empty playtest form were also prepared for the gameplay agents.

Observed (code and archived crash logs):
- The crash register values match AttachTo's failed VTable-slot read, not a null
  Entity argument. The matching GetCombatMoveLength return address follows its
  Other-wrapper construction. That routine uses only Self.Species and action for
  its result.
- The crash logger scans stack words for module-range addresses; it does not
  unwind frames. Native callsites support a candidate chain through a saved
  combat action target.
- New reputation synchronization can overwrite unscanned local changes when a
  host state arrives before the client's 500-ms scan; reliable packet order does
  not protect local state changes. Pending theft rollback uses a global
  returned-item counter rather than matching the rejected transaction.
- The host binds all IPv4 interfaces. Vendored ENet limits UDP payload to 1392
  bytes by default; that can mean 1420 bytes including inner IPv4/UDP headers.
- NPC snapshots reach 3031 application bytes; save chunks reach 16012. ENet
  fragments them, but oversized packets sent with flags zero are promoted to
  reliable fragments unless UNRELIABLE_FRAGMENT is explicitly selected. Losing
  such an NPC fragment can hold later snapshots behind its reliable sequence.
- ENet peer timeouts, 15-second puppet expiry and three-second NPC stale handling
  are separate mechanisms. No existing configurable latency/loss simulator was
  found; disabling a snapshot handler does not test UDP loss or retransmission.

Derived and proposed (not tested by C):
- An independently reviewed narrow guard passes an empty Entity wrapper for Other,
  without binding the suspect raw target. Passing null instead would re-enter the
  original SPU Other lookup. Validate the original implementation before wrapping.
  This avoids one unnecessary dereference, not every upstream/downstream target use
  or the unproven source of corruption. The runtime guard requires combat
  regression. A global dead-entity AI skip or virtual IsKilled on a suspect pointer
  is not a safe lifetime proof.
- ENet host MTU 1200 before connection establishment and 18-record NPC send
  batches, retaining the 48-record receive limit and wire layout. Datagram
  aggregation means smaller batches alone do not cap UDP size.

Unknown:
- The exact target GUID and destruction history behind the AttachTo crash.
- The actual VPN/path MTU (unmeasured, so no universal Hamachi MTU was asserted).

Afterwards in `master` (per commit messages, not re-tested in this write-up): `5bea542`
gives GetCombatMoveLength the empty wrapper (QA `movelength`, suite `stageRaid`);
`66e44e1` sets ENet MTU 1200 and 18 NPCs per message, and a client now reports its
unreported deeds before taking the host's counters and keeps held-back thefts on top.

The package choice and runtime acceptance remain with the release agents and
owner. Player texts distinguish the fallback package from the unapproved candidate;
final ZIP identity and compatible fallback save are intentionally not invented.

## 2026-10-21: code review of the 2026-10-20/21 notes

A review of the sections above found contradictions; each was checked against the
mod's git history and the private findings, and the sections were corrected in place.
No game was started and no test was run for this review.

Observed:
- `ce6c5c7` (P2 arrows, first version) does not contain the dead-mirror fix
  `9acdaf5`; every later baseline named above (`d4dc47e`, `5628884`, `760af83f`,
  `d2e331f8`, `f1ad45e3`, `32fbcfe`, `7595022`, `64ad370`, `fdb1229`) does
  (`git merge-base --is-ancestor`). The standing dead orcs in the first P2 runs
  therefore do not contradict the fix.
- The six frozen mirrored copies are ArmyOfDarkness creatures in `stageCreatureLife`,
  not stage E1 NPCs.
- `Game.dll+0x2BD1F3` was a faulting EIP, not a stack-scan word.

Derived:
- Agent A's `0x100b4f50` is the VA of Script_Game.dll RVA 0xb4f50: the same trace
  writes `ContinueRoutine` as `0x100a5ea0`, which the notes give as RVA 0xa5ea0.

Unknown:
- Whether `ContinueRoutine` has direct native callers (see the puppet-turn section).
- Whether the skill union on hero restore shows at runtime.
