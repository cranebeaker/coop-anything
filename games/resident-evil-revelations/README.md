# Resident Evil Revelations (2012, PC port 2013)

| | |
|---|---|
| Engine | MT Framework (`KATARIBE`, project tree `bhr\biorevhd`), Direct3D 9, x86 |
| Versions tested | PC/Steam appid **222480**, build 13298487; `rerev.exe` 14,260,576 bytes, SHA-256 `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA`; image base 0x400000, no ASLR |
| Projects | [Resident Evil Revelations Co-op](https://github.com/dolbaras/revelations-coop) (MIT, a `dinput8.dll` proxy); version [0.2.0](https://github.com/dolbaras/revelations-coop/releases/tag/v0.2.0), published 2026-11-12, GitHub "Latest": split screen for the whole campaign, network co-op section by section through the whole story (0.1.0 of 2026-11-10 had network co-op for Episode 1 only). Claim issue: [#299](https://github.com/cranebeaker/coop-anything/issues/299). Earlier probe `re1coop`: superseded, see [Existing work](#existing-work) |
| QA level | automated (offline suites, a headless bot, an installer suite and scripted sessions with real game clients on two PCs of one LAN, plus split screen on one PC); no human playthrough, 0.2.0 was not played by people before its release |
| Last updated | 2026-11-12 (0.2.0) |

## Where things stand

**Two players can play the story campaign today, with a mod.** [Version 0.2.0](https://github.com/dolbaras/revelations-coop/releases/tag/v0.2.0)
(published 2026-11-12, GitHub "Latest") takes over the campaign's AI partner (Parker, Jessica, Chris and so on) with a second human:

* **Split screen on one PC, whole campaign**, two gamepads, each player with an own camera, HUD, ammunition and pause/item menu; side by side or top/bottom.
* **Network co-op, the whole story, section by section**: two PCs, one person each, each with its own copy of the game and of the mod; the guest joins by
  Steam friends or by IP address (UDP 28410). The guest's game follows every section and episode change of the host with the pair each part gives (Jill and
  Parker, Chris and Jessica, Keith and Quint ...), from Episode 1 into Episode 2 inside one co-op game and on to the end of the campaign, where both PCs show
  the game's own end. Enemies are run by the host's PC (a kill by either player counts on both PCs), doors, key items and pickups are kept in step, game over
  and Continue are shared, downed/revive is a lobby option. 0.1.0 (2026-11-10) offered Episode 1 only.
* **New in 0.2.0** besides the whole story: **Give to partner** in the game's own item screen (as in Resident Evil 5), the **Genesis scanner for the second
  player** too, **HUNK** as the second player in 2-2 ("8:32 P.M. Guest Cabin"), where the story locks the partner away, and a split-screen fix: the second
  player can aim while player 1 has no gun in hand. See [What 0.2.0 adds](#what-020-adds).
* A **"Co-Op Play" row in the game's own main menu** (F6 or the pad's BACK button also open it), ready buttons in the room, the **partner on the minimap and
  map**, a **co-op save of its own** (a co-op game never writes the solo `DATA0.DAT`), an installer and a self-updating shortcut, 13 installer/lobby languages.
* Raid mode and the game's own online play are never touched by the mod.

**The shipped game still has no campaign co-op.** Its only two-player mode is **Raid Mode**, the second entry of the main menu, and the campaign pause menu has no
invite path. The mod does not unlock anything the game hides: the second human is the AI partner taken over, and the network layer is the mod's own.
An earlier version of this write-up (PR #126) held the opposite, see [Correction of the earlier thesis](#correction-of-the-earlier-thesis).

**Honest QA.** What was checked is listed under [QA](#qa). Short version: offline test suites, a headless bot and an installer suite; scripted sessions with real
game clients on two PCs of one LAN (every section from 1-2 to 12-2 for arrival, characters and movement, the story's own transitions between parts and episodes, the
end of the campaign) and split screen driven with two virtual pads. Not yet done: any play by people, a session between two different networks (Steam relay), the
last boss fight (12-3) played the story's way, 2-2's own story with HUNK, the fights of most sections, and anything in Raid.

## Existing work

| project | state |
|---|---|
| [Resident Evil Revelations Co-op](https://github.com/dolbaras/revelations-coop) 0.2.0 (MIT) | Described in this page. Release notes and checksums: [v0.2.0](https://github.com/dolbaras/revelations-coop/releases/tag/v0.2.0). Before it: the first full release 0.1.0 ([v0.1.0](https://github.com/dolbaras/revelations-coop/releases/tag/v0.1.0), 2026-11-10, network co-op for Episode 1 only) and the pre-releases 0.0.1 and 0.0.2. |
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

### What 0.2.0 adds

All of it **ours**, build 13298487 (static reading of the exe plus the running game; the code and its comments are in the repository's `native/`).

* **The whole story over the network.** The host's own section change (the stock `cEpisodeEndState` calls the start-record function `FUN_00720540` with the next
  area) sends the next section's start triple (the area id and the two checkpoint ids of the game's start table); the guest's PC follows it with the stock Continue
  call `FUN_00720870`, through the same hook that restarts both PCs at the host's checkpoint after a shared game over. 0.1.0 accepted only Episode 1's sections;
  0.2.0 accepts every campaign section that starts with two playable characters (27, from 1-1 to 12-3; `native/secids1.h`). The end of Episode 1 is no longer the
  end of the session: both PCs go on into Episode 2. The end of co-op is the section change after 12-3, which leads to an area that is no section: each PC tells
  the other and closes the session after the game's own end and credits. No message layout changed (`CP_PROTO_VERSION` stays 3); the network hello carries the
  mod version, so a 0.1.0 PC is refused with a reason before any section message is sent.
* **Kills far from the other PC.** An enemy the host does not run (the host's engine put it to sleep outside the zone of the host's human, or the host has no unit
  for it) runs on the guest's own AI while it is near the guest's human; the guest's PC reports its death once and the host marks its record used up, so the
  host's story counts it dead too (`native/local1.h`).
* **Give to partner** (`native/give1.c`, `give1.h`). On a herb, grenade or ammunition cell of the stock item screen (BACK) the mod adds "Give to partner" while the
  partner is up and within 3 m on the same floor: one herb or grenade per press, half of that ammunition (rounded up). Only counts move (the bag's herb count,
  grenade counts, ammunition counts); a weapon object is never moved between bags (a test tool that put one weapon object in two bags crashed the next reload),
  and a grenade type the receiver has none of gets a new unit of his own through the stock add on the receiver's PC. In split screen one pass moves the items
  between the two bags of that PC. Over the network each PC writes only its own human's bag: the giver takes the items out and sends GIVE, the receiver's PC adds
  what fits and answers with what it took, the giver gets the rest back; without an answer in 6 s the giver cancels, and the refund always follows the receiver's
  answer, never a guess. A reload of the same section restores both bags from the checkpoint and drops an open give; a section change carries both bags over and
  completes it.
* **The Genesis for the second player** (`native/scan1.c`, `scan1.h`). The story gives the Genesis to one character (in 1-2 to Jill alone) through a story
  action, and the engine has one scanner pipeline bound to the own slot (the scan view acts only for the first camera manager, the sight and every target's ray use
  the own human, there is one scanner GUI). Every human of the co-op game whose bag has no Genesis gets one through the same stock action as soon as any player
  holds one. In split screen the second human uses it in the whole view as if he were the own human, one scanner at a time (first come, first served). The
  analysis rate is the story's own counter in 1-2, so it stays the team's there; elsewhere each player has his own rate and herb rewards. Over the network a hidden
  item revealed by a scan on one PC is revealed on the other (a reliable session event).
* **HUNK in 2-2 ("8:32 P.M. Guest Cabin")** (`native/hunk1.h`, `bag1_game.inc`, `fallres1.h`). The story locks Parker away in this part, and a human in
  Parker's place died there within seconds while the AI Parker lived: a human placed where the story puts Parker (on another stage than Jill's) has no floor and
  hovers, and the engine's fall-out timer kills a PC's own human after 5 s without ground (it spares every other human, so the AI Parker lives). 0.2.0 puts HUNK,
  a character of the game, in the partner's place for this part only: after the engine's own section-start write the mod writes HUNK's class id (`0x800110B0`,
  resource `pl10b0`, part of the installed game) into slot 1 of the player setup object, so the engine's own creation path builds a HUNK human, and takes the id
  away at the next section start, so 3-1 builds its own pair. HUNK has no profile bag, so he gets the M92F handgun through the stock AddWeapon action; a human that
  hovers without ground (not one that falls) is put next to the partner before the fall-out timer runs out.
  The lock: 2-2's stage script runs `FsmLockPlayer` with `mIsLockPartner=1`. Its action takes "the partner" as the human that is **not this PC's own one**
  (`FUN_0041BC10`) and locks it (`FUN_006B3B80`: two state bits, the lower body put into a wait state: no walk, no aim). Over the network the guest's HUNK is the
  guest PC's own human, so on that PC the lock lands on the host's puppet. In split screen the second human is "the partner", and HUNK stood frozen: the mod gives
  him the stock unlock of the same action (`FUN_006AE390`) whenever the lock is on during play, after checking that function's bytes against the supported build.
* **Split-screen aim** (`native/aimgate1.h`, used by `capture.c`). `sGamePad+0x883` is one global "aim allowed" byte. The player's frame writes it only for the
  PC's own human (`FUN_00883C70(FUN_006AD740(unit))`: a gun in the hand, not unarmed, not an item, not the Genesis, not a state that forbids weapons), and its only
  reader (`FUN_00888590`, "the aim button of the current pad slot, if aiming is allowed") feeds every human's aim flag and upper-body states. So the second human
  could aim and shoot only while player 1 could, also with the published 0.1.0. Now, inside the swapped sections of the second human, the byte holds his own value
  (the game's own "may aim" asked for him on the logic thread, and cached for his weapon and state-machine sections, which may run on worker threads) and gets the
  game's value back when the section ends.

## What we changed

The mod, by what a player sees (code and design notes in the [repository](https://github.com/dolbaras/revelations-coop)):

* **Second human on one PC:** the campaign's AI partner is taken over by a second pad or by a remote input record (aim, fire, reload, melee, weapon change, menus).
* **Split screen:** two cameras and two HUDs through the engine's own viewports; cutscenes, pause and game over use the whole screen; checkpoint reloads and the game's
  own Exit survive.
* **Co-Op Play row, lobby and room:** a fourth main-menu row in the game's own style and fonts, with its text in the game's 13 languages; character choice, layout,
  gamepad choice, host/join, Steam friends or IP, ready buttons, downed/revive options.
* **Network, the whole story:** the guest's character is driven from the network (pad record plus position correction); both PCs wait for each other at every load and
  cutscene end; the host runs the enemies and the guest's shots count; doors, key items, pickups and story flags are mirrored; the host's section changes are followed
  section by section and episode by episode (0.1.0: the sections 1-1, 1-2, 1-3 only), and the session closes after the end of the campaign; where the story has Jill
  alone the other player watches her.
* **Give to partner (0.2.0):** RE5-style giving of a herb, a grenade or half of an ammunition type from the stock item screen, in split screen and over the network.
* **The Genesis for both players (0.2.0):** the second player gets the scanner too, with his own analysis rate and herbs; in split screen his scan uses the whole view.
* **HUNK in 2-2 (0.2.0):** the second player plays HUNK where the story locks the partner away, over the network and in split screen.
* **Co-op save of its own**, with the guest's weapons taken from the guest's own co-op save and Continue restoring the save's characters.
* **Gamepad routing** for virtual, duplicated or unplugged pads; **partner on the minimap/map**; **achievements** credited to the PC whose player earned them.
* **Other mods:** an Ultimate ASI Loader the player had as `dinput8.dll` keeps running as `winmm.dll`; a foreign `dinput8.dll` is kept and restored.
* **One-click log zip** (`coop\Collect logs.bat`) with names, paths, e-mail, MAC and public IP addresses removed.

Bugs found on the way to 0.2.0, with cause, fix and the check that shows it (the full history is in the repository's code comments and docs):

| Problem | Cause | Fix and proof |
|---|---|---|
| Split screen: the second player could not aim or shoot while player 1 had no gun in hand (unarmed, the Genesis, an item) or was in a state without weapons | One global "aim allowed" byte of the pad module, written for the PC's own human only and read for every human. | His own value inside his own sections (`native/aimgate1.h`); `tests/test_aimgate1.c` models the engine's frame (in the stock model he aims only while player 1 may) and compares the facts with the exe's bytes when its image is at hand. Live in split screen: he aims and fires while player 1 has no gun in hand. |
| A human in Parker's place died within seconds in 2-2 | The story puts Parker on another stage than Jill's: a human placed there has no floor, and the engine's fall-out timer kills a PC's own human after 5 s without ground (every other human, so the AI Parker, is spared). | HUNK in the partner's slot for 2-2 only, put next to the partner when he hovers without ground (`native/hunk1.h`, `native/fallres1.h`; `tests/test_hunk1.c`, `tests/test_fallres1.c`). Live on two PCs: both players alive, both walk and shoot, and the end of 2-2 (set by a test command) leads both PCs into 3-1 with the story's next pair. |
| In split screen HUNK stood frozen in 2-2 | The stage script's `FsmLockPlayer` (`mIsLockPartner=1`) locks "the partner", the human that is not the PC's own one: in split screen that is the second human. | The stock unlock `FUN_006AE390` for the second human while the lock is on; `tests/test_hunk1.c` checks the lock and unlock facts in the exe image. Live in split screen: HUNK walks, aims and shoots in 2-2. |

## Lessons that carry over

Short ones that were new relative to the playbook; the evidence is in the repository.

1. **One global engine flag can track player 1 only.** A single-player engine keeps some answers that look per player as one global value, written each frame for
   "the" player and read for everybody (here the "aim allowed" byte of the pad module). A second human then inherits player 1's state: he could not aim while player 1
   held the scanner. When a second human misbehaves only while player 1 is in some state, look for such a global and give each human his own value inside his own update,
   with the game's value put back afterwards.
2. **"The partner" in a stage script is a slot, not a person.** The story's script actions that lock or move "the partner" pick the human that is not the PC's own one.
   Once a human takes the AI partner's place, those actions hit him (in split screen the second human; over the network, on each PC, the other player's puppet). Before
   putting a human into that slot, list the actions a part runs on "the partner", and undo the ones meant only for the AI with the game's own counter-action.
3. **Measure an enemy on the PC that runs it.** A test check that read the host's hit counter missed hits on an enemy that the guest's PC was driving: the enemy had
   followed the guest across a zone border, the host's engine had put it to sleep, and the guest's PC ran it by itself. When the PC that runs an enemy can change, read
   the counters on both PCs (or on the one that runs the enemy at that moment) before calling a hit lost.

## QA

* **Offline suites** (`sh tests/build.sh`): C unit/integration suites for the lobby, pad routing, downed/revive state machines and pose, the network core and session, the
  role, load and cutscene barrier with the section follow (`tests/test_rolebarrier1.c`, also run as 32-bit code), guards, world mirroring, the save, and for 0.2.0 Give
  (`tests/test_give1.c`: reach, a downed partner, the split plan, GIVE, answer and cancel on a delayed line with timeouts, repeats, reloads and section changes, the
  13-language texts), the Genesis (`tests/test_scan1.c`), HUNK (`tests/test_hunk1.c`), the fall-out rescue (`tests/test_fallres1.c`) and the aim gate
  (`tests/test_aimgate1.c`); suites that rely on the exe compare their facts with the bytes of the supported build when its image is at hand. A headless bot
  (`tests/coop_bot.c`, with a network emulator and a Steam mock) speaks the protocol. All pass at the 0.2.0 release; the check counts are not quoted here (rerun with
  `tests/build.sh`).
* **Installer** (`powershell -File tests\test_installer.ps1`): 289 of 289 offline; 294 of 294 after the publication with the published 0.1.0 zip as the old version and
  `-Online` (the real GitHub feed, read only). After the publication the published 0.1.0 launcher updated a stand-in copy to 0.2.0 through the real feed (checksum against
  GitHub's digest, installer exit 0, version file 0.2.0), a second start installed nothing, and the uninstall gave the copy back byte for byte.
* **Live, two PCs on one LAN** (one copy of the game per PC, one PC as the host and the other as the guest, driven by a console and scripts, not by a person):
  - every section from 1-2 to 12-2 (25 sections), started directly or reached by the story's own transitions: arrival with the right characters, both players alive,
    the host's walk followed by the partner's character on the other PC;
  - transitions: 1-1 -> 1-2 by the story after a New Game from the lobby (the guest's real shots hurt the host's Ooze in 1-1), 1-3 -> 2-1 (the game's own end zone, no
    session end), 2-1 -> 2-2, 4-1 -> 5-2, 12-3 -> the end of the campaign (on both PCs the game's end, the credits and the session closed; the section ends were set by
    a test command, the boss fight itself was not played);
  - kills on both PCs for both players in 1-2, 3-2, 6-2, 8-3 and 9-2; real gun shots of the host's and of the guest's player hit and kill in 9-2, also while the partner
    holds the Genesis;
  - Give with the gamepad, the keyboard and the mouse: a herb, ammunition by half, a grenade to a partner who had none, a full partner refused with the line on the
    screen, no entry when the partner is too far or down, and a real section change (9-2 -> 10-1) while a herb was on its way: it arrived exactly once (the game ran in
    Russian);
  - the Genesis: both players get it in 1-2, the team rate in 1-2 both ways, each PC's own rate after it (1-3), a hidden item revealed on one PC (by a test command on
    the scan's own path) appears on the other;
  - 2-2 with HUNK: both players alive, both walk and shoot, and the end of 2-2 (set by a test command) leads both PCs into 3-1 with the story's next pair.
  The 0.1.0 checks of Episode 1 (both role orders, a Continue, a game over, joins by Steam friends on one network and by IP, the solo save byte for byte unchanged) are
  listed in the [v0.1.0 notes](https://github.com/dolbaras/revelations-coop/releases/tag/v0.1.0).
* **Split screen on one PC**, two virtual Xbox 360 pads: the second player aims and fires while the first has no gun in hand; Give from both players' screens, a full
  partner, too far; the Genesis: the second player gets it, holding it opens his scanner view in the whole screen, his own rate and his herb at 100 work (the rate set
  by a test command), and while the first player scans the second waits; HUNK walks, aims and shoots in 2-2.
* **What these were not:** parts were driven by test commands (teleports, set story flags, virtual gamepads); no person played 0.2.0 before its release; the last boss
  fight (12-3) was not played the story's way (the section was only started directly); 2-2's own story with HUNK (its scenes, doors and enemies) was not played
  through; many sections were checked for arrival and movement only (no enemy near the start), and shots in water sections (6-2) were not tested; a real scan of a
  target by the second player in split screen was not reached; Give's texts in the other 12 languages were not seen in a running game, and Give in split screen with
  the keyboard, the mouse or a downed partner was not run; no session between two different networks; Raid never touched. That is why the level is `automated`,
  not `verified-playthrough`.

## How to play

Download `revelations-coop-setup-0.2.0.exe` (or the zip and `Install Co-op.bat`) from the [release](https://github.com/dolbaras/revelations-coop/releases/tag/v0.2.0),
on both PCs for network play; an installed 0.0.1, 0.0.2 or 0.1.0 updates itself to 0.2.0 at the next start from its shortcut (both PCs need 0.2.0 for a network game:
another version is refused and the lobby says so). The installer finds the Steam copy (only build 13298487 is accepted), adds the mod and a "Resident Evil Revelations
Co-op" shortcut; play from the shortcut and choose **Co-Op Play** in the main menu. For network play the host picks "Host", the other player joins from the Steam friends
list or types the host's IP address (UDP 28410), both mark themselves ready, and the host starts New Game or Continue; from then on the guest's game follows the host's
story section by section. For split screen switch co-op on in the lobby, pick the layout and start the story as usual with two gamepads. To give an item, open the game's
item screen (BACK on a gamepad) and choose "Give to partner" on a herb, a grenade or ammunition while the partner stands close and is not down. The mod adds only
`dinput8.dll` and a `coop` folder; "Uninstall Co-op.bat" puts everything back. Setup exe and zip are not code-signed (SmartScreen may warn). Bug reports: an issue in the
mod's repository with the zip from `coop\Collect logs.bat` (both PCs for a network game).

## Known issues

* Not played by people before the release: everything under [QA](#qa) was run on the test PCs, partly with test commands (teleports, set story flags, virtual gamepads).
  Not yet tried between two different networks (Steam relay).
* The last boss fight (12-3) has not been played the story's own way on two PCs: the section could only be started directly; the end after it was checked.
* 2-2 with HUNK: the part's own story (its scenes, doors and enemies) was not played through; its end was set by a test command.
* Many sections were checked for arrival and movement only (no enemy near the start): fights there use the same code as the checked ones. Shots in water sections (6-2)
  were not tested.
* The Genesis in split screen: while the second player scans, the screen shows his scanner view in full (the game has one scanner view, so one player scans at a time: the
  first to start, the other waits); his analysis rate is kept only while the game runs (it starts at 0 again after the game is restarted).
* Give to partner: an item that is on its way to the partner at the very moment a new section starts is not in that section's first checkpoint, so loading that checkpoint
  again (a game over before the next one) takes it back, like everything else since. Where the story changes the characters with the section (Episode 9 to 10), an item
  still on its way lands with the new character on that PC; it is not lost.
* The Genesis: a real scan of a target by the second player in split screen was not reached in the tests (no scan target at the test spots, also not for the first player),
  and a hidden item revealed by a real scan on the other PC was checked only with a test command on the same path.
* Give to partner: its texts in the other 12 languages were not seen in a running game (only Russian); in split screen the keyboard, the mouse and a downed partner were
  not run.
* An enemy that chases the guest into another area is driven by the guest's PC. The host sees it asleep there, and if the host comes back before it dies, the guest's
  damage to it is lost (known since 0.1.0).
* Split screen and network are not used together; joining a running network game is not possible (start a new co-op game, Continue picks up the checkpoint).
* While co-op is on, the Campaign page's save panel can still show the solo save's play time and chapter; Continue loads the co-op save.
* In a network game the partner's character on your screen can hold other guns than the partner really carries (the DLC pistols are never shown on it); hits are always made with the partner's own weapon on the partner's PC.

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
   campaign session through Episode 1 in the author's tests (with 0.2.0, the whole story section by section); no switch in the game was involved.

The earlier plan to start a session in the campaign with the game's own `SessionDriver` was not followed. The earlier statement that two-client testing needs a second
Steam account and that the game's DRM stub kills a second local instance applies to one PC; the mod's network tests use two PCs. Technical facts from the earlier write-up that
we could not confirm are marked in "How the game works". The offsets we cross-checked matched; the one thing we found wrong there is the name `sNetworkManage` for the object at `0x1169398` (it is `sCoopManager`).

## Next steps

1. **Human playtest**, first the points under [Known issues](#known-issues): the last boss fight (12-3) played the story's way, 2-2's own story with HUNK, the fights
   of the sections checked only for arrival, shots in water (6-2), a real Genesis scan by the second player in split screen; reports welcome as issues with the
   `Collect logs.bat` zips of both PCs.
2. **Steam relay** between two different networks.
3. **The enemy that follows the guest into another area:** hand it back to the host with the guest's damage, so that no hit is lost when the host comes back.
4. **Downed/revive in split screen** (offered in network games through the lobby; a local variant exists behind an environment switch, off by default).
5. **The second player's Genesis rate in split screen** kept across a restart of the game.
