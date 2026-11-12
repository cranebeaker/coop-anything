# Resident Evil Revelations 2 (2015)

| | |
|---|---|
| Engine | MT Framework (Capcom), 32-bit, Direct3D 9, XInput; the engine's own class registry ("DTI") and a story script machine (FSM) |
| Versions tested | Steam app 287290, the only public branch, build **3658995** (2019-03-19); `rerev2.exe` 18,315,248 bytes, SHA-256 `55ac7d2bd80704c6a8a1fa7bd2371726adbaa01ae0fda056198fb042404367f9`; x86, image base 0x400000, no ASLR; Steam stub (`.text` is encrypted on disk and decrypted by the stub at start-up); Windows 10 and 11 |
| Projects | [Resident Evil Revelations 2 Co-op](https://github.com/dolbaras/revelations2-coop) (MIT), directory entry "Resident Evil Revelations 2 Co-op"; version [0.3.0](https://github.com/dolbaras/revelations2-coop/releases/tag/v0.3.0), published 2026-11-12, GitHub "Latest" (0.2.1 of 2026-11-10 was Episode 1 only). Sibling game: [Resident Evil Revelations](../resident-evil-revelations/README.md) (the first game, its own write-up and mod, same engine family, different binary) |
| QA level | automated (C test suites, an installer suite and scripted two-PC lab sessions with real game clients; Episode 1 from start to end, Episodes 2-4 and the bonus episodes stage by stage); no human playthrough, nobody outside the lab has played it |
| Last updated | 2026-11-12 (0.3.0) |

Claim issue: [#298](https://github.com/cranebeaker/coop-anything/issues/298).

## Where things stand

Two players on two PCs, each with their own copy of the game and of the mod, can start **every
episode** of the story campaign together: Episodes 1-4 (Claire and Moira's half and Barry and
Natalia's half of each) and the bonus episodes **The Struggle** (Moira and Evgeny) and **Little
Miss** (Natalia and Dark Natalia), over a direct IP or through Steam. Each player chooses a
character in a lobby, both press Ready, and the host's PC runs the enemies. There is an installer,
a shortcut that installs newer GitHub releases before each start, and a one-click log collector
for bug reports. 0.2.0 (2026-11-09) was the first full release (Episode 1; 0.1.0 and 0.1.1 were
pre-releases), 0.2.1 (2026-11-10) gave a co-op game its own save and a **Continue** row, and
**0.3.0** (2026-11-12) opens Episodes 2-4 and the bonus episodes.

How far each part is checked differs a lot. **Episode 1** was checked in the lab from its start to
its end. **Episodes 2-4 and the bonus episodes** were checked stage by stage, mostly after console
jumps to the game's own checkpoints, and **no human has played them yet**: 0.3.0 was published
on the author's decision so that players can try them, with an explicit list of what is not
verified (see [Known issues](#known-issues)). Survival, Invisible and Raid are not offered (Raid
is never touched), joining a running game is not possible, and nobody outside the lab has played
the mod. Everything below was checked by the author's own agents on the author's own PCs, mostly
with a console and scripted sessions. See [QA](#qa) for exactly what that means.

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
  episode and difficulty, both press Ready. A level-load barrier (up to 300 s) holds the faster PC.
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

### Episodes 2-4 and the bonus episodes (0.3.0)

- **Starting a half.** The stock start call that the room uses starts Claire's halves of
  Episodes 2-4 and both bonus episodes directly. For Barry's halves it stops at the game's results
  screen or falls back to the title, so the mod starts the episode's first part and then *hops*:
  it writes the half's first stage into the game's own restart record and asks for the stock
  "retry from checkpoint". The partner's PC follows the same hop. The first stage's opening may
  flash by.
- **More than twelve enemies.** A batch of the host's enemy records holds at most twelve
  records. The host used to send the first twelve living enemies in every batch, so with 13 or
  more (the Episode 2 siege, Episode 3's sewers) the rest never got a record and, if their one-time
  spawn message arrived while the guest was still loading, never got a replica. The window of
  records now turns from batch to batch; a model test checks that every enemy is covered within
  `ceil(n / 12)` batches, and the old model fails it.
- **Stale replicas.** A record that left the host before an enemy's death could arrive after the
  death message and bring the replica back to life on the guest's PC. Records for an enemy that
  died less than ten seconds ago are now ignored. A replica whose enemy the host no longer sends
  for ten seconds and forty batches is removed when it is an enemy, and only logged when it is a
  story companion (the guest's own story scripts still point at those).
- **The end.** The end of Episodes 2 and 3, of the bonus episodes and of the game's last stage
  (Episode 4, where the credits run first) closes the co-op session on both PCs after the game's
  own results, and the end menu offers no "next episode".
- **Episode 4's finale.** Two set models of the finale are helicopters moved by the stage's
  script. Mirroring their actions like doors freed a moving unit that the engine still walked
  over, and the game crashed; they are now left to each PC's own script. In the same stage the
  second human could stay parked at the edge of the map after the load; the mod places that human
  next to the partner.

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
| With 13 or more enemies (Episode 3's sewers: 14 on the host, 12 on the guest) some never existed on the guest's PC | A batch of enemy records holds twelve; the host always sent the first twelve, so the rest had no record, and their one-time spawn message could arrive while the guest was still loading. | The record window turns from batch to batch (`tests/test_wrec.c`, 1657 checks; the old model fails). Shown on the pair: 14 of 14 on both PCs, twice. |
| Enemies killed on the host came back alive on the guest's PC | A record sent before the death arrived after the death message and set the replica's health again. | Records for an enemy that died less than ten seconds ago are ignored; shown in the siege: the alive counts of both PCs equal at every sample. |
| Both games died in Episode 4's finale | The finale's helicopters are script-driven set models; mirroring their actions like doors freed a moving unit still linked in the engine's update list. | Those set models are excluded from the door mirror (`tests/test_objsync.c` plus a model of the old rule that must fail); both games ran more than eight minutes in the finale afterwards. |

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
4. **A fixed-size batch silently truncates.** A per-tick state batch that is "full" drops
   whatever comes after its limit every time, always the same objects, and nothing logs it. When
   a world can hold more objects than one batch, rotate the window and test that every object is
   covered within `ceil(n / per_batch)` batches.

## QA

- **Offline, in the repository** (`tests/`): `sh tests/build.sh` builds and runs the C suites with
  the Zig compiler and refuses any suite that fails, and every model test must fail against the
  version before its fix. Suites at the 0.3.0 gate, with their check counts: ach 786, aim 159730,
  bgl 85, combat3 19072, downed 443, grapple 603859, lang 3587 (texts in 12 languages), lobby
  1735, lobbymust 133, meleehit 15066, objsync 150, proto 121435, warp 6750, wrec 1657 (the
  enemy-record window) and more; `tests/test_steam_real32.c` calls the real Steam code against fake
  interfaces that use the game's C++ calling conventions; `python -I tests/run_net_tests.py` runs the
  session over real UDP through a network emulator with loss, duplication and reordering. A
  headless bot (`tests/coop_bot.c`) speaks the protocol for the refusal, version and lost-peer cases.
- **Installer**: `powershell -File tests\test_installer.ps1`: 252 of 252 before the 0.3.0
  publication (with the 0.2.1 package as the old version) and 257 of 257 with `-Online` (the real
  GitHub feed, read only); after the publication the published 0.2.1 updated a stand-in copy to
  0.3.0 through the real feed (checksum against GitHub's digest, install, version file) and the
  uninstall gave the copy back byte for byte.
- **Live, two PCs on one LAN** (the author's test copies of the game, one PC as the host and the
  other as the guest, driven by a console and scripts, not by a person):
  - Episode 1: every checkpoint of both halves, reached with a lab warp or teleport, with the doors,
    shutters, boxes, slides, the boost and Natalia's hole; the radio ends of both halves with
    identical results and records on both PCs; game over and Retry; downed and revive both ways;
    the guest's knife and handgun kills; the co-op save; the lobby by IP and through Steam on one
    network; one shared achievement really unlocked and read back on two accounts.
  - Episodes 2-4 and the bonus episodes (0.3.0): the start from the room with the right characters
    for every episode; both humans walking and seeing each other in every half; the Episode 2
    village siege (sixteen enemies, then a second group) and the Pedro boss with enemy health, hits
    and kills identical on both PCs and the kill credited to the player who made it; The Struggle's
    second chapter fought both ways; the game's own results screens between Episode 3's halves and
    between The Struggle's chapters; the ends of Episodes 2 and 3, of both bonus episodes and of
    Episode 4's last stage; the stages after the first of each half mostly reached by console jumps
    to the game's own checkpoints. Scripted walking, aiming and kept health were used.
- **What this does not show**: a person playing any of it; the story's own step in Episode 2
  from the village (stage 201) to the tunnel (stage 203, entered only by a jump) and stage 203
  itself; the fights in Barry's halves of Episodes 2-4 and in Claire's halves of Episodes 3-4 (the
  enemies there were asleep or out of reach of the scripts); the two last bosses of Episode 4
  (stages 453 and 454, 30,000 health each; the end of the episode was shown after a test request of
  the stage's end); The Struggle's stage 610, which starts behind a gate that did not open in the
  tests, and the clear conditions of its middle chapters; Little Miss's stealth rules; the story's
  own steps between the stages of Episodes 2-4; two different networks (Steam relay); any player
  outside the lab. This is why the QA level is `automated` and not `verified-playthrough`.

## How to play

Both players need the Steam game (app 287290, build 3658995) and the same mod version.

1. Download `revelations2-coop-setup-0.3.0.exe` (or the zip: extract it and run
   `Install Co-op.bat`) from the
   [release](https://github.com/dolbaras/revelations2-coop/releases/tag/v0.3.0) on **both** PCs. The
   installer is not code-signed, so Windows SmartScreen may warn. An installed 0.1.1 or later
   updates itself to 0.3.0 at the next start from its shortcut.
2. Start the game from the **Resident Evil Revelations 2 Co-op** shortcut: Campaign, then
   **Co-Op Play**.
3. One player picks **Find a Partner** (Steam friend or System Link; for a direct IP the host
   forwards UDP port 28410 and allows the game in the firewall), the other **Join a Partner**.
   The host picks the episode (1-4, The Struggle, Little Miss) and the difficulty, each player
   picks a character (Claire's or Barry's half), both press Ready; the host starts the game.
4. Steam achievements are **not** blocked: what a co-op game earns is unlocked on your account.

Bug reports: double-click `coop\Collect logs.bat` in the game folder right after the problem; it
writes one zip with the mod's logs, settings and a system summary, with user, computer and player
names, Steam names and IDs, paths, e-mail, MAC and public IP removed. Nothing is uploaded. Open an
[issue](https://github.com/dolbaras/revelations2-coop/issues) (there is a bug report form) with the
zip of both PCs.

## Known issues

- Episodes 2-4 and the bonus episodes are verified stage by stage, not as playthroughs, and no
  human has played them. Not verified: Episode 2's step from the village to the tunnel (the
  button at the rubble on the north terrace) and the tunnel stage; the fights in Barry's halves of
  Episodes 2-4 and in Claire's halves of Episodes 3-4; the two last bosses of Episode 4; The
  Struggle's chapter behind Ryan's gate and the clear conditions of its middle chapters; Little
  Miss's stealth rules; the story's own steps between the stages of Episodes 2-4.
- Story companions that the host's story removes may stay on the guest's PC where the story left
  them (no effect on play was seen).
- Barry's half of Episodes 2-4 starts a few seconds after the first stage of the part has loaded;
  that stage's opening may flash by.
- "Continue" in the room works for Episode 1 checkpoints only.
- Survival, Invisible and Raid are not offered. Joining a running game is not possible: if one
  player's game closes or the connection is lost, start a new co-op game.
- The end of Episode 1's halves needs both players at the radio together.
- Japanese text in the co-op screens uses a Windows font, not the game's own.
- Play between two different networks (Steam relay) has not been tried.
- A `dinput8.dll` of another mod that is not an Ultimate ASI Loader is switched off while co-op is
  installed; removing co-op puts it back.
- The installer and the setup exe are not code-signed.

## Next steps

1. **A person playing it.** The checks listed under "What this does not show", first Episode 2's
   step from the village to the tunnel, the fights in Barry's halves, Episode 4's last bosses and
   The Struggle's gate; the author has a prepared set of starting points for exactly these. Reports
   with the `Collect logs.bat` zips of both PCs help most.
2. **Test the Steam relay** between two different networks, and a click in the real friends list
   and the overlay's invite dialog.
3. **Continue for Episodes 2-4**: the room offers Continue for Episode 1 checkpoints only.
4. See the sibling [Revelations 1](../resident-evil-revelations/README.md) write-up for the
   first game; the offsets of the two builds do not transfer, only the structure ideas.
