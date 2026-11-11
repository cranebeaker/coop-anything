# Resident Evil Revelations (2012, PC port 2013)

| | |
|---|---|
| Engine | MT Framework (`KATARIBE`, project tree `bhr\biorevhd`), Direct3D 9, x86 |
| Versions tested | PC/Steam appid **222480**, build 13298487; `rerev.exe` 14,260,576 bytes, SHA-256 `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA`; image base 0x400000, no ASLR |
| Projects | [Resident Evil Revelations Co-op](https://github.com/dolbaras/revelations-coop) 0.1.0 (MIT, a `dinput8.dll` proxy): split screen for the whole campaign, network co-op for Episode 1. Claim issue: [#299](https://github.com/cranebeaker/coop-anything/issues/299). Earlier probe `re1coop`: superseded, see [Existing work](#existing-work) |
| QA level | automated (offline suites, a headless bot and lab sessions on two PCs; nobody outside the lab has played 0.1.0) |
| Last updated | 2026-11-10 |

## Where things stand

**Two players can play the story campaign today, with a mod.** [Version 0.1.0](https://github.com/dolbaras/revelations-coop/releases/tag/v0.1.0)
(published 2026-11-10, GitHub "Latest") takes over the campaign's AI partner (Parker, Jessica, Chris and so on) with a second human:

* **Split screen on one PC, whole campaign**, two gamepads, each player with an own camera, HUD, ammunition and pause/item menu; side by side or top/bottom.
* **Network co-op, Episode 1 only** (Jill and Parker): two PCs, one person each, each with its own copy of the game and of the mod; the guest joins by
  Steam friends or by IP address (UDP 28410). Enemies are run by the host's PC, doors, key items and pickups are kept in step, game over and Continue are
  shared, downed/revive is a lobby option, both PCs show the game's own ending. Episodes 2 and later are not offered over the network.
* A **"Co-Op Play" row in the game's own main menu** (F6 or the pad's BACK button also open it), ready buttons in the room, the **partner on the minimap and
  map**, a **co-op save of its own** (a co-op game never writes the solo `DATA0.DAT`), an installer and a self-updating shortcut, 13 installer/lobby languages.
* Raid mode and the game's own online play are never touched by the mod.

**The shipped game still has no campaign co-op.** Its only two-player mode is **Raid Mode**, the second entry of the main menu, and the campaign pause menu has no
invite path. The mod does not unlock anything the game hides: the second human is the AI partner taken over, and the network layer is the mod's own.
An earlier version of this write-up (PR #126) held the opposite, see [Correction of the earlier thesis](#correction-of-the-earlier-thesis).

**Honest QA.** What was checked is listed under [QA](#qa). Short version: offline test suites and a headless bot; lab sessions on two PCs (several rounds, both role
orders, up to the end of Episode 1) and split screen driven with two virtual pads. Not yet done: any play by people outside the lab, a session between two different
networks (Steam relay), the story section 1-2 played in network co-op (a lab command skipped it), and anything in Raid.

## Existing work

| project | state |
|---|---|
| [Resident Evil Revelations Co-op](https://github.com/dolbaras/revelations-coop) 0.1.0 (MIT) | Described in this page. Release notes and checksums: [v0.1.0](https://github.com/dolbaras/revelations-coop/releases/tag/v0.1.0). Pre-releases 0.0.1 and 0.0.2 came before it. |
| `re1coop` (an earlier probe proxy, as described in PR #126; unpublished) | A diagnostic `dinput8.dll` that read the mode field and the network manager, walked the game's type registry and registered one payload type of its own. It sent no traffic and ran no session; it is not the mod above. Its measurements are kept below where they could be cross-checked. |
| Hub PR #115 (another contributor's study of the same build) | Agrees that mode 1 is Raid. Its "route B" (running the campaign inside the Raid session) was not taken; its patterns and offsets were used as reading material and checked against the exe before use. |

Searched on 2026-10-22: Steam community, GitHub, general web search turned up no other campaign co-op for the PC release. Nexus Mods and ModDB refused the fetcher
with HTTP 403, the Steam community answered 429, and Reddit and YouTube were not checked; that search is partial.

## How the game works (for co-op)

Every fact below is tagged: **ours** = checked by the mod's author, statically in the on-disk exe and/or in the running game; **earlier** = taken from the earlier
write-up (PR #126 / `re1coop` probe) and **not** reproduced by us, so treat it as a lead, not as a fact.

### The mode field and Raid

One `int32`: `mode = *(int32 *)(*(int32 *)(sGameSystems + 0x24) + 0x20)`. The global cell is at VA `0x1169108` (RVA `0xD69108`) and holds a
`sGameSystemsHolder<Game>`; `+0x24` is a `cSystemData<Game>` and `+0x20` in it is the mode (**ours**, static; the setter is `FUN_008C6960`).

* **0 in the title, the campaign menus and Episode 1** (**ours**, read in the running game, 2026-11-03).
* **1 in the Raid lobby and in Raid** (**earlier**: the probe measured 0 at the menus and 1 in Raid. We read it statically instead of entering Raid: all 16 callers of the
  setter pass the constants 0 or 1, 1 is set only by Raid menu and session code, and **the value 2 that PR #126 described is never written** - **ours**).
* What reads the mode: the two scene loaders (`FUN_0042BFC0` and `FUN_0041B710`) pick `archive\game\scene_coop\...` instead of `scene\...` when it is not 0 and keep
  `_coop` actor variants only when it is 1; `FUN_0087E900` ("in a co-op session" = play mode in {1,2} and mode 1); the second game-system instance; the result/save
  path of a Raid stage (**ours**, static). The Raid loot drops that pay out only when the mode is 1 are Raid mechanics (**ours**, static).

### Why the data looked like campaign co-op

The build does contain a lot named "coop": `game\scene_coop\...` for every episode id (23 episode folders, 81 `*_event.arc`), `coop_table\*.lvt`, `_coop` actor variants,
`cCoopSyncData::*` (64 message types), `cCoopMenu::*`, `sCoopManager`, `nNetwork::*`, `SteamNetworking005`, `ws2_32`. All of it is **Raid** (**ours**): the `scene_coop`
archives are about 5 MB against 27 MB for `scene`, hold event/arrange data with Raid names (Abyss, Deep, Dark, BPit, Host, Guest, `EventBattle`) and only 93 cutscene
(`demo\`) entries against 2176 in `scene`; the message texts (`Coop_*.gmd`) are Raid stage names and "Player Match / Private Match / Custom Match"; the code itself
calls Raid "Coop" (`cCoopMenu::cRaidMenuState`). The episode ids in `scene_coop` match the campaign's ids, which is probably what made the inference look safe (the Raid stages use the campaign's maps; not checked further).

### The network layer: Raid only

* Session: `nNetwork::Session` / `SessionDatabase` / `SessionDriver` / `Route` / `Transport` / `VoiceChat`, over `ISteamNetworking` (`SteamNetworking005`) and `WS2_32`
  (20 imports) (**ours**: classes and strings exist; what each does at run time, **earlier**).
* `sCoopManager` is the object at VA `0x1169398` (RVA `0xD69398`), created by `FUN_00880F10`, DTI size `0xE90` (**ours**). The earlier write-up called it
  `network::sNetworkManage`; a different class of that name exists, and where its instance lives is **unknown**. Fields (**ours**): `+0xF4` play mode (-1 at the menus
  in the running game; menu code writes -1, 0 and 2 and the "in session" test reads {1,2}; the earlier probe reported 0 for the Raid lobby and 1 in Raid play: **earlier**, not
  matching our static reading, unresolved), `+0xF8` player index (-1 at the menus), `+0x8F4` state 0..4. In the main menu there is **no session** (**ours** and earlier).
* **137 `*SyncData` payload classes** and 14 `cCoopMenu::*` classes (**ours**, static count; they all derive from `cRemoteCall`). Their names (Lobby, Ready, Wanted, Rush,
  UnlockStage, Result, Continue, per-player vitals, dying, enemy drop) are the Raid flow.
* The mod **does not use any of it**: it has its own transport (ENet over UDP, or Steam's P2P for the friends route), its own handshake (mod version and the first 8 bytes
  of the exe's SHA-256, a mismatch is refused with a reason) and its own messages; the game's Raid session is never started. Every module of the mod stops and
  removes its hooks while a Raid scene or session is active.
* Cross-check against PR #126's census: our static scan of the registration initializers finds **4069** type descriptors; the earlier probe reported 4071 types
  reachable from `MtObject` at run time. The two-type difference is **not reconciled** (the earlier count may include the types its own probe registered). Of its
  other numbers we confirm 137 `*SyncData` and 14 `cCoopMenu::` exactly, and 55 names containing `nNetwork::` against its 54 (**ours**; the extra one may be a substring match).

### The type registry (kept from the earlier write-up)

Every type is a static descriptor in `.bss`, built by a static initialiser that calls `FUN_008E1330` (the descriptor constructor; **ours**, matches the earlier
write-up and our own decoding):

```
+0x00  vtable           (0x102AA00 after construction)
+0x04  name             "cCoopSyncData::cZZZSyncData"
+0x08  next sibling
+0x0C  first child
+0x10  parent type
+0x14  link (hash bucket)
+0x18  size in BYTES (low 23 bits) | more bits (class id, see below)
+0x1C  hash
```

Confirmed by us: the layout up to `+0x1C`, size in bytes, the name walk `object -> vtable[4] -> "mov eax, DTI; ret"`. **Not confirmed by us** (earlier probe, one
running-game experiment): that bits 23..28 of `+0x18` are a class id selecting one of 64 class records, that the network classes are class 16 (`NETWORK`), that the id
is assigned at run time by walking the parent chain, that a payload type registered later gets instances from the NETWORK heap, and that the allocator in
class-record slot 7 is a `__thiscall` (calling it as `__cdecl` crashes). They are plausible and would matter to anyone building on the game's own replication; our mod
does not need them. Everything the earlier write-up said about "milestone 1" (registering and allocating an own payload type) describes a probe, not a feature of the mod.

### Other facts the mod is built on (ours, build 13298487)

* Players: the partner is a full `uPlayer` (`0x117B498`, size `0x1760`, parent `uNpc`), 32 `uPl<...>` character classes. Per unit: role `+0xE30`, control type
  `[[unit+0xE80]+0x38]+0x34` (**1** local pad, **2** AI partner, **3** remote); `sPlayer` keeps four circular unit lists; Episode 1 starts as `uPl<Jill>` (type 1) and
  `uPl<ParkerLight>` (type 2). Verified in the running game on 2026-11-03.
* Pads: `sPad` owns two `Pad` slots and polls XInput users 0 and 1, but every reader indexes the pad by one "current slot" (`sGamePad+0x870`), so a second pad does
  nothing by itself; the mod swaps the slot per unit. The five `XInputGetState` call sites (imports by ordinal) are `0x9BDB4F`, `0x9BEFF6`, `0x9C0423`, `0x9C0555`, `0x9C0692`.
* The "own slot" (`FUN_00882D60` / `FUN_0087E1B0`, about 190 callers) is 0 offline and decides which unit the engine treats as the human (HUD, doors, events); the mod sets it for the guest's PC.
* Camera: `[0x11E00B8]` is `sGameCamera`; `[root+0xCE0]` points to the `uCameraManage`, `+0x74/+0x78` hold the target unit. The engine already has 8 `sCamera::Viewport`
  objects; the split screen switches a second viewport on and builds a second cockpit (HUD) unit set with the engine's own calls.
* The `dinput8.dll` import is by name, so the proxy's export must be undecorated (`DirectInput8Create`, not `_DirectInput8Create@20`); the repo pins it with a `.def` file.
  Two start-up crashes found on the way (a proxy linked without `/guard:cf`; the CRT stream layer in the logger) are fixed and covered by tests.
* The campaign's save is `DATA0.DAT` (`BioSaveData_Win.cpp`); the mode also picks a save slot in the Raid path, which is not used. The mod keeps its own file
  (`COOP0.DAT` in `%LOCALAPPDATA%\CAPCOM\RESIDENT EVIL REVELATIONS\rev1_coop_save`).
* Raid's dying/agony state (`cPlayerDyingSyncData`, applier `0x6B6DC0`) has **no motions in the campaign** (driving it offline put Parker in the T-pose), so the mod's
  downed pose is built from the campaign's knock-down and get-up states instead.

## What we changed

The mod, by what a player sees (code and design notes in the [repository](https://github.com/dolbaras/revelations-coop)):

* **Second human on one PC:** the campaign's AI partner is taken over by a second pad or by a remote input record (aim, fire, reload, melee, weapon change, menus).
* **Split screen:** two cameras and two HUDs through the engine's own viewports; cutscenes, pause and game over use the whole screen; checkpoint reloads and the game's
  own Exit survive.
* **Co-Op Play row, lobby and room:** a fourth main-menu row in the game's own style and fonts, with its text in the game's 13 languages; character choice, layout,
  gamepad choice, host/join, Steam friends or IP, ready buttons, downed/revive options.
* **Network, Episode 1:** the guest's character is driven from the network (pad record plus position correction); both PCs wait for each other at every load and cutscene
  end; the host runs the enemies and the guest's shots count; doors, key items, pickups and story flags are mirrored; the sections 1-1, 1-2, 1-3 follow each other and
  the session closes at the end of Episode 1; where the story has Jill alone the other player watches her.
* **Co-op save of its own**, with the guest's weapons taken from the guest's own co-op save and Continue restoring the save's characters.
* **Gamepad routing** for virtual, duplicated or unplugged pads; **partner on the minimap/map**; **achievements** credited to the PC whose player earned them.
* **Other mods:** an Ultimate ASI Loader the player had as `dinput8.dll` keeps running as `winmm.dll`; a foreign `dinput8.dll` is kept and restored.
* **One-click log zip** (`coop\Collect logs.bat`) with names, paths, e-mail, MAC and public IP addresses removed.

## QA

* **Offline suites:** C unit/integration suites built with `sh tests/build.sh` (lobby, pad routing, downed/revive state machines and pose, network core, guards, world
  mirroring, save, installer) and a headless bot (`tests/coop_bot.c`, with a network emulator and a Steam mock) for the protocol. They were run before the release; the final
  pass counts are not quoted here (rerun with `tests/build.sh`).
* **Lab sessions, two PCs, real game copies** (one copy per PC): host and guest in both role orders (host as Jill, host as Parker): lobby -> New Game -> 1-1 -> 1-2 ->
  1-3 -> the end of Episode 1 on both PCs, with a Continue from the title at the start of 1-3; a game over and its Continue; the guest's real shots killing the host's
  enemies; the guest leaving the enemy's area in a fight (guest as Jill): the enemy stayed on the guest's screen and took the guest's hits; the solo save byte for byte
  unchanged after every co-op session. Join through Steam friends ("Join Game", invite) on two PCs of one network, and by IP address.
* **Split screen on one PC**, two virtual Xbox 360 pads: New Game, game over with Yes and No, the title's Continue of the co-op save, on the co-op save, solo save unchanged.
* **Installer, uninstaller and the self-update** from the published pre-release 0.0.2 to the 0.1.0 package (offline, against a local server) and a checksum-checked download of 0.0.2.
* **What these were not:** parts of the route were driven by lab aids (teleports and scripted walking, a no-damage mode, scripted aiming, extra ammunition), and **section 1-2
  was skipped by a lab command**, so its own gameplay has not been played in a network session. No human playthrough, nobody outside the lab has played 0.1.0, no session
  between two different networks, no Episode 2 or later over the network, Raid never touched. That is why the level is `automated`, not `verified-playthrough`.

## How to play

Download `revelations-coop-setup-0.1.0.exe` (or the zip and `Install Co-op.bat`) from the [release](https://github.com/dolbaras/revelations-coop/releases/tag/v0.1.0),
on both PCs for network play. The installer finds the Steam copy (only build 13298487 is accepted), adds the mod and a "Resident Evil Revelations Co-op" shortcut; play
from the shortcut and choose **Co-Op Play** in the main menu. For network play the host picks "Host", the other player joins from the Steam friends list or types the
host's IP address (UDP 28410), both mark themselves ready, and the host starts New Game or Continue. For split screen switch co-op on in the lobby, pick the layout and
start the story as usual with two gamepads. The mod adds only `dinput8.dll` and a `coop` folder; "Uninstall Co-op.bat" puts everything back. Setup exe and zip are not
code-signed (SmartScreen may warn). Bug reports: an issue in the mod's repository with the zip from `coop\Collect logs.bat` (both PCs for a network game).

## Known issues

* Network play is Episode 1 only; split screen and network are not used together; joining a running network game is not possible (start a new co-op game, Continue picks up the checkpoint).
* While co-op is on, the Campaign page's save panel can still show the solo save's play time and chapter; Continue loads the co-op save.
* In a network game the partner's character on your screen can hold other guns than the partner really carries (the DLC pistols are never shown on it); hits are always made with the partner's own weapon on the partner's PC.
* Not yet tried between two different networks; not yet played by people outside the lab.

## Correction of the earlier thesis

The first notes of PR #126 (merged 2026-11-02; `NOTES.md` keeps the whole story, including the later correction in its Session 6) carried a central claim that the PC build ships a
full **campaign** co-op netcode behind the mode field (1 = dormant campaign co-op, 2 = Raid). That thesis is **false**, and the earlier session itself withdrew it on
2026-10-22 after running the game; this section states it explicitly so nobody builds on it. The evidence:

1. **The mode field in the running game.** It is 0 in the title, the campaign menus and Episode 1 (we read it in the game on 2026-11-03; the earlier session itself withdrew
   the thesis for the same reason on 2026-10-22) and 1 in Raid. Static reading: the value 2 is never written, and 1 is written only by Raid menu and session code.
2. **The network layer is used by Raid only.** Session classes, 137 sync payloads, `sCoopManager` and the `cCoopMenu` states are the Raid lobby, wave, result and unlock flow
   (Section "The network layer: Raid only"). In a campaign chapter the game has no session at all.
3. **What the `scene_coop` data is.** Raid stage and wave data, not campaign chapters (5 MB against 27 MB, Raid names, 93 against 2176 cutscene entries).
4. **A campaign co-op had to be built, and was.** The mod above uses its own transport and the AI-partner takeover, and two real PCs ran a host and a guest
   campaign session through Episode 1 in the lab; no switch in the game was involved.

The earlier plan to start a session in the campaign with the game's own `SessionDriver` was not followed. The earlier statement that two-client testing needs a second
Steam account and that the game's DRM stub kills a second local instance applies to one PC; the lab uses two PCs. Technical facts from the earlier write-up that
we could not confirm are marked in "How the game works". The offsets we cross-checked matched; the one thing we found wrong there is the name `sNetworkManage` for the object at `0x1169398` (it is `sCoopManager`).

## Next steps

1. **Human playtest**, including a first network play of section 1-2 and the guest crossing zones mid-fight as Parker; reports welcome as issues.
2. **Steam relay** between two different networks.
3. **Network co-op beyond Episode 1** (the split screen already covers every episode; the story sync is the limit).
4. **Downed/revive in split screen** (0.1.0 offers it in network games through the lobby; a local variant exists behind an environment switch, off by default).
