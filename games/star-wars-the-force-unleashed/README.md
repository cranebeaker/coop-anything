# Star Wars: The Force Unleashed – Ultimate Sith Edition (2009, PC)

| | |
|---|---|
| Engine | LucasArts Ronin: Lua 5.1 scripting (32-bit float numbers), Havok/DMM/Euphoria physics, Aspyr PC port (AWL) |
| Versions tested | Steam app 32430, build 6522; `SWTFU.exe` SHA-256 `1C156705…2032`, unpacked with Steamless v3.1.0.5 to `2D872CA9…7B8A` |
| Projects | tfu-coop (code repository not yet published); directory entry "tfu-coop" |
| QA level | automated |
| Last updated | 2026-10-27 |

## Where things stand

Online co-op for 2 to 4 players, each on their own PC with their own camera, works in automated tests
with several game copies on one PC. The tests include the Kashyyyk prologue started from the main menu,
the TIE Factory, and simulated internet delay and packet loss. A real session between two PCs has not
happened yet.

## Existing work

- No co-op or netplay mod for TFU1 on PC was found (web, Reddit, YouTube, ModDB, GitHub, 2026-10-23).
  The Wii (Krome) had a local two-player Duel mode and the PSP had 2–4 player modes, but these are
  different codebases from the PC version.
