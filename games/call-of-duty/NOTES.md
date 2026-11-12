# Call of Duty (2003) + United Offensive: co-op notes

## Game identity

| | Value |
|---|---|
| CoD version | 1.5 (`version.inf` ExtVersion=1.5, IntVersion=5.3) |
| UO version | 1.51 (`version.inf` ExtVersion=1.51, IntVersion=12.4) |
| Engine | IW engine 1 (id Tech 3 derived); `detect_engine`: "id Tech 3 (or derived)" |
| Language | French localized paks (`localized_french_pak*.pk3`) |

| File | PE timestamp |
|---|---|
| CoDSP.exe | 1078214618 (2004-03-02) |
| CoDUOSP.exe | 1093030896 (2004-08-20) |
| gamex86.dll (SP game) | 1078214576 |
| uo_gamex86.dll (UO SP game) | 1093030666 |

All 32-bit PE32. A native module must still check its target build before patching; derive that
check against the player's own copy at build time rather than shipping a redistribution's hashes.

### Observations
- `CoDUOSP.exe` carries sections `stxt774`, `stxt371` (SafeDisc section names). `CoDSP.exe` does not. Unknown whether code is still encrypted.
- SP game logic is a separate DLL (`gamex86.dll`, `uo_gamex86.dll`), like Quake 3's qagame. Client game `cgamex86.dll`, UI `uix86.dll`.
- PunkBuster files in `pb/` (MP only). Stay away from it.

## Design goals for the mod
- CoD1 campaign first, then port to UO. 2 players first, design for 4.
- Networking: Steam Spacewar (480) relay by default, direct IP + UPnP fallback.
- Death: downed + revive (WaW-like) if possible; fallback respawn on teammate after 5 s.
- Progression: host drives, auto next mission, host level-select menu. No save/resume needed.
- Difficulty scaling: optional bonus, low priority.
- Language: French first; support any localized paks, or report why not.
- Vehicle missions: try shared roles (driver/gunner, chosen in a menu), fallback spectator; decide per mission.
- Launch: in-game menu ideal, launcher .exe fallback; clear diagnostics of what failed.

## Log

### 2026-10-23 Phase 0-2: triage, workspace
- Cloned coop-anything at 5b86e5b, branch `game/call-of-duty`. MCP tests: 13 OK (`python -m unittest tools/mcp/test_coop_mcp.py`; with `-I` the import fails, cosmetic).
- Test install = hardlinked copy of the player's install (configs copied for real).
- No CoD1/UO issue or PR in coop-anything (`gh issue list --search "call of duty"`: only MW 2019, MW3, MW2CR claims).

### 2026-10-23 Phase 1: research

