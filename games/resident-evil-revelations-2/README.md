# Resident Evil Revelations 2 (2015)

| | |
|---|---|
| Engine | MT Framework (Capcom), 32-bit, Direct3D 9, XInput; the engine's own class registry ("DTI") and a story script machine (FSM) |
| Versions tested | Steam app 287290, the only public branch, build **3658995** (2019-03-19); `rerev2.exe` 18,315,248 bytes, SHA-256 `55ac7d2bd80704c6a8a1fa7bd2371726adbaa01ae0fda056198fb042404367f9`; x86, image base 0x400000, no ASLR; Steam stub (`.text` is encrypted on disk and decrypted by the stub at start-up); Windows 10 and 11 |
| Projects | [Resident Evil Revelations 2 Co-op](https://github.com/dolbaras/revelations2-coop) (MIT), directory entry "Resident Evil Revelations 2 Co-op"; version [0.2.1](https://github.com/dolbaras/revelations2-coop/releases/tag/v0.2.1), published 2026-11-10, GitHub "Latest". Sibling game: [Resident Evil Revelations](../resident-evil-revelations/README.md) (the first game, its own write-up and mod, same engine family, different binary) |
| QA level | automated (C test suites, an installer suite and scripted two-PC lab sessions with real game clients); no human playthrough, nobody outside the lab has played it |
| Last updated | 2026-11-10 (0.2.1) |

Claim issue: [#298](https://github.com/cranebeaker/coop-anything/issues/298).

## Where things stand

Two players on two PCs, each with their own copy of the game and of the mod, can play
**Episode 1** of the story campaign together, Claire and Moira's half and Barry and Natalia's
half, over a direct IP or through Steam. Each player chooses a character in a lobby, both press
Ready, and the host's PC runs the enemies. There is an installer, a shortcut that installs newer
GitHub releases before each start, and a one-click log collector for bug reports.
0.2.0 (2026-11-09) was the first full release (0.1.0 and 0.1.1 were pre-releases); 0.2.1
(2026-11-10) gives a co-op game its own save so the campaign save is never touched, and a
**Continue** row in the room.

What is **not** there: Episodes 2-4, the bonus episodes, Survival, Invisible and Raid are not
offered (Raid is never touched), joining a running game is not possible, and nobody outside the
lab has played the mod. Everything below was checked by the author's own agents on the author's
own PCs, mostly with a console and scripted sessions. See [QA](#qa) for exactly what that means.

## Existing work

- Before this project: no network co-op for the campaign found (GitHub, Nexus, ModDB, Steam
  discussions and the FearLess forum, 2026-11-01; claim [#298](https://github.com/cranebeaker/coop-anything/issues/298)).
  What exists is the game's own **local split screen** in the campaign, a 2015 local co-op toggle
  and cheat tables, and Steam Remote Play Together or Parsec for the campaign. Online co-op in
  the shipped game is Raid mode only, which depends on Capcom's online service.
- [Resident Evil Revelations 2 Co-op](https://github.com/dolbaras/revelations2-coop) (this
  project, MIT): what each release added is in its
  [releases](https://github.com/dolbaras/revelations2-coop/releases).
- Other mods that load as `dinput8.dll` do coexist: ThirteenAG's Fusion Fix for this game
  ([WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack), shipped with an
  Ultimate ASI Loader as `dinput8.dll`) keeps running next to the co-op mod (tried with the January 2025 build; the
  fix and the co-op lobby both ran). Fusion Fix and
  [Lyall's RERevFix](https://github.com/Lyall/RERevFix) (MIT) were also read for patterns and
  structures of the engine.
- Same toolchain and approach as [Clive Barker's Jericho](../clive-barkers-jericho/README.md)
  and [TimeShift](../timeshift/README.md).

## How the game works (for co-op)

**The game is one process, and a campaign already has two humans in it.** The PC build has
local split screen in the campaign. The engine can run two human characters in one story, so
the design is "split screen whose second half lives on another PC": both PCs run the whole
engine and the whole story script, the host's world is the truth, and the other player's
character is a *puppet* in the second player slot. Placed objects (enemies, boxes, doors) are
identified by the engine's own placement id, the **LayoutID** stored inside every placed object.

**Netcode of the game.** Raid has its own session and sync layer, and the binary also links
a full Winsock stack, but the campaign has no network path and Raid online is dead, so the mod
ships its own transport instead of driving the game's. Raid's Steam keys are never used and Raid
is not touched.

**Save system.** The campaign save is one file, `DATA0.DAT`, which the game reads and writes
through the `ISteamRemoteStorage` interface (Steam Cloud carries it).

**Steam.** The game runs under its real app id with its own 2015 `steam_api.dll`. The pointers of
its Steam interfaces are in the context struct that `SteamInternal_ContextInit` returns, so the
mod calls exactly the interface versions the game uses.

### How the mod is loaded and hooks

`rerev2.exe` imports `dinput8.dll`, so a proxy `dinput8.dll` next to it is loaded by the exe's own
import table, with no injector. It forwards `DirectInput8Create` to the system DLL (or to a chained
`dinput8_real.dll`), checks the exe's SHA-256 and does nothing on any other build, and then patches
**import slots and COM vtables in memory**: Direct3D 9 (drawing the co-op screens over the finished
frame, showing a strip of text), the input path (only the local human's keyboard, mouse and pad
drive the local character), the game's Steam calls, and a few game functions and class vtable slots found by
the engine's own class names. **No game file is changed**, the Steam stub is not touched, and Raid
is not hooked.

### Transport and session

- **ENet over UDP**, direct IP (default port 28410), two channels: 0 reliable and ordered
  (control, events), 1 unreliable and sequenced (the character state, 20 Hz, 60 bytes). The wire
  protocol is versioned and a decoder ignores trailing bytes, so a newer sender can append fields.
  Described in [protocol.md](https://github.com/dolbaras/revelations2-coop/blob/main/docs/protocol.md).
- **Steam**: a lobby with the mod's own keys (never a Raid key), "Join game" in the friends list,
  invites, and a message pipe. The pipe is `ISteamNetworkingMessages002` fetched through the
  game's old `ISteamClient017` (it works, returning small adapter objects), with the game's
  `SteamNetworking005` on a channel of our own (21042; the game polls only 905 and 0) as the
  fallback. Steam callbacks about *our* lobby are kept from the game's own handlers (they would
  feed its Raid state machine) and all others pass unchanged. Facts and layouts:
  [steam-transport.md](https://github.com/dolbaras/revelations2-coop/blob/main/docs/steam-transport.md).
- **The lobby** is a **Co-Op Play** row on the campaign page, drawn by the mod in the game's own
  language and font where it can: Find a Partner (Steam friend, or System Link by IP) or Join a
  Partner, then a room where each player picks one of the four characters, the host picks the
  half and difficulty, both press Ready. A level-load barrier (up to 300 s) holds the faster PC.
  The host can remove a player; a refused join says why (full, already started, no answer, other
  mod version, other game version, removed). The mod and game versions must match: a player with
  another version is refused with a message that says so.

### The shared world (host authority, replicas)

- **Enemies**: the host's story creates them; the other PC gets **replicas** made from the host's
  records by LayoutID, and its own enemy creator is refused except when applying the host's
  record. Position, HP and action are mirrored; a hit by the guest on a replica becomes a message
  to the host, which applies the exact damage; HP reaching 0 on the host becomes the death on
  both. The damage virtual (slot 234 of the enemy classes) is patched in every class vtable.
- **Kill credit** belongs to the human whose slot made the kill, on both PCs.
- **Story**: the host and the guest run the same script, so the mod shares the triggers and the
  humans' own actions: **scenario flags** are mirrored (the `SetFlag` change becomes a message),
  real-time scenes that only one PC's trigger started are replayed on the other, the game's own
  sync point for cutscenes becomes a barrier with a time-out, and the personal inventories of both
  players are kept the same on both PCs (also after a game over and a return to a checkpoint).
- **Co-op doors, shutters, boxes and slides**: the story's pair-only objects open on both PCs
  from either player's press; boxes carried or pushed (also by Natalia) and the two-person slide
  are mirrored; Natalia's crawl through the hole of the shack runs on her own PC.
- **Stage change gate and loader mirror**: the game streams the map in blocks around the *local*
  player and changes the story's stage by the floor of the player in slot 0. A gate holds the
  change until every living human's floor belongs to the new stage (at most 25 s) and the two
  PCs exchange their block lists.
- **End of a half**: the trigger zone at the radio never fired for a second human, so the host
  watches both humans and sets the zone's flag when both stand in it; the game's own result flow
  then runs on both PCs. The end of Episode 1 ends the co-op session and the game's end menu
  offers no next episode.
- **Per-player achievements**: the game's statistic counters are gated by slot, so a record
  belongs to the player who earned it and shared records go to both. The mod never calls
  `SetAchievement`; the game's own path unlocks it on the PC of the human who did it. The Steam
  unlocks were switched off in most lab tests; in a player's game **they are not blocked**.
  Details: [achievements.md](https://github.com/dolbaras/revelations2-coop/blob/main/docs/achievements.md).
- **Downed and revive** (the host's choice in the room, on by default): a player whose health
  runs out lies down and bleeds out after 60 seconds unless the partner holds the game's Rescue
  key (or the pad's Y) next to him; both down is game over for both. Design:
  [downed-revive.md](https://github.com/dolbaras/revelations2-coop/blob/main/docs/downed-revive.md).
- **Partner on the minimap**: the partner's arrow is shown on the minimap on both PCs.
- **A partner is gone**: the host reads it and plays on with the game's AI partner; a guest whose
  host is gone returns to the title by itself; silence of 8 seconds is named on screen.
- **Retry** on either PC takes both players to the same checkpoint.

### The co-op save (0.2.1)

While a co-op game runs, the game's `DATA0.DAT` calls on the `ISteamRemoteStorage` vtable
(FileExists, GetFileSize, FileRead, FileWrite, FileDelete) are routed to a co-op save in
`%LOCALAPPDATA%\CAPCOM\RESIDENT EVIL REVELATIONS2` (`rev2_coop_save_<number>.dat`, one per
Steam account) and the solo save image kept in memory is put back when the title menus are
back. The campaign save, and Steam's own record of it, are therefore not opened by a co-op game.
The first co-op game starts from a copy of the solo save's settings and unlocks. When the host's
co-op save holds an Episode 1 checkpoint, the room shows a **Continue** row; the guest follows
the host's checkpoint record with his own items. The co-op save is not in Steam Cloud.

### Installing, updating and other mods

The installer finds the Steam copy, checks the exe against the pinned SHA-256, backs up a
foreign `dinput8.dll` once, copies the mod and makes a shortcut that starts the game through
Steam. The shortcut installs a newer release from GitHub before the game starts, only when
GitHub gives the file's SHA-256 digest and the download matches it; any failure only leaves a
line in a log and the game starts. An Ultimate ASI Loader found as `dinput8.dll` is copied as
`winmm.dll` (a name that loader serves and the exe imports), so its plugins keep running. A
`dinput8.dll` that another mod put over the co-op one later is kept as the original at the
next install. Uninstalling leaves the game folder as before, byte for byte. The co-op screens and
the installer are in the game's 12 languages (Japanese in the co-op screens uses a Windows
font). Details: [releasing.md](https://github.com/dolbaras/revelations2-coop/blob/main/docs/releasing.md).

## What we changed

Selected bugs found in the lab, with cause, fix and the test that shows it (the full history is
in the repository's docs).

| Problem | Cause | Fix and proof |
|---|---|---|
| Both players died at the first stage change (the hop from stage 100 to 101) | Each PC streams blocks for its own local player; the story changes its stage when the player in slot 0 stands on the next stage's floor. The human still on the old floor lost it. | The gate on the stage change plus the loader-list mirror ([world.md](https://github.com/dolbaras/revelations2-coop/blob/main/docs/world.md), sections 6j and 6m). Shown on the pair: stage 100 to 102, both alive; model test `tests/test_bgl.c`. |
| Host crash 2-3 s after a retry | The mirror merged the partner's stale block list into the list the engine had just emptied for the level rebuild. | Rule: when the engine's own need is empty, the mirror drops its merged entries and merges nothing. 5 of 5 retries crashed with the mirror on and 0 of 3 with it off; with the rule, three retries in a row and one with the humans far apart did not crash on either PC; model test in `tests/test_bgl.c`. |
| The second human could not end a half | The radio zone is an enter-type zone that a puppet never fires. | The host's trigger watches both humans (`tests/test_radio.c`); both PCs reach the cutscene barrier and show the results, records identical on both. |
| The guest's blows and shots did not reach the enemies | On the guest's PC the enemies are replicas; the host's PC decides their health. | The guest's knife, handgun and grabbed-enemy hits go to the host, which applies them; the kill is seen on both PCs and credited to the guest; seen live on two PCs for 0.2.0. |
| Another mod's `dinput8.dll` was overwritten by an update and deleted by the uninstaller | The record assumed the file was always the co-op DLL. | The record notices it, keeps it as the original, and the uninstaller leaves it in place; `tests/test_installer.ps1`. |
| A partner's kill moved *this* PC's profile towards an achievement | The game bumps its global counter for any slot, including the puppet. | The gate on the bump pair by slot (`tests/test_ach.c`, 786 checks), then a real unlock of one shared record read back on two lab accounts. |

## Lessons that carry over

Short ones that were new relative to the playbook; the evidence is in the repository's docs.

1. **A virtual that most subclasses override cannot be hooked at its base function.**
   `uEnemy::ApplyDamage` (vtable slot 234) is the base function for only some enemy classes; the
   common ones point at their own functions. List the slot over *all* class vtables from the memory
   image first, patch each slot after checking it against the expected value, and keep the
   original pointer per vtable (56 vtables here).
2. **A research feature that a lab variable switches on has not shipped.** Six stages of world sync
   ran only in sessions started with a test variable plus console lines; a player starting from
   the lobby got puppets and nothing else, and no lab script noticed because they all set the
   switches. After a set of features, run one session with no test variable at all and read the
   log for the line that proves each feature is on.
3. **A streaming or stage system built for one local viewer drops the *other* human's world.** Here
   the block streamer follows the viewport-0 player and the story changes its stage by the floor
   of the player in slot 0. On a pair each PC streams for its own human, so the first stage change
   unloaded the floor under the partner (a death with no damage source right after a load). Find
   the mechanism by the engine's own class names (`sBGLoad`, `cZoneContentsBGLoad`), then keep both
   humans' blocks and hold the stage change until both have crossed.

## QA

- **Offline, in the repository** (`tests/`): `sh tests/build.sh` builds and runs the C suites with
  the Zig compiler and refuses any suite that fails, and every model test must fail against the
  version before its fix. Suites at the 0.2.0 gate, with their check counts: ach 786, bgl 85,
  devreset 19, downed 443, glue 94, lobby 1360, meleehit 15048, objsync 145, pad 113, proto 121435,
  radio 64, resil 979, session 395, steam 158 (a mock Steam universe), warp 852 and more;
  `tests/test_steam_real32.c` calls the real Steam code against fake interfaces that use the game's
  C++ calling conventions; `python -I tests/run_net_tests.py` runs the session over real UDP through
  a network emulator with loss, duplication and reordering. A headless bot (`tests/coop_bot.c`)
  speaks the protocol for the refusal, version and lost-peer cases.
- **Installer**: `powershell -File tests\test_installer.ps1 -Online`: 241 of 241 on 2026-11-10 after
  publication, including the 0.2.0 launcher updating a copy to 0.2.1 through the real GitHub release
  feed (download, checksum against GitHub's digest, install, version file) and an uninstall that
  gives the copy back byte for byte.
- **Live, two PCs on one LAN** (the author's test copies of the game, one PC as the host and the
  other as the guest, driven by a console and scripts, not by a person): every checkpoint of both
  halves of Episode 1, reached with a lab warp or teleport, with the doors, shutters, boxes, slides,
  the boost and Natalia's hole; the radio ends of both halves with identical results and records on
  both PCs; game over and Retry in both halves; downed and revive both ways; the guest's knife and
  handgun kills; the co-op save (a checkpoint written on both PCs, Continue from it, the campaign
  save and Steam's record of it identical by SHA-256 before and after); the lobby by IP and every
  waiting line and refusal; a lost partner and a lost host; the lobby through Steam on two PCs of
  one network in three ways (the friend row in the lobby, "Join game" in the Steam friends list,
  the host's invite). One shared achievement was really unlocked and read back on two accounts.
- **What this does not show**: a person playing a half on foot from start to end (the lab routes
  use console warps and teleports, and the lobby was driven by injected keys; mouse and pad menus
  were not exercised); two different networks (Steam relay); any player outside the lab; a
  real fight that downs a player; the installer window on the player's real Steam install.
  This is why the QA level is `automated` and not `verified-playthrough`.

## How to play

Both players need the Steam game (app 287290, build 3658995) and the same mod version.

1. Download `revelations2-coop-setup-0.2.1.exe` (or the zip: extract it and run
   `Install Co-op.bat`) from the
   [release](https://github.com/dolbaras/revelations2-coop/releases/tag/v0.2.1) on **both** PCs. The
   installer is not code-signed, so Windows SmartScreen may warn.
2. Start the game from the **Resident Evil Revelations 2 Co-op** shortcut: Campaign, then
   **Co-Op Play**.
3. One player picks **Find a Partner** (Steam friend or System Link; for a direct IP the host
   forwards UDP port 28410 and allows the game in the firewall), the other **Join a Partner**.
   Pick characters, press Ready; the host starts the game.
4. Steam achievements are **not** blocked: what a co-op game earns is unlocked on your account.

Bug reports: double-click `coop\Collect logs.bat` in the game folder right after the problem; it
writes one zip with the mod's logs, settings and a system summary, with user, computer and player
names, Steam names and IDs, paths, e-mail, MAC and public IP removed. Nothing is uploaded. Open an
[issue](https://github.com/dolbaras/revelations2-coop/issues) with the zip of both PCs.

## Known issues

- Episode 1 only. Episodes 2-4, the bonus episodes, Survival, Invisible and Raid are not offered.
- A co-op game always starts at the beginning of the half the host picks (or at the last co-op
  checkpoint with Continue); joining a running game is not possible. If one player's game closes
  or the connection is lost, start a new co-op game.
- The end of a half needs both players at the radio together.
- Japanese text in the co-op screens uses a Windows font, not the game's own.
- Play between two different networks (Steam relay) has not been tried.
- A `dinput8.dll` of another mod that is not an Ultimate ASI Loader is switched off while co-op is
  installed; removing co-op puts it back.
- The installer and the setup exe are not code-signed.

## Next steps

1. **A person outside the lab.** The first real playthrough of both halves on foot, on two PCs
   and ideally on two different networks, and with real mouse and pad menus; report with the
   `Collect logs.bat` zips of both PCs.
2. **Test the Steam relay** between two different networks, and a click in the real friends list
   and the overlay's invite dialog.
3. **Episodes 2-4.** Each episode has its own stages, scripts and bosses; the stage gate, the
   scenario flags and the host-authoritative enemies should carry over, but nothing beyond
   Episode 1 has been run. Of the 43 achievements the mod maps, only the Episode 1 end-of-episode
   records and one shared record were exercised.
4. **Separated sections and bosses**, a real fight that downs a player, and the AI partner in
   the downed rule.
5. See the sibling [Revelations 1](../resident-evil-revelations/README.md) write-up for the
   first game; the offsets of the two builds do not transfer, only the structure ideas.