- TFU-Modloader (Reloaded-II, C#, no license), https://github.com/LongTermMess/TFU-Modloader: its
  `LuaRegister` 0x4d2120 and other addresses match the Steam build 6522 exe after Steamless.
- TFU-Import-3dsMax (GPL-3.0, `.lp` packs, GTO models), pc-trainers (CE scripts), and Nexus mods 30 (NPC
  spawner) and 35 (frame-rate notes).

## How the game works (for co-op)

- **Scripting:** Lua 5.1 with 2,147 C bindings registered through `LuaRegister` (0x4d2120), each with a
  help string (`re/lua_bindings.py` extracts them). Globals are protected: create them with
  `rawset(_G, …)`. `ronin.Update(dt)` runs every frame at dt = 1/30 and can be wrapped from Lua.
- **Players:** the engine keeps a player map (ren → PlayerComp at `*0x03a1be80`). `Mp.CacheEncounterAsset`,
  `Mp.SetPlayerController` and `Mp.FixupPlayerSpawn`, left over from the console Duel mode, add
  player-controlled characters. Each level has its own player actor (`playerDarthVader` in the prologue,
  `player` in the TIE Factory). A puppet must use the same actor or it never takes its slot.
- **Input:** the PC port polls only XInput pad 0. The input manager (`vtbl+0x50`) accepts state for
  controllers 0–3, so remote input is injected after each poll.
- **Two-player limits in the engine:**
  - The per-controller object at `game+0xac` exists for controller 0 only. Four code sites index it
    without a bounds check, which crashed controllers 2 and 3.
  - `FixupPlayerSpawn` crashes for controller ≥ 2 for the same reason.
  - The camera matrix copy (0xc349b0) serves controller 0 only.
- **Death:** the death handler (0x7c2200) calls the respawn (0x7b9330) with `isMultiplayer` = 0, which
  restores the checkpoint or reloads the whole world. With 1, the player is reset in place.
- **Rendering on PC:** a WM_SIZE marks the D3D9 device lost, and the game recovers only on
  `D3DERR_DEVICENOTRESET`. Under DXVK without fake focus loss this left an unfocused window at ~0.4 fps.
- **Level scripts and spawns** are not deterministic between copies: wave numbering, types and even
  whether a wave spawns differ. AIs can be paired by their **spawn point** (recorded from level load,
  ≤ 0.06 m apart), not by handle or `Spawn_N` name. `ren.PathInside` gives `…/Spawn_<n>_<actorType>`.
- **Cinematics** are EDLs (`edl.Go`, `edl.StopEdl`); a level-intro EDL makes no Lua calls while it runs.
- **Saves:** loading a level with `ronin.LoadWorld` skips the menu's save-slot setup, so the first
  checkpoint shows a SAVE FAILED dialog. Starting from the menu avoids it.

Tools: Ghidra 12 headless scripts (decompile, xrefs, operand search), a Lua console injected into the
game (drop `.lua` files into a folder), a stack sampler and a crash logger.

## What we changed

All code is in a `dinput8.dll` proxy plus Lua modules:

- **Background stall** (an unfocused copy at 0.4 fps):
  - Root cause: device-lost after a resize, never recovered under DXVK.
  - Fix: patch 0xb5ddab to reset unless the result is DEVICELOST.
  - Test: two copies, both 300–600 input polls per 10 s.
- **Players 3–4 crashed:**
  - Root cause: missing per-controller objects.
  - Fix: the four loads go through stubs that return a zeroed dummy object for controllers other than 0.
  - Test: 4 copies, every copy tracks the other three, and a slot-2 puppet killed a host enemy.
- **Death reloaded the world for everyone.** Fix: patch the death call to take the multiplayer respawn,
  then move the player to a teammate. Test: kill, fall and host-side kill, all without a reload.
- **Network link (UDP):**
  - The host (id 0) relays between clients; remote player p drives local controller (p − myId) mod 4.
  - The host owns enemies (position, health, death), client player health, the world name and
    cinematics.
  - Clients hide AIs of their own that no host AI claims, and spawn copies by actor type.
  - Hardening after a review: session key, client source check, slot binding, peer expiry, finite-value
    checks. Test: wrong-key, right-key-other-address and NaN floods ignored.
- **Autorun from `coop.ini`**, and on-screen status through `ui.RewardMessage`.
- **Level reloads and cutscenes:** the host counts world loads (marker rens vanish on a load) and sends the
  count; clients reload when it goes up. `skip_cutscenes=1` presses A while no Lua runs, which skips
  level intros. Test: reload in 2 min 13 s, both reload once (m14r4).
- **Force powers for puppets:** pad Force Push needs controller 0's per-controller object plus a profile
  with the powers unlocked. On the host the stubs share controller 0's object and puppets get the host
  player's profile (Force energy stays per character). On clients sharing made puppets act on their own,
  so it is host only. Test: a remote player's pad push threw a host trooper 18 m; no kills while idle.

## QA

- Harness: shell drivers launch several copies of the game, load a level or start from the menu, answer
  dialogs through a virtual pad, drop Lua into each copy's console and grep their logs.
  - `m6-run.sh`: kill, fight and client hits.
  - `m7-*-run.sh`: death, level, cinematics, waves.
  - `m8-run.sh <n>`: 2–4 players.
  - `m9-*-run.sh`: autorun and menu start.
  - `spoof_test.py`: forged packets.
  - `m11_world_diff.py`: object drift.
- Last results (2026-10-26/27, Steam build 6522): all checks passed, including 4 players (m8r5), 150 ms /
  5% loss (m6net1), menu start in the prologue (m9m3) and keyed sessions with forged-packet floods (m9a5).

## How to play

See `PLAYING.md` in the tfu-coop package:
- every player unpacks their own exe with Steamless and copies in `dinput8.dll` and the `coop` folder;
- set role, player id, host address and a shared session key in `coop.ini`;
- the host forwards UDP 37000 or everyone joins a VPN such as Tailscale;
- start New Game; co-op switches on in the first level and clients follow the host's level.

## Known issues

- Enemies on clients slide into place instead of walking, because their AI is off. Walking them with
  `mobility.RequestPathToPosition` was too slow (~0.5 m/s), and it let them die from the client's own
  hits.
- Pre-rendered movies between levels play on each PC separately (the cutscene skip probably skips them;
  not tested).
- A reload of the same level on the host now reloads it on clients (tested with `ronin.LoadWorld`). A
  checkpoint restart from the pause menu and loading a save with clients connected have not been tried.

## Next steps

- A real two-PC session over the internet.
- A full-level playthrough, checking doors and objectives on clients with the world diff.
- Force powers for remote players: Push and Grip work on the host (Grip: m13-grip-run, a puppet lifted a
  trooper 2.5 m); Lightning fires for puppets (seen on screen; damage not yet measured); Repulse untested; in one spot a puppet is pushed into the air by something on the host and cannot grip.
- Frame rate above 30 fps (the game caps at 30; physics is reported to break above 60).
