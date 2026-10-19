# Resident Evil Revelations (2013)

| | |
|---|---|
| Engine | Capcom MT Framework (Mobile / 2.x, 32-bit DirectX 9) |
| Versions tested | Steam build 13298487 (`rerev.exe` SHA-256 `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA`) |
| Projects | Official Raid Mode (built-in 2-player co-op); Campaign co-op: (none found, gap) |
| QA level | none (official Raid Mode not tested by us; campaign co-op does not exist yet) |
| Last updated | 2026-10-19 |

## Where things stand

Resident Evil Revelations natively features full 2-player online co-op in its arcade **Raid Mode** via Steam invites. However, the main story campaign is strictly single-player with AI-controlled partners; no released or working campaign co-op mod exists for this title today.

## Existing work

- **Official Raid Mode:** Built into the game. Supports 2 players online over Steam matchmaking and P2P with character selection, weapons, levels, and stage progression.
- **Nucleus Co-op handler:** reported to run two local instances for split-screen Raid Mode. Unverified: no source link recorded yet.
- **Campaign co-op:** None found. Searches across GitHub, Nexus Mods, ModDB, Resident Evil Modding forums, and Steam discussions (checked 2026-10-19) confirmed no functional campaign co-op mod has been released.

## How the game works (for co-op)

- **Engine:** Capcom MT Framework (Mobile variant ported to PC/DirectX 9, PE32 x86). Assets are packaged in ARC version 7 archives under `nativePC/archive/`.
- **Netcode & Primitives:** The game contains a full network replication subsystem (`cCoopSyncData`, `sCoopManager`) designed for Raid Mode, supporting player pad sync (`cPlayerPadSyncData`), enemy state and death replication (`cActivateEnemySyncData`, `cDeathEnemySyncData`, `cHunterNet`, `cScagdeadStateBaseNet`), door and block transitions (`sArrange::cNetSyncGoNextBlockData`), and cutscene synchronization.
- **Game-mode flag (verified 2026-10-19, statically and at runtime):** `sGameSystems` singleton pointer at `0x01169108` (no ASLR, fixed address); `getData()` is the pointer at `+0x24`; the int at `data+0x20` is the mode, `0` = campaign (`isCampaign()`), `1` = coop (`isCoop()`). The checks are inlined: 686 reads of this field in `.text` (432 compare with 1, 147 with 0, 107 with a register). Read at runtime from the boot/menu: mode `0`. Inside the Raid menu it reads `1`.
- **Mode setter (Ghidra, 2026-10-19):** the only writer is `0x8c6960` (thiscall on `data`, one int argument). It stores the mode and broadcasts the change to registered listeners (vfunc `+0x2c(mode)`). The title menu (`uTitleMenuManager`) calls it with `1` when entering Raid Mode; session leave/error paths call it with `0`. Experiments must call the setter rather than write the field.
- **Global mode flip experiment (2026-10-19):** calling the setter with `1` inside a campaign episode keeps the game running (the pause menu loses the campaign objective, nothing crashes). Restarting from a checkpoint with mode `1` crashes every time: the campaign save code asserts on the Episode game system (`0x0116A7D4`), which the reload did not keep. Mode `1` must stay hidden from area loading, or individual coop checks must be patched instead. Runtime check: the Raid menu sets mode `1`, leaving it sets `0`.
- **Mode 1 only between loads (2026-10-19):** dropping to mode `0` as soon as a load starts (Episode system state at `+0xFC` changes) makes a checkpoint restart with mode `1` survive. Switching to `1` while an episode is still loading soft-locks the game on a black screen. 375 functions read the mode, including the arrange loader (`scene_coop` vs `scene` assets) and the cutscene-end flow (waits for the network partner when the mode is `1`). In a manual playtest mode `1` ran for 173 s of Episode 1 without errors, including at least one door and the first enemy. Every load, video or save was announced by a change of `+0xFC` (values `1`–`4`), and the DLL dropped the mode to `0` before it ran. No second player appears yet: players are created at area load, which now always runs in mode `0`. The switch to the first video (state `+0xFC` = `4`) dropped it back to `0` in time, and the video played normally.
- **Player 2 route (static analysis, 2026-10-19):** campaign partners (Parker, Jessica, Keith, Quint, ...) are `uPlayer` subclasses, the same class as the Raid second player. `uPlayer::updatePad` (`0x6afbd0`) picks its input by a control type (`1` local pad, `2` AI think manager at `+0x1630`, `3` remote/network) read from `[[[unit+0xe80]+0x38]+0x34]`, and writes a per-unit pad group at `+0x1138`–`+0x1151`. Plan: switch the partner from `2` to `3` and feed its pad from the second player, with no new character and mode left at `0`. First test: Parker was moved from the numpad with type `3`. Aiming did not work yet, because `updatePad` clears the aim flag for type `3`. With the pad written after `updatePad`, a second test gave about 6 minutes of control and repeated AI/player switching with no errors. Moving, running and aiming work (aiming was confirmed by eye). Shooting is not wired yet.
- **Partner AI:** `cNpcThinkManager` functions found so far (`0x614d50`, `0x61e080`) are the checkpoint save/load of the partner AI state; the per-frame update is still to be found. The engine uses MT Framework's own type system, not MSVC RTTI.
- **Campaign vs Raid Mode:** Driven by `game_systems::sGameSystems::getInstance().getData().isCoop()` and `isCampaign()`. In Campaign, the companion is spawned as an NPC actor (`npXXXX`, e.g. Parker `np1110`) governed by `cNpcThinkManager`. In Raid Mode, player 2 is instantiated as a playable character (`plXXXX`) and replicated across the session. Notably, story episode flow code (`cEpisodeEndFlowState.cpp`) already contains `isCoop()` evaluation.
- **Hooking vector:** `rerev.exe` imports `DINPUT8.dll` (`DirectInput8Create`), so a proxy `dinput8.dll` in the game folder loads without patching the exe.
- **Anti-Cheat:** the Steam store page does not list Valve Anti-Cheat (checked 2026-10-19). The mod should still use its own transport (Spacewar AppID 480 or UDP) and stay out of official Raid Mode lobbies.
- **DRM wrapper:** SteamStub (`.bind` section, entry point inside it), not Enigma. `.text` is not encrypted on disk (entropy 6.4), so static analysis works on the file as-is.