#### Found (with sources)
| Find | What it really is | License | Last activity | Use for us |
|---|---|---|---|---|
| "CoD 1 Cooperative Mod" | Only a name in a 2025 Steam reply, no link, no author. Not found by searches on ModDB, GitHub, Russian web. | unknown | unknown | none until a build is found ([thread](https://steamcommunity.com/discussions/forum/0/2284960483102477437/)) |
| CoD1 Splitscreen PC v1.1 (ModDB) | 318-byte zip, MP splitscreen launcher, not campaign | none stated | 2024-05 | none ([page](https://www.moddb.com/downloads/call-of-duty-1-splitscreen-pc-v11)) |
| Workshop 2327804851 | "Call of Duty 2 - Deluxe Edition" mod for **Men of War: Assault Squad 2**, not CoD | n/a | 2025 | none ([page](https://steamcommunity.com/sharedfiles/filedetails/?id=2327804851)) |
| [Clippy95/CoDUO-YAP](https://github.com/Clippy95/CoDUO-YAP) | UO SP+MP plugin (ddraw.dll proxy or ASI), SafetyHook/MinHook, cvars, widescreen, hooks into SP game. FAQ: does NOT support CoD1 (2003). | GPL-3.0 | 2026-10-21 | UO: loader + hook framework + known SP addresses. Reference for CoD1 hooking style. |
| [opencoduo/coduomp](https://github.com/opencoduo/coduomp) | Reconstructed source of UO **MP** client, server engine, server game, cgame, ui. 2485 files, no AI/actor code. | **none stated** (no LICENSE file) | 2026-10-18 | Read-only reference for engine structs (netchan, snapshot, server). Can't fork without author OK. |
| [cod1raph/iw1x-client-r](https://github.com/cod1raph/iw1x-client-r), [xtnded/codextended-client](https://github.com/xtnded/codextended-client), [xtnded/iw1x-server](https://github.com/xtnded/iw1x-server), [Wolf-Pack-Clan/t1x-client](https://github.com/Wolf-Pack-Clan/t1x-client) | CoD1 **1.1** MP client/server extensions. Our install is 1.5. | iw1x: none stated; codextended: GPL-3.0 | 2026 | MP 1.1 only; reference for hooking style, not addresses |
| [kungfooman/libcod](https://github.com/kungfooman/libcod) | MP server extension CoD1/2/4 | see repo | 2026-07 | MP only |
| [United Fronts](https://www.moddb.com/mods/call-of-duty-united-fronts), Singleplayer Improved | SP-only campaign mods | | | none for net |
| [games/modern-warfare-2](../modern-warfare-2/README.md) | Same engine family (IW4): co-op = SP netcode + script work | | | design reference: script patching of `level.player` |

#### Observations (binaries, this install)
- `CoDSP.exe` / `CoDUOSP.exe` import no WSOCK32/WS2_32; `CoDMP.exe` imports `WSOCK32.dll`. SP net layer still has `loopback`, `net_qport`, `NET_CompareAdr` strings: Q3 client/server split with netchan over loopback only. (Python string scan, scratch script.)
- SP game DLL `gamex86.dll` has `G_LoadGame: clientnum out of range`, `getentarray`, script builtins.
- Level scripts are plain-text GSC inside pk3 (zip): CoD1 809 `.gsc`, 1210 `level.player` refs (top: `_loadout` 160, `stalingrad` 119, `dawnville` 106); UO 315 `.gsc`, 1916 refs.

#### Derived (not verified)
- Q3 heritage: the SP exe likely still runs a server + client in one process over loopback. Adding a real transport (UDP / Steam P2P) under the NET layer from an injected DLL could let a second CoDSP.exe join as a pure client. Server side then needs >1 client slot in the engine and in `gamex86.dll`.
- Script co-op: override pk3 (`zzz_coop.pk3`) can replace `maps/*.gsc`; but rule 7 = do not ship game scripts, so ship patches applied to the player's own pk3 at install.

#### Unknown
- Whether the SP server code (SV_*, client array) survived with >1 slot or was cut to 1.
- Whether `gamex86.dll` SP handles client 1 (spawn, playerstate, AI targeting of non-`level.player`).
- Whether `CoDUOSP.exe` code is still SafeDisc-encrypted (sections `stxt774`, `stxt371` present).
- Whether "CoD 1 Cooperative Mod" exists as a build anywhere.

### 2026-10-23 Phase 3: reverse engineering (Ghidra 12.1.2 headless)

Setup: project `<workspace>/ghidra/CoD.gpr` (CoDSP.exe, gamex86.dll, CoDMP.exe; default auto-analysis, ~2 min each).
Query scripts in `<workspace>/ghidra/scripts/` (StrXrefDecomp, DecompAt, RefCount). Call `analyzeHeadless` from
PowerShell through a junction without spaces: cmd splits arguments on `,` and `|`. All addresses below are for the
hashes in "Game identity". Function names are mine, given from strings and Quake 3 source layout.

#### Observations: CoDSP.exe (image base 0x400000)
- `SV_Startup` 0x44be90: allocates `svs.clients` = `0x2e1f0` bytes = **4 x client_t (0xb87c)**. Pointer at `0x152e8e8`.
- `SV_GetUsercmd` 0x449a10: errors if clientNum < 0 or > 3.
- `SV_ReadPackets` 0x44ccb0: loops over the 4 clients and matches address + qport (`NET_CompareBaseAdr` 0x43efb0), then `Netchan_Process` 0x43eea0 and `SV_ExecuteClientMessage` 0x4491b0. Connectionless packets (seq -1) go to 0x44cc00.
- `SV_ConnectionlessPacket` 0x44cc00: recognises one command only (string compare), else prints "bad connectionless packet".
- `SVC_DirectConnect` 0x448d70: **zeroes all 4 client slots** and sets up slot 0 only: state, netchan with the sender's netadr, buffers `Z_Malloc(0x18000)`, `0x400` x2.
- `NET_SendPacket` 0x43f290: stripped to a loopback copy into the other socket's buffer (`0x14f07a0 + (sock^1)*0x200c`). It ignores the address type, has **one packet slot per direction**, and has no sendto. No WSOCK32 import.
- `NET_AdrToString` 0x43f030: still formats `NA_LOOPBACK` (2), `NA_IP` (4) `%i.%i.%i.%i:%i` and IPX. `ntohs` is called through a pointer (`0x8cc71c`).
- `Netchan_Transmit` 0x43ed50: max 0x4000 bytes, MAX_PACKETLEN 0x1f9b.
- Client side is intact: `CL_ParseServerMessage`, `svc_gamestate`/`svc_snapshot`/`svc_serverCommand`, and "%s:sequenced packet without connection" in `CL_PacketEvent` 0x40dec0.
- `CoDUOSP.exe`: entry point in `.text`, `.text` entropy 6.75 (CoDSP: 6.71). Code is not encrypted; `stxt774`/`stxt371` are inert SafeDisc leftovers in this build.

#### Observations: gamex86.dll (image base 0x20000000, exports `dllEntry`, `vmMain`)
- `vmMain` 0x2001cd70, Q3 order: 2 = ClientConnect(n) 0x20014800, 3 = ClientBegin 0x200148d0 (clientNum in EAX, missed by the decompiler), 4 = ClientDisconnect 0x20014ca0, 5 = ClientCommand 0x20017ff0, 6 = ClientThink 0x200129c0, 7 = map/spawn init 0x2001ed30.
- `ClientConnect` is per client: `gclient_t` (0x74e8) at `level.clients` (`0x202db220`) + n*0x74e8, gentity n (0x2c8 each, `g_entities` around 0x2012fa80), and **a sentient per player** (AI perception, "No sentient for player").
- `G_LoadGame` 0x2002d980 serialises a client list, bound check `clientnum > 4`.
- ~~Player singleton `0x202db224`~~ **Corrected in pass 2:** `0x202db224` is the `g_entities` pointer (accessed as `idx*0x2c8 + base`), not `level.player`.

#### Derived (not verified at runtime)
- The SP engine keeps the 4-slot Quake 3 server. What is missing: (1) a real transport under `NET_SendPacket` / the loopback receive, (2) a connect path for slots 1-3 (`SVC_DirectConnect` is hard-wired to slot 0 and wipes the others), (3) game fixes where code assumes one player: few in native code (pass 2), mostly in scripts.
- So: native-code co-op on the existing server/client netcode (host-authoritative, Q3 snapshots), not lockstep. Player 2 runs a normal CoDSP.exe as a pure client.
- Plan sketch: a hook DLL (ddraw/winmm proxy like CoDUO-YAP) that (a) replaces `NET_SendPacket` and the client/server receive with a queue keyed by netadr: loopback for the local pair, UDP or Steam P2P for remote; (b) adds a `SVC_DirectConnect` variant that fills a free slot 1..3 without zeroing the others; (c) on the client, a `connect <host>` path that sends the OOB connect to a remote netadr.

#### Unknown (next to check)
- Whether `SV_SendClientMessages`, snapshot building and `SV_Frame` loop all 4 clients or only slot 0.
- Whether the cgame/client copes with clientNum != 0 (`cg.clientNum`, playerstate index).
- The AI functions behind the 32 `level.player` refs: targeting, friendly-fire, "player seen" triggers.
- Same checks for CoDUOSP.exe / uo_gamex86.dll.

### 2026-10-24 Phase 3, pass 2

#### Observations: CoDSP.exe server send path
- 19 functions use `svs.clients` (`0x152e8e8`). Most loop over all 4 slots (`while (off < 0x2e1f0)` step 0xb87c, or `+0x2e1f` dwords): configstring broadcast 0x44b900, server commands `SV_SendServerCommand` 0x44caf0, 0x44b710, 0x44c0b0, map spawn 0x44c1b0.
- **`SV_SendClientMessages` 0x44da50 sends to client 0 only**: `MOV ECX,[0x152e8e8]` then `CALL 0x44d960`, no loop.
- `SV_SendClientSnapshot` 0x44d960 (client in ECX) and `SV_BuildClientSnapshot` 0x44d800 (client in EAX) are generic. Client index = `(cl - svs.clients)/0xb87c`, playerstate = `gameClients + idx*gameClientSize` (`0x152bc64`, `0x152bc68`), `ps.clientNum` at ps+0x9c checked `< 0x800`.
- `SV_Frame` 0x44cf10 runs the game at 20 Hz (50 ms steps), then `SV_SendClientMessages` once per frame.

#### Observations: cgamex86.dll (base 0x30000000, exports dllEntry/vmMain)
- `CG_Init` 0x3001af80 takes no clientNum (Q3 MP passes one); it checks game name "cod-sp". The cgame reads snapshots from the engine (`CG_ReadNextSnapshot` 0x30022380, `CG_ProcessSnapshots` 0x30022460). The playerstate in each snapshot belongs to the receiving client (server side, above).
- Not found statically: whether cgame treats entity 0 as "me" (hides its body, uses its entity for the view). Searching for ps.clientNum offsets was too noisy.

#### Observations: gamex86.dll player assumptions
- `g_entities` static array at `0x2012fa80` (0x2c8 each), `ent->client` at +0x104; `level.num_entities` at `0x202db22c`.
- `G_FreeEntity` 0x20046200 refuses entity numbers 0..3: **MAX_CLIENTS = 4** in the game DLL as well.
- The 4 "AI" functions from pass 1 (0x2006b020, 0x2006dbb0, 0x2006f320, 0x2006fad0) loop over all entities. No player assumption.
- Direct, non-indexed entity-0 accesses (`[0x2012fb84]` = `g_entities[0].client`, or `MOV reg,0x2012fa80`): **10 functions, 13 instructions** (0x2001df70, 0x2001e110, 0x2001e940, 0x2001f0c0, 0x2002c1b0, 0x2002d980, 0x2002ede0, 0x20045c70, 0x20046090, 0x2004d8b0). Several are loop starts. Also 0x20070f40 reads `g_entities[0].client` origin for a radius test (likely spawn or cull distance around the player).
- Scripts set the player with `getent("player", "classname")`: `maps/_load.gsc:49-50` (`level.player = player`), `animscripts/utility.gsc:570` (`anim.player`), and per-map copies (stalingrad, chateau, us_intro...). `getent` errors when more than one entity matches ("getent used with more than one entity" in gamex86.dll).

#### Derived
- Server fix: replace `SV_SendClientMessages` with a loop over active slots. Snapshot code is already per client.
- Native game code is mostly multi-client. The single-player assumption is mostly in GSC.
- Low-risk first step: give clients 1..3 a different classname (e.g. `player_coop`) so `getent("player","classname")` keeps returning the host. Scripts then run unchanged with the host as `level.player` (World at War does the same with its host). Player 2 is then patched in, script by script (triggers, `activator` checks, objectives). AI uses sentients (one per client), so enemies should already target player 2. Needs a runtime check.

#### Unknown (needs the prototype)
- cgame with a remote client: does player 2 see their own body or the host's body hidden?
- What 0x20070f40 culls around player 0 (AI spawn distance? sound?).
- Where the game DLL sets classname "player" (ClientSpawn) to hook it per client.

### 2026-10-24 Script (GSC) analysis, CoD1 campaign

Method: Python scan of the `.gsc` files inside the player's `main/*.pk3` (zip, read only), counting patterns per map
(all `maps/<map>*.gsc` files). Counts only; no game script text is copied here. The line-level review of vehicle
sequences was not done, so the vehicle classification below is coarse.

#### Campaign order (observed: `missionsuccess` / `changelevel` targets in each map's scripts)
training → us_intro (briefing) → pathfinder → burnville → dawnville → carride → brecourt → us_mid (briefing) → chateau →
powcamp → uk_6ab (briefing) → pegasusnight → pegasusday → uk_sas (briefing) → dam → truckride → airfield → ship →
ru_stalingrad (briefing) → stalingrad → redsquare → trainstation → sewer → pavlov → factory → railyard →
tankdrivecountry → tankdrivetown → allied_start (briefing) → hurtgen → rocket → berlin. `credits` is separate.
Briefing maps have 0 `level.player` refs and call `getent("player","classname")` once.

#### Per-map counts (observed)
| Map | Script lines | `level.player` | trigger waits | player compares | weapon calls | stance calls | health/damage | follow player | vehicle words | turret words | objective calls |
|---|---|---|---|---|---|---|---|---|---|---|---|
| training | 2117 | 19 | 28 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 34 |
| pathfinder | 2969 | 33 | 25 | 0 | 5 | 8 | 1 | 6 | 32 | 1 | 15 |
| burnville | 2773 | 65 | 30 | 2 | 0 | 0 | 6 | 6 | 7 | 25 | 11 |
| dawnville | 4582 | 106 | 38 | 3 | 3 | 4 | 11 | 26 | 22 | 28 | 45 |
| carride | 2457 | 61 | 26 | 0 | 3 | 12 | 6 | 2 | 76 | 13 | 13 |
| brecourt | 2224 | 32 | 19 | 2 | 0 | 0 | 2 | 13 | 0 | 3 | 24 |
| chateau | 1982 | 19 | 20 | 0 | 0 | 0 | 1 | 11 | 9 | 0 | 17 |
| powcamp | 876 | 7 | 9 | 0 | 0 | 2 | 0 | 1 | 9 | 0 | 10 |
| pegasusnight | 3444 | 76 | 8 | 2 | 8 | 12 | 12 | 2 | 19 | 29 | 23 |
| pegasusday | 3574 | 43 | 1 | 3 | 0 | 7 | 8 | 2 | 34 | 20 | 27 |
| dam | 986 | 9 | 21 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 5 |
| truckride | 1381 | 28 | 21 | 0 | 10 | 0 | 0 | 0 | 41 | 0 | 8 |
| airfield | 1101 | 26 | 8 | 0 | 7 | 0 | 2 | 0 | 77 | 4 | 7 |
| ship | 1240 | 5 | 14 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 29 |
| stalingrad | 15532 | 119 | 40 | 3 | 3 | 4 | 13 | 0 | 70 | 74 | 12 |
| redsquare | 3140 | 54 | 16 | 0 | 0 | 5 | 2 | 3 | 4 | 16 | 17 |
| trainstation | 132 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| sewer | 335 | 4 | 7 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 3 |
| pavlov | 4035 | 72 | 6 | 0 | 0 | 0 | 13 | 11 | 102 | 22 | 44 |
| factory | 950 | 7 | 12 | 0 | 0 | 0 | 0 | 3 | 6 | 0 | 3 |
| railyard | 467 | 10 | 7 | 1 | 2 | 0 | 0 | 1 | 4 | 0 | 5 |
| tankdrivecountry | 343 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 6 | 0 | 3 |
| tankdrivetown | 372 | 0 | 6 | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 15 |
| hurtgen | 1008 | 17 | 7 | 0 | 0 | 0 | 2 | 3 | 6 | 19 | 5 |
| rocket | 588 | 12 | 11 | 0 | 0 | 0 | 0 | 2 | 13 | 0 | 10 |
| berlin | 1090 | 23 | 13 | 0 | 0 | 0 | 0 | 3 | 12 | 12 | 9 |

Pattern words are counted with simple regexes ("vehicle words" matches any line with vehicle-related builtins or the
word "vehicle"), so treat them as a size indicator, not exact usage.

Shared library scripts with the most player refs: `_loadout` 160, `_load` 53, `_spawner` 24, `_mg42` 21,
`_truckridewaters` 18, `_shellshock` 15, `_mortar` 9, `_tankdrive` 7, `_utility` 7, `_tankmg42` 5, `_vehiclechase` 5,
`_autosave` 4. `animscripts/utility.gsc` keeps its own `anim.player`.

#### Mission types (derived from the vehicle libraries each map calls; coarse)
- **Player drives a tank:** tankdrivecountry, tankdrivetown (`_tankdrive`); railyard also calls `_tankdrive`.
- **Vehicle chase / ride:** carride, truckride, airfield (`_vehiclechase`, `_truckridewaters`); dam (`_truckridewaters`).
- **Boat:** ship; the stalingrad crossing (stalingrad is also the longest script, 15.5k lines).
- **Flak / anti-aircraft:** pegasusnight, pegasusday, brecourt, hurtgen, tankdrivecountry, tankdrivetown (`_flak`).
- **Infantry, fixed MGs (`_mg42`, `_scripted`):** burnville, brecourt, stalingrad.
- **Mostly plain infantry:** chateau, powcamp, trainstation, sewer, factory, rocket, berlin, hurtgen, redsquare, pavlov, dawnville (these last three still have tanks and AI turrets around the player).

#### Plan for player 2 (derived; nothing implemented or tested)
1. **Keep the host as `level.player`.** Shared scripts (`_load`, `_loadout`, `animscripts/utility`) and per-map
   `getent("player","classname")` calls expect exactly one match. The other players need another classname.
   Scripts then run as in single player, with the host as the mission's player, the way World at War's co-op uses the host.
2. **Shared library layer first** (one change covers every map): `_loadout` (player 2 needs the same weapons and
   ammo), `_load` (spawn of player 2 next to the host, autosave and checkpoint behaviour), `_spawner`/`_mg42`
   (AI targeting and turret use around "the" player), `_shellshock` (effects only on the player who was hit),
   `_autosave` (host decides).
3. **Triggers:** 400+ `"trigger"` waits across the campaign. Where a script waits for `level.player` touching a
   trigger, any player should count. Order: dawnville (38 waits, 16 `istouching` checks),
   stalingrad (40), burnville (30), training (28), carride (26), pathfinder (25).
4. **Friendly AI following the player** (`setgoalentity(level.player)`, friendly chains): dawnville 26, brecourt
   13, chateau 11, pavlov 11. Keep the host as leader at first; later follow the nearest player.
5. **Death and revive:** downed state plus revive as the target, respawn next to the teammate after 5 s as the
   fallback (design choice). Both down = reload last checkpoint. Needs `_load`/`_autosave` changes plus a per-player health hook.
6. **Vehicle missions:** spectator for player 2 first (the fallback). Then try shared roles where the vehicle
   has a second seat or gun. Decide per mission: tankdrivecountry, tankdrivetown, railyard,
   carride, truckride, airfield, dam, ship, stalingrad crossing.
7. **Effort order (smallest first) for a first playable run:** trainstation, sewer, factory, powcamp, rocket, chateau,
   then the large ones (dawnville, pavlov, stalingrad, pegasusnight).

#### Unknown
- How each vehicle sequence attaches the player: line-level review not done.
- Whether the scripts or the engine refuse or misbehave with a second client: needs the native part, which is not done.
- UO (`uo/*.pk3`, 315 scripts, 1916 `level.player` refs): not analysed per map yet.
