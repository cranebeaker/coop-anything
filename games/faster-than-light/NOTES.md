# FTL: Faster Than Light — co-op notes

## Game identity (observed 2026-11-05)
- Store: Steam, app 212680, buildid 4710954, Linux native (no Proton).
- `<game dir>/data/FTL.amd64` SHA-256 `23897b7e7b538da7b5858e86aed87bd2a1e567d33d12d956761099628f557d80`
  ELF x86-64, "with debug_info, not stripped".
- `<game dir>/data/FTL.x86` SHA-256 `be037b0ba1146bc66e303c99c432104de9aa4ac91b488b7487e63cde4ed87cc8` (ELF i386, debug info).
- Version: 1.6.13 — derived: the amd64 hash equals the one Hyperspace documents for "FTL 1.6.13 Steam, Linux amd64"
  (https://github.com/FTL-Hyperspace/FTL-Hyperspace, wiki/FTL-Source-Layout.md). A string "1.6.29" also occurs in the
  binary; its meaning is unknown (not the game version per the hash match).
- Engine: Subset's own C++ engine (SDL/OpenGL); data in `ftl.dat`; libsteam_api.so shipped.

## Prior co-op attempts (research 2026-11-05)
- tomahawk812/FTL---Faster-than-light-Multiplayer — README-only stub, last push 2013-09-06. Nothing usable.
- Tachyon / TachyonsCE (https://github.com/Dav-Edward/TachyonsCE) — standalone FTL-like multiplayer game, not a mod.
- Final Frontier (Garry's Mod gamemode, https://www.pcgamer.com/ftl-garrys-mod/) — separate first-person game.
- Subset Games: no plans for multiplayer (quoted at https://playco-opgame.com/ftl-faster-than-light/).
- No co-op mod found on Steam discussions (https://steamcommunity.com/app/212680/discussions/0/558752449822405389/).
- Hub: no directory entry, no issues/PRs for FTL (checked 2026-11-05).
- Claim issue: https://github.com/cranebeaker/coop-anything/issues/329 (opened 2026-11-05).

## Building blocks
- FTL-Hyperspace (GPL-3.0-or-later, active: release v1.23.2 2026-09-25, push 2026-11-04): binary hook mod with Linux
  1.6.13 build scripts. Not multiplayer. Derived: best base for a co-op layer (hooks, function map from DWARF already done).

## Path
- Build from zero (AGENTS.md path 2); nothing to revive.

## Design decisions (owner, 2026-11-05)
- Ship sharing is a host setting, switchable mid-run: free-for-all (anyone commands anything) and own-crew
  (players command their crew + systems those crew staff). Host-authoritative; guests send commands; the mode is a
  permission filter on the host. Free-for-all built first. Captain-only (for now): jump, events, store, upgrades.

## How Hyperspace loads (observed in its source, 1b05a07, 2026-11-05)
- Linux: `Hyperspace.1.6.13.amd64.so` next to `FTL.amd64`, launched with `LD_PRELOAD` from the `FTL` script
  (`Release Files/Linux/README.txt`); entry is `__attribute__((constructor)) launchHyperspace()` in `src/main.cpp`.
- Hooks: ZHL engine, functions found by byte signature; 1.6.13 Linux map in
  `libzhlgen/test/functions/ELF_amd64/1.6.13/*.zhl` (185 class files). New features go in `src/features/<name>/`.

## Player command surface (from the 1.6.13 .zhl map; not yet traced at runtime)
Candidate hook points for "a guest asks the host to do X" — derived from names, unverified:
- Crew move: `ShipManager::CommandCrewMoveRoom`, `CrewMember::MoveToRoom`; selection in `CrewControl::{LButton,RButton,SelectCrew}`.
- Power: `ShipSystem::{IncreasePower,DecreasePower}`, `PowerManager::*`; UI in `SystemControl::{LButton,RButton}`.
- Weapons: `WeaponControl::{ArmWeapon,SetCurrentTarget,Fire,SetAutofiring,SwapArmaments}`; `CombatControl::{SelectTarget,MouseClick}`.
- Doors: `Door::{Open,Close}`; Teleport/Mind/Hack/Cloak: `TeleportSystem::InitiateTeleport`, `MindSystem::InitiateMindControl`,
  `HackingSystem::StartHacking`, `CloakingSystem::SetTurnedOn`; `CombatControl::{ArmTeleporter,ArmMindControl}`.
- Captain-only: `StarMap::{TravelToLocation,MouseClick}`, `ChoiceBox::{MouseClick,GetChoice}`, `Store::MouseClick`, `CommandGui::SetPaused`.
- State transfer candidates: `CommandGui::{ExportData,ImportData}`, `ShipManager::{ExportShip,ExportBattleState}`, `*::SaveState/LoadState`.

## Open questions
- Model for guests' view: full snapshots from host vs. guests simulating with corrections. Is FTL's sim deterministic
  Observed: a global seedable RNG exists (`random32`, `srandom32` in Global.zhl).
  enough for lockstep? unknown.
- Does the Hyperspace .so run without the Hyperspace.ftl data mod installed? unknown.

## Randomness and time (observed by disassembly of FTL.amd64 1.6.13, 2026-11-05)
- `random32` (0x6615c0): 64-bit LCG, `state = state*0x5851f42d4c957f2d + 1; return state >> 33`; state in global
  `_sil_random_state` (0xa35ff0, non-PIE so fixed). `srandom32(seed)` stores the seed. 327 direct call sites.
- libc `rand`/`srand` are also imported and used in ~100 functions, mixing simulation (ShipGenerator::CreateShip,
  StarMap::GenerateMap, GenerateReward, CombatAI::PrioritizeSystem, Projectile::RandomSidePoint, Room::OnLoop,
  ShipManager::SunDamage) and visuals (ParticleEmitter::Birth, Animation::Update, ExplosionAnimation::Update,
  ArmamentBox::OnLoop). Derived: a preloaded .so can define `rand`/`srand` itself and own both RNGs.
- Frame time: `CFPS::GetSpeedFactor` (forceDetour in the .zhl map) scales simulation per frame; hookable to a fixed step.
- Derived plan to test: deterministic lockstep = fixed speed factor + shared seeds for both RNGs + inputs applied on
  the same frame; render-phase RNG calls get a separate stream. Verify by per-frame state hashes on two copies.

## Who calls the action functions (call graph from disassembly, 2026-11-06)
Observed: the action functions are shared by player UI and AI, so they cannot be intercepted unconditionally.
- `ShipManager::CommandCrewMoveRoom` <- CrewControl::RButton (player) and CrewAI::{AssignTask,AttemptToAssign,...} (AI).
- `ShipSystem::IncreasePower/DecreasePower` <- SystemBox::{MouseClick,MouseRightClick,AdjustPowerTo} (player) and ShipAI::CheckPowerLevels (enemy).
- `ShipManager::PowerWeapon/DePowerWeapon` <- WeaponControl::{SelectArmament,DeselectArmament}; drones likewise via DroneControl.
- `WeaponControl::Fire` <- CombatControl::SelectTarget; `HackingSystem::StartHacking` <- CombatControl::SelectTarget.
- `CloakingSystem::SetTurnedOn` <- CloakingBox; `HackingSystem::InitiatePulse` <- HackBox; teleport via CompleteShip::InitiateTeleport.
- `CommandGui::SetPaused` <- CApp::OnInputBlur/OnInputFocus (auto-pause on focus loss: must become a command or be off).
- Doors: no Door::Open/Close call on desktop; CrewControl::MouseMove picks the door (Ship::GetSelectedDoor) and
  CrewControl::LButton toggles it inline. Intercept at CrewControl::LButton.
- Hyperspace already hooks many of these UI handlers (CustomCrew, TileBasedCrewMovement, CustomCommandGui, ...).
Derived plan: intercept action functions only while an input event is being handled (CApp::OnLButtonDown etc.),
encode them with stable ids (ship id, crew index, system id, weapon slot, door id), apply on every copy in lockstep.

## Lockstep core (mod repo, branch coop)
- `src/features/coop/core/`: TCP transport + lockstep session, game-independent C++11.
- Fake-world test `tests/coop/fakeworld_test.cpp` (2026-11-06): 12/12 checks pass — 2 and 8 players, both sharing
  modes, 30 seeds x 4 players, mutation twin (double apply) detected, over-limit and wrong-build guests refused.
  A guest-stall bug (first `delay` frames had no bundle on guests) was found by review and fixed before the run.

## Frame time (disassembly of CFPS::OnLoop, 0x5a4010)
Observed: `SpeedFactor = (now_ms - LastTime_ms)/1000 * (16 + 7*speedLevel)`, clamped to [0, 0.5]; CFPS fields
RunningTime, OldTime, LastTime, SpeedFactor (+0x18), NumFrames, Frames, speedLevel (+0x24).
Derived: normal play is 16 speed units per second; at 60 FPS SpeedFactor = 0.2667. Lockstep uses a fixed 16/60.

## First real-game runs (2026-11-06, mod branch `coop`, FTL 1.6.13 Steam Linux, headless gamescope)
Observed problems and fixes, in order:
- A preloaded Hyperspace inside gamescope/Xwayland hangs them: pass LD_PRELOAD to FTL only (`gamescope -- env LD_PRELOAD=...`).
- Without Hyperspace.ftl data, Hyperspace shows a blocking SDL message box ("hyperspace.xml not found"); a wrong hook
  return type also ends in a ZHL message box. Added `HYPERSPACE_NO_DIALOGS=1` (log, and exit on ZHL errors).
- First launch shows the language chooser, so the main menu never opens: test copies get a settings.ini.
- `random32` is detoured by Hyperspace (CrewNames.cpp); its state address is read from `srandom32` (not hooked).
- `ChoiceBox::KeyDown` only works when the dialog-hotkey setting is on (Settings+0x34 = dialogKeys); the choice
  command sets `selectedChoice` directly.
- libc `rand()` is interposed correctly (checked by dlsym) but was never called in these runs; Hyperspace replaces
  the vanilla generators that used it.
- Desync 1: running several frames without a render (guest catching up) changed how often `Animation::Update`
  draws `random32` (cosmetic: random flicker frame, random frame sound). Found with a per-caller trace; fixed by
  saving/restoring the random32 state around Animation::Update. Hitch test before: ~1 in 3 runs failed; after: 6/6 pass.
- Desync 2: `ParticleEmitter::Birth` (particle effects) likewise; added ParticleEmitter.zhl (OnUpdate, Birth
  signatures) and the same save/restore. 
Results (all with the chaos bot: random crew moves, power, doors, weapon power, pauses from every copy):
- 2 copies, 3600 frames: 4/4 same seed + 6/6 different seeds (3 with forced catch-up): PASS, 513-588 commands.
- 4 copies own-crew, 3600 frames: PASS, 485 commands applied, 635 rejected by the host (others' crew/systems).
- 8 copies free-for-all, 3600 frames: PASS, 2192 commands, 120 hash checks, 0 stalls.
Limits of these tests: only the start beacon (no jump, no enemy, no combat yet); the bot submits commands directly,
so the mouse/keyboard interception hooks are not exercised yet.

## Jumps, events, combat (2026-11-06)
Observed and fixed:
- Jump: StarMap::MouseClick sets `potentialLoc` and `readyToTravel` (StarMap+0xc0); StarMap::GetNewLocation then
  moves the ship. C_JUMP (captain) sets both on every copy, for connected beacons only.
- The FTL charge (`ShipManager::jump_timer`, DWARF offset 1544) is filled by `ShipManager::SetSafe()`, which
  WorldManager::OnLoop calls when `HostileEnvironment()` is false (no hazard, no intruders, no mind-controlled enemy).
- Pause: `CommandGui::IsPaused() = bPaused || menu_pause`; `menu_pause` (CommandGui+12946) is recomputed in
  CommandGui::OnLoop from the focus-window list (store, ship screen, map, menu...). An event-opened store froze the
  simulation. Co-op patches the three writes (0x4fec6b, 0x4fecc5, 0x4fed03; bytes checked first) and sets
  `menu_pause` each frame to `choiceBox.bOpen || gameOverScreen.bOpen` (shared state only). Local windows no longer pause.
- Jumping while the store window is open segfaulted both copies at the same frame; C_JUMP closes store and ship
  screens first (a player must close them to reach the map).
- Event options with unmet requirements have `ChoiceText::type == 1`; the bot picks the first other option.
- Cosmetic random32 draws found by trace and isolated (save/restore state): Animation::Update, ParticleEmitter::OnUpdate,
  SpaceManager::OnLoopNebulas. Jumps reseed random32, which can hide a stream divergence until the next check.
- Struct layout of Hyperspace's ShipManager matches DWARF (jump_timer 1544, fuel_count 1552, bJumping 1652,
  lastJumpReady 2257), checked at runtime.
Results: "fight" bot (chaos + weapons kept powered + targeting enemy rooms), 2 copies, 7200 frames: enemy destroyed
(hull 7 -> 0), 1683 commands, 240 hash checks equal. Batch of 4 (10800 frames) before the nebula fix: 3 pass, 1 fail
(random state diverged at frame 90 by one random32 call from OnLoopNebulas).

## Open design questions
- Desync recovery and late join: plan to send the host's save file (FTL saves mid-combat on quit) and restart
  lockstep from it with fresh seeds. unknown whether mid-combat save/load restores every simulated detail.
- Internet play: the TCP transport needs port forwarding. Steam lobbies + relayed P2P under the game's own app id
  would avoid that; needs the owner's OK to use their Steam account for tests.

## Resync, late join, keys (2026-11-06, mod commit 262a0a7)
- Resync: host `WorldManager::SaveGame()`, reads `FileHelper::getSaveFile()`, sends it in a START(load) blob with new
  epoch; every copy (host too) opens the main menu, waits 3 loops (the game auto-saves when its menu opens over a game),
  writes the file to its own save path, calls `TutorialManager::Stop()`, `WorldManager::LoadGame(path)`,
  `MainMenu::Close()` (the main menu's Continue path in CApp::OnLoop), reseeds and restarts lockstep.
- Forced resync at frame 900 mid-combat (fight bot, 2 copies, seed 51): save 7153 bytes; status before/after the reload:
  hull 29, fuel 15, enemy hull 8, FTL charge kept; 150 hash checks over both epochs equal; PASS.
- Late join: 3rd copy joined at host frame 706; host resynced everyone (7325-byte save); 60 hash checks after the join
  equal; PASS.
- Keys bot (real hotkeys through CApp::OnKeyDown, 2 copies, 3600 frames): 93 commands from hotkeys (power, weapon
  power/clear, autofire, pause, and the captain's dialog/jump), 120 hash checks equal, PASS. `Settings::GetHotkey`
  returned 0 for un_shields/un_engines/un_oxygen/un_medbay/un_weapons/open_doors/close_doors: unknown whether they are
  unbound by default or the names differ.

## Results table (all PASS unless noted; headless gamescope, one PC)
| Test | Copies | Frames | Commands | Hash checks |
|---|---|---|---|---|
| chaos, 6 seeds (3 with forced catch-up) | 2 | 3600 | 513-588 | 120 each |
| chaos own-crew | 4 | 3600 | 485 applied, 635 rejected | 120 |
| chaos | 8 | 3600 | 2192 | 120 |
| fight, seeds 32/41 | 2 | 10800 | 2617 / 2552 | 360 each |
| fight own-crew, seed 42 | 2 | 10800 | 1420 | 360 |
| fight, seed 43 / own-crew seed 44 | 4 | 10800 | 4939 / 1967 | 360 each |
| fight, seed 45 | 8 | 7200 | 6306 | 240 |
| forced resync mid-combat | 2 | 3600 | 991 | 150 |
| late join | 2+1 | 706 + 1800 | 751 | 60 after join |
| keys (real input path) | 2 | 3600 | 93 | 120 |
| fake world (core only) | 2-8 | 300-600 | random | 14/14 checks incl. 2 mutation checks |
Earlier failures and their fixes are listed in the sections above (catch-up desyncs, nebula, store crash, frozen store).