## What we changed

- Documented game identity, engine architecture, and co-op status as a baseline gap in the co-op directory.
- Added Capcom MT Framework engine detection and hooking tools to `tools/mcp/coop_mcp.py` and `playbook/engine-families.md`.
- Implemented `tools/build_proxy_dll.py`, a pure Python generator that builds a standalone 32-bit `dinput8.dll` export-forwarder proxy DLL (forwarding `DirectInput8Create` to `C:\Windows\SysWOW64\dinput8.dll` and executing hook code on process attach).
- Booted `rerev.exe` in an isolated test copy with the fixed proxy DLL (2026-10-19): log written, the system `dinput8.dll` loaded through the forwarder, game window up, process alive and responding after 77 s.

## QA

- Raid Mode: not tested by us.
- Campaign co-op: none (gap).
- Proxy hook: manual boot check, see What we changed. The main menu was not seen (the fullscreen window was minimized), so input through DirectInput is not yet confirmed.

## How to play

### Playing Raid Mode Co-op (Available Now)
1. Complete the initial Campaign prologue to unlock Raid Mode (or access it from the main menu if already unlocked).
2. From the main menu, select **Raid Mode** -> **Online Play**.
3. Create a private session and invite your friend via the Steam overlay (Shift+Tab), or have your friend join your lobby.

### Playing Campaign Co-op
Currently not supported natively or via existing mods. Building campaign co-op requires reverse-engineering the partner AI actor and state synchronization over an external relay (e.g. Steam Spacewar AppID 480) to avoid VAC interference.

## Known issues

- Campaign co-op does not exist natively or as a public mod.
- Keep modified clients out of official Raid Mode lobbies (VAC is not listed, but other players are).

## Next steps

- Runtime check of control types and the partner's think pointer, then drive the partner from a second input with control type `3`.
- Find how actions (fire, reload, melee) reach a type-2/type-3 player.
- Network the partner's pad between two instances (Spacewar 480 or UDP).
