# Skyward Sword co-op: working notes

Game: The Legend of Zelda: Skyward Sword, Wii disc, game ID SOUE01, Revision 0 (NTSC-U).
Disc SHA-1 9cf9a4a7ed2a6a4abb4582e3304af1327c160640 (DolphinTool verify, no problems).
Emulator: Dolphin 2609a, Windows 11 x64.
Decomp: [zeldaret/ss](https://github.com/zeldaret/ss) @ 96f2dd54 (2026-10-19). It builds against this disc
with 637/637 files byte-identical (`dtk shasum -c config/SOUE01/build.sha1`).

## Phase 0 triage (2026-10-25)

- No co-op project for Skyward Sword in DIRECTORY.md, none in hub issues/PRs (search "skyward").
- Not an Archipelago game ([games list](https://archipelago.gg/games)). Randomizers exist for Wii and HD, single-player only.
- Path: build from zero, emulator memory-sync (like the Wind Waker Dolphin mods), using the decomp for addresses.

## Phase 3 memory map (SOUE01)

### Observed (read live from Dolphin 2609a)

- Game RAM found as a MEM_MAPPED view of 0x2000000 bytes whose first 6 bytes are `SOUE01`. MEM2 view sits at
  MEM1 view + 0x10000000 (same layout Dolphin Memory Engine relies on).
- At boot (warning screen): `FileManager::sInstance` = 0x8095545C (MEM1 heap). `dAcPy_c::LINK` = 0.
  `dScGame_c::currentSpawnInfo` empty. All live flag arrays zero.
- Dolphin 2609a has three mapped views that start with `SOUE01` (sizes 0x2000000, 0x2000000, 0x1800000). All three
  are live (~160k words changed per second) and agree, so any of them works.

Live session 2026-10-25, new file, tester driving Link (human, not a bot):

- **Player pointer** `dAcPy_c::LINK` (0x8057578C) = 0x80B7652C (MEM1). Same value after a stage change.
- **Position** LINK+0xC0 (3 x f32, big endian) verified: tracked walking (X 329..3890, Z -2902..-409),
  stairs (Y 1.5 -> 25.7 -> 50.7). `mpPosition` (+0x98) points at +0xC0. +0x118 (mOldPosition) and
  +0x130 (Y + 150) follow it.
- **Facing** LINK+0xB8, the Y component (s16 at +0xBA) verified: full s16 range, snaps to exact
  0 / +-16384 / 32765 when walking straight along corridors.
- **Area** `currentSpawnInfo` verified: `F001r` room 1 layer 1 entrance 5 (Knight Academy, Link's room) ->
  `F000` room 0 layer 1 entrance 4 (Skyloft) on walking outside. During the load LINK read 0, then Link
  reappeared at (-4582.9, 1237.7, -7701.0). The stage string switches before the layer byte does.
- **Flags** verified to update live. One story action in Skyloft changed: story word 6 (0 -> 0x0008),
  story word 32 (0 -> 0x0010), scene word 2 (0 -> 0x0010), skip word 6 (0 -> 0x0010). Bit numbers are raw
  word*16+bit, not the game's story flag IDs (those go through storyflag_map.h).
- **Save copy:** the in-RAM SaveFile mFileA story flags matched the live array right after the action
  (before saving to NAND). mFileB held an older subset. So mFileA was the active file in this session.
- Health from mFileA: 24/24 quarter-hearts (6 hearts) on a new file.

Not checked: whether the "first frozen recording" (position constant for 40 s) was Link idling in a text box.
The tester first said they walked, later said they had not. All later recordings moved as expected.

### Derived from the decomp

Addresses from `config/SOUE01/symbols.txt`, layouts from headers. Player pointer, position, facing,
currentSpawnInfo, story/scene/skip flag arrays and mFileA are verified (above). The rest are not yet checked.

| What | Address / offset | Source |
|---|---|---|
| Player actor pointer `dAcPy_c::LINK` | 0x8057578C (u32 ptr) | symbols.txt, include/d/a/d_a_player.h |
| Actor rotation `dAcBase_c::mRotation` | +0xB8, 3 x s16 | include/d/a/d_a_base.h |
| Actor position `dAcBase_c::mPosition` | +0xC0, 3 x f32 | include/d/a/d_a_base.h |
| Current area `dScGame_c::currentSpawnInfo` | 0x805B3860, 0x2A bytes | symbols.txt, include/d/d_sc_game.h |
| Next area (load in progress) `dScGame_c::nextSpawnInfo` | 0x805B388C | same |
| SpawnInfo layout | +0x00 stage name char[32], +0x22 room, +0x23 layer, +0x24 entrance, +0x25 night, +0x26 trial | include/d/d_sc_game.h |
| Story flags (live) `StoryflagManager::sFlags` | 0x805A9AD8, 0x80 x u16 | symbols.txt, include/d/flag/storyflag_manager.h |
| Item flags (live) `ItemflagManager::sFlags` | 0x805A9BD8, 0x40 x u16 | |
| Dungeon flags (current dungeon) | 0x805A9C58, 8 x u16 | |
| Scene flags (current scene) | 0x805A78D0, 8 x u16 | |
| Zone flags | 0x805A78E0, 0xFC x u16 | |
| Temp flags | 0x805753E8, 4 x u16 | |
| Skip flags | 0x805A9C68, 0x10 x u16 | |
| Chest (tbox) flags | 0x805753F4, 2 x u16 | |
| Enemy defeat flags | 0x805A7AD8, 4096 x u16 | |
| FileManager | ptr at 0x80574FFC; SaveFile mFileA at +0x8, mFileB at +0x53C8 | include/toBeSorted/file_manager.h |
| SaveFile | 0x53C0 bytes: story_flags +0x8E4, item_flags +0x9E4, scene_flags[208] +0x1A64, health cap +0x5302, health +0x5306 | include/toBeSorted/save_file.h |

How flags work (from the headers): the managers keep live copies in the static arrays above and copy them
into the current SaveFile on `commit()`. Some story/item "flags" are multi-bit counters (storyflag_map.h),
so a sync layer should copy whole u16 words or use the flag index map, not single bits.

### Guesses / unknowns

- Whether mFileA is always the active file, or only in this session (`isFileInactive()`): unknown.
- Whether writing the live flag arrays from outside takes effect without calling commit(): unknown.
  Nothing has been written to game memory yet.
- Whether the player actor is always in MEM1: observed MEM1 once, unknown in general.

## Phase 4 design (2026-10-25)

Path: build from zero. Emulator memory-sync bridge (console-emulators.md 2b) plus a small amount of
game code built with the decomp's own toolchain. Decomp license is CC0, so derived code is fine.

### Checked before deciding (observed)

- Dolphin 2609a boots an extracted data partition from the command line:
  `Dolphin.exe -u <own user dir> -b -e <extracted>\DATA\sys\main.dol`. Game RAM shows SOUE01 and the
  FileManager pointer as with the RVZ. So modified game files can live in an extracted copy, never the RVZ.
- A second Dolphin with its own `-u` user dir runs next to the first. `ss_mem.py` picks one with
  `SS_DOLPHIN_PID`. This is the two-player-on-one-PC test setup.
- `mod/tools/u8.py` unpacks `files/rels.arc` (636 RELs, 32-byte aligned, last file unpadded) and repacks it
  byte-identically (SHA-256 equal).
- The decomp rebuilds every REL byte-identically, so a REL built from edited source can replace one in the
  archive.
- `Alink` (Link's model archive) is loaded at boot (src/REL/d/d_s_boot.cpp) and other actors already take
  models from it (`getOarcResFile("Alink")` in d_a_obj_arrow.cpp, d_a_obj_bomb.cpp).
- Actors spawn via `dAcObjBase_c::create(profile, room, params, &pos, &rot, &scale, params2)`; the actor's REL
  loads on first create.
- MEM2 location in `ss_mem.py` is a guess (picks a 0x4000000 view by size; the "MEM1 + 0x10000000" rule was
  wrong for both instances seen). Everything used so far is in MEM1. Fix before reading MEM2.

### Decision

1. **Puppet (second Link):** a custom actor REL, written in C++ against the decomp headers, compiled with the
   decomp's toolchain (mwcceppc Wii/1.5). It builds Link's model from the resident `Alink` archive, reads a
   mailbox (stage, room, position, facing, speed, later animation id), interpolates between the last two
   snapshots, and draws only when the remote player is in the same stage and room. No input, no collision,
   no AI at first (HKMP-style "visible only").
   - It replaces the REL of a profile that is never placed in any stage and never created in code.
     `CO_TEST` is NOT free (d_t_beetle creates it). Picking the slot is build step 1: scan every stage
     archive for placed actor names, grep the decomp for `create(fProfile::X`.
2. **Injection:** modified `rels.arc` in the extracted copy (u8.py) for the puppet code, plus a tiny spawn
   hook: a Gecko C2 code (assembled with the decomp's binutils `powerpc-eabi-as`) that calls
   `dAcObjBase_c::create(PUPPET, ...)` once Link exists in a stage. Gecko codes go in the test user dir's
   `GameSettings/SOUE01.ini`, so nothing in the game files is patched for the hook. Hook site: unknown,
   candidates are the end of the player's execute or the stage-load path.
3. **Bridge:** one process per player next to Dolphin (grown from `ss_mem.py`): reads the local Link at
   20 Hz, writes the remote player into the puppet mailbox, diffs flag arrays every 250 ms.
4. **Sync model** (networking.md 1.1 "client-authoritative with host arbitration"):
   - Own Link: client-authoritative, unreliable snapshots at 20 Hz, interpolated in the puppet.
   - Story, item, scene, dungeon, chest flags: changed u16 words sent as reliable events; the host merges with
     OR (flags only ever get set in normal play) and rebroadcasts. Known multi-bit counters (storyflag_map.h)
     use the host's value instead of OR. Each player keeps their own save; the host's flags win at connect.
   - Enemies, items in hand, health: per player in v1 (each fights their own copies).
   - Cutscenes and dialog: not synced in v1, each player plays their own.
5. **Transport:** v1 plain UDP in the bridge (Python stdlib, no installs), host and join by IP. Works on
   LAN, over Tailscale, or with a forwarded port. v2: Steam relay via Spacewar 480 as the playbook
   recommends, which needs steam_api64.dll from the Steamworks SDK (ask the tester first).

### Unknowns to settle in the build

- Free profile slot for the puppet; hook site for the spawn call; where the bridge finds the mailbox (fixed
  address vs. scanning the actor list for the puppet profile).
- Whether writing flag words from outside takes effect without `commit()`.
- How Link's animations are driven (dAcPy uses several layers); v1 can pick idle/walk/run from speed.

## Phase 5a: static puppet on screen (2026-10-25)

Result: a second Link body (green tunic `al` model, bind pose = T-pose, no head/hands) renders next to the
player in Dolphin 2609a, in the Knight Academy and in Skyloft. Tester confirmed with a screenshot.

### Observed

- **Free slot:** scanned all Stage/Object/Layout/US files (LZ-decompressed) for the 777 names in
  actor_info.cpp: 31 never appear. `J_Test` (profile `J_TEST` 0x14E, REL `d_a_morita_test`, all states are
  empty stubs) is never placed and never created in decompiled code. `CO_TEST` is used (d_t_beetle).
  Residual risk: non-decompiled code could create 0x14E by number.
- **Build:** decomp branch `coop-puppet`: `src/REL/d/a/d_a_morita_test.cpp` includes `REL/coop/d_a_coop_puppet.cpp`
  with `PUPPET_PROFILE=J_TEST`, module marked Matching. Only `d_a_morita_testNP.rel` changes; main.dol and the
  other 635 modules stay byte-identical. `g_profile_J_TEST` sits at `.data:0x0` in both the original and our REL,
  so the unchanged `d_profile` relocation still points at it.
- **REL actors are linked on demand.** `fProfile::sProfileList` (ptr at 0x80575BC0) has null entries for REL
  actors whose module is not linked (J_TEST, NPC_TMN, OBJ_BSTONE were null; ARROW, PLAYER in main.dol are set).
  `fBase_make` returns null for a null entry. `DYNAMIC_NAME_TABLE` (0x804DCBF0, 8-byte entries {u16 relId, pad,
  char *name}) maps relId == profile id (0x14E -> "d_a_morita_test"). `dDyl::isLinked` 0x80052FA0 and
  `dDyl::tryLink` 0x80052FF0 link one module; after tryLink the J_TEST entry was filled (0x807AE6A8).
- **Hook:** Gecko C2 at 0x801BA700 (`dScGame_c::execute`, first instruction `stwu r1,-0x10(r1)`). Dolphin
  placed the code at 0x80002348; the shared block (puppet ptr, cooldown, creates, last event, executes,
  events) is at 0x80002364. The hook passes this address to the puppet as `params`.
- **Lifecycle:** puppets are deleted with the stage, and once more without a stage change (cause unknown).
  The hook now checks `mProfileName` (+0x08) of its stored pointer each frame and recreates the puppet
  (max once per 30 frames). Test run: 3 creates, 5 events, current puppet alive 763 frames in F000.
- **Placement:** a world-X offset put the puppet behind a door after a doorway spawn. Now 100 units along
  Link's facing ((sin y, 0, cos y), same convention as d_a_obj_utajima_island.cpp).
- Gotcha: `addi`/`subi` with rA = r0 read the constant 0. `build_gecko.py` now uses `--fatal-warnings`.

### Next (5b and on)

- Head, face, hands: separate models attached to body joints by callbacks in `daPlayerModelBase_c`
  (d_player_mdl.cpp: sBodyMdls/sHeadMdls/sFaceMdls/sHandMdls, headModelTimingB). Idle animation ("waitS").
- Outfit: tester's Link wore the Skyloft clothes (`pl_body` set) while the puppet used `al`; follow the remote
  player's tunic type.
- Keep a short event history in the shared block to explain the unexplained deletion.
- Then drive the puppet from the bridge (mailbox in the shared block) using a second Dolphin.

## Phase 5c: puppet follows a second Dolphin (2026-10-25)

Result: two modded Dolphin 2609a instances on one PC (own `-u` user dirs, A and B, each with a copy of the
tester's save), `mod/bridge/local_bridge.py` attached to both. Each window shows the other window's Link as the
T-posed puppet; tester confirmed with a screenshot of both windows.

### Observed

- Shared block grown to 0x40 bytes (layout: `struct Shared` in mod/puppet/d_a_coop_puppet.h). Bridge writes
  position (3 f32) and facing (s16) at +0x20/+0x2C, flags at +0x1C (1 bridge, 2 same stage), sequence at +0x18
  last. Both instances placed the hook block at 0x80002364; the bridge derives it from the branch at
  0x801BA700 and checks the first hook word (0x9421FFC0) instead of assuming.
- `pair_status.py` while both stood in F000: puppet-to-other-Link gap 0.0 units in both directions, mail seq
  ~6.5k (60 Hz), one create per side, no deletions (history: create ok).
- Puppet glides half the remaining distance per frame, snaps above 600 units, hides when the stages differ,
  stands still when no bridge is attached. Facing is copied directly (no angle smoothing yet).
- Not yet tested: stage change while the bridge runs (hide/reappear), different rooms of one stage, the
  unexplained deletion (did not happen this session).
- Test was the tester driving one window by hand; no bot yet (playbook asks for one).

## Phase 5d: networked bridge, host and join over UDP (2026-10-25)

`mod/bridge/net_bridge.py`: one process per player next to their Dolphin. Host binds UDP (default 27645),
joiner sends HELLO until WELCOME, then both send STATE at 30 Hz (stage char[8], room, has_link, pos, rotY;
header "SSC1", type, flags, seq). Stale/out-of-order STATE dropped by seq (32-bit wrap). Peer silent for
2 s, different stage, or own Link missing -> puppet hidden (flags = bridge only). BYE on exit clears the
peer; host accepts a rejoin. Each side clears its own mailbox on exit. Game access split into `game_side.py`.

### Observed

- `test_net_bridge.py --chaos` (fake game sides, real loopback UDP): 15/15 pass. Covers codec + garbage,
  seq wrap, both-way state, moving target, different stage, own Link missing, timeout, out-of-order, garbage
  from the peer, BYE + rejoin, host started after the joiner, and bad_wifi through
  coop-anything/tools/chaos: received 198 of 228, 16 stale, target error p50 13.9 / p95 22.5 units for a
  Link moving 200 units/s, never hidden.
- Real games, loopback, no chaos (40 s): handshake at once, gap 0.0 both ways, 1064 of 1065 received,
  clean BYE ("peer left") and mailboxes cleared on exit.
- Real games through chaos relay `bad_wifi` (host A <- relay :27646 <- join B), ~3.5 min: host received
  5702 of 6323, joiner 5685 of 6324, ~400 stale per side, no disconnects. Tester walked one Link around and
  reported the puppet in the other window "looked smooth".
- Not tested: two real PCs (host needs `--bind 0.0.0.0`, which triggers a Windows Firewall prompt the tester
  must answer), stage changes while networked, NAT/Tailscale, a bot driving the movement.

## Phase 5e: story, item and scene flag sync (2026-10-25)

`mod/bridge/flag_sync.py` + FLAGS/FLAGS_ACK in net_bridge.py. Each side polls its flags at 10 Hz and sends the
changed bits (set mask, clear mask) per u16 word; the receiver replays exactly those bits, so two players setting
different bits of one word both keep them. Reliable and ordered: one message in flight, resent every 250 ms
until acked, applied once. New connection: both send a full snapshot, merged with OR.

### Derived from the decomp (then checked)

- During gameplay the current SaveFile is `mFileA`: `isFileInactive()` is true only on the TITLE scene
  (file_manager.cpp).
- `commit()` copies the whole live array over the save copy, so every applied change is written to the live
  array AND the save copy. Scene flags: save has 26 scenes x 8 words (`scene_flags[208]`), live
  `sSceneFlags` holds the current scene (index `SaveFile::scene_flag_index` +0x5318); other scenes are
  written to the save copy only and loaded on entry (`copyAllFlagManagersFromSave`).
- Clears travel for story and item only. Scene words switch source at area changes, so only set bits travel.
- Not synced yet: dungeon flags (save layout `dungeon_flags[8][22]` vs 8 live words unclear), chest (tbox) and
  enemy flags, skip flags. Counters stored in story/item words are OR-merged by the connect snapshot:
  players should start from the same save.

### Observed

- Real games, loopback: story action in A only -> B received 3 changed words, 0 resends; both identical after,
  B's save copy equal to its live array. No dialogue or event appeared in B (tester confirmed).
- A temporary story flag cleared by the game after dialogue was mirrored to the other side as a clear.
- **Bug found by the tester's reset test:** Dolphin Emulation -> Reset in B sent a clear for story[6] bit 3 to A
  (lost in both). Likely cause (not proven): the flag arrays were zeroed while `LINK` still held its old value,
  so the bridge thought B was in play. Fixes: (1) `in_play()` = LINK points at an actor with profile PLAYER
  (0xC8) and a stage name is loaded; (2) after any load, and when more than 2 words clear in one poll,
  no clears are sent and the peer is asked for a full snapshot again (RESYNC). Lost flag restored by writing
  it into A; the bridge carried it to B.
- Retest after the fix: B saved at a bird statue, Reset, loaded: A unchanged, both identical to before,
  B's bridge logged three loads and asked for a resync each time.
- **Persisted:** B's new `wiiking2.sav` (0xFBE0 bytes = SavedSaveFiles, region "SOUE", slot 0) has
  story[6] = 0x0008, which B only ever got over the bridge; its old save did not. Save slot 0 story/item flags
  equal B's live game. So `SavedSaveFiles` layout from the decomp matches a real save.
- Tests without the game: test_flag_sync.py 19/19 (incl. bad_wifi random churn converging, reload of an older
  save, wipe without load), test_net_bridge.py 15/15 over 15 repeated runs after fixing a racy test.
  Deliberate breakages caught: no resends, no anti-echo baseline, no load protection, no mass-clear brake.

## Phase 7: player package v0.1 (2026-10-25)

`release/ss-coop-v0.1.zip` (23 KB, no game files): setup.ps1 (checks disc ID/revision, main.dol SHA-1 and
original rels.arc SHA-256, extracts with DolphinTool, swaps in the puppet REL via u8.py and checks the patched
archive hash, writes the Gecko ini + EnableCheats into its own Dolphin user dir, optional save import),
play.ps1 host|join (starts Dolphin, waits for the hook, runs net_bridge.py), README.md guide.

### Observed

- Fresh-folder test on this PC (package + copied Dolphin 2609a + tester's RVZ): setup 11 s, both play.ps1 host
  and join found the hook at 0x80002364 and connected over loopback (port 27700).
- **Save import bug found and fixed:** copying only `wiiking2.sav` made the game write a new empty save at the
  title screen (all flags 0). A save is three files (`wiiking2.sav`, `skip.dat`, `banner.bin`); with all three
  the save stayed byte-identical through a 45 s boot. setup.ps1 now copies the folder and backs up the old one.
- Not yet tested: two real PCs over the internet (tester hosts with UDP 27645 port-forwarded on the router).
- **First real two-PC session (2026-10-25, afternoon):** tester hosting via `2 Host.cmd` with UDP 27645
  port-forwarded on the router, a second player joining over the internet via `3 Join.cmd` with their own SOUE01
  dump and the host's save. Tester reported: "We're connected and can see each other in the game."
  Observed by the tester only; no bridge logs from this session (the windows print to the console).
- Double-click launchers: `1 Setup.cmd` (file dialog for the dump, `host-save\` picked up on first setup
  only), `2 Host.cmd`, `3 Join.cmd` (asks for the IP, remembers it in last-host.txt, adds port 27645).
- No-Python build (tester-approved install: PyInstaller 6.22.3 from PyPI into `tools/exe-build` venv, with
  altgraph 0.17.5, packaging 26.3, pefile 2024.8.26, pyinstaller-hooks-contrib 2026.9, pywin32-ctypes 0.2.3,
  setuptools 84.2.1). `release/build/build_exe.ps1` bundles all helpers into `sscoop.exe` (onefile, 9.6 MB,
  unsigned; subcommands bridge/u8/cheats/wait). Fresh-folder test with Python removed from PATH: setup, hook
  detection and host/join connection all worked.

## Phase 5f: puppet looks, v0.2 (2026-10-25)

Puppet build 8: body, head, face, hands from "Alink" (tunic `al_*` or Skyloft clothes `pl_*`, chosen by the
remote Link's `mTunicType` at dAcPy+0x120B, sent by the bridge; STATE packet + tunic byte, magic SSC2), stand/walk/
run (`waitS`/`walkS`/`dashS` from PLCommonAnime's External folder, unpacked by calling the local Link's
`daPlayerModelBase_c::getExternalAnmChr` 0x800603B0), picked from the puppet's own smoothed speed.
Verified with screenshots in Dolphin: standing, mid-stride walking and running poses, Skyloft clothes, head/hands
in place.

### Observed (each one a bug found on the way)

- Head and face take the body's `head` node (4) world matrix; hands copy body nodes 9/14 into hands nodes 1/2 in an
  ICalcWorldCallback (as `handsCallbackC`). Worked first time.
- **Hook liveness check fooled:** after a deletion the freed memory still held profile 0x14E, so the hook never
  respawned. The puppet now clears the hook's slot in `doDelete` and sets a 60-frame cooldown.
- **T-pose in the outfit the local Link wears:** the local Link sets `envelope_mtx_mode = APPROX` on the shared
  ResMdl of its own body (`loadBodyModels`); RAM showed mode 1 on `pl_body`, 0 on `al`. Our ScnMdl then stays in
  the bind pose. Fix: copy the whole Alink BRRES (~820 KB) into `dHeap::workExHeap` and set NORMAL in the copy.
  A copy of only the MDL0 block failed `AnmObjChr::Bind` (node names live in the BRRES string pool) and does not
  fit work1Heap anyway.
- **T-pose when walking/running:** `m3d::anmChr_c::setAnm` detaches the animation and re-attaches it only under a
  blend node (the player's case). Fix: call `mdl.setAnm(anm)` again after switching. PLAY_MODE_0 loops.
- Savestate dev loop (boot `-s state`, swap the REL bytes in RAM) did **not** work: the linked module kept the old
  code although neither the RAM copy of rels.arc nor the disc had it any more (`.text` 0xD74 vs 0xDE4, build
  marker missing). Cause unknown. Fresh boot + tester loading the save was used instead; screenshots via
  `devloop.py shot` (PrintWindow) and a fake remote via `devloop.py walk` let the agent check every build alone.
- Package v0.2: `release/ss-coop` (folder without version, update by copying over), `setup.ps1` update mode (no disc
  when `game\rels.arc.original` exists). Update over the tester's v0.1 install: save byte-identical, hook and
  module replaced, patched rels.arc matches the manifest.

### Not tested

- Outfit over the real network bridge between two games (unit tests only), v0.2 between two PCs, Silent Realm
  outfit (falls back to tunic), animations for jumping, climbing, swimming, flying, sword.

## Phase 5g: dungeon flags (small keys) and chests, v0.3 (2026-10-25)

### Derived from the decomp

- Dungeon flags: `DungeonflagManager::commit` writes the 8 live words (`sDungeonFlags` 0x805A9C58) to the save at
  `(mStageIndex & 0x1FFF) * 8`; `copyFromSave(flag)` is called with the scene flag index
  (`copyAllFlagManagersFromSave`, `updateFlagForFlagIndex`). SaveFile `dungeon_flags[8][22]` (+0xA64) = 22 entries
  x 8 words. Holds counters (small keys) via FlagIndex definitions.
- Chests: `TBoxflagManager` keeps 2 live words (`sTBoxFlags` 0x805753F4), commits to the save at `mSceneIndex * 2`
  (+0x2A64). `getFlagCount()` = 0x200 bits = 32 words = scenes 0..15; synced as 32 words (nothing past it).

### Observed

- Live `sDungeonFlags` equals save entry 0 in Skyloft (scene index 0), so entry = scene index holds.
- Clear rule: dungeon clears are sent only for the current scene's entry and only if the scene index did not change
  since the last poll (small key counter goes down on use; a stale save word at an area change must not remove a
  key). Scene and chest flags stay sets-only.
- Tests without the game: 22/22 (key pickup and use, stale word on scene change, chest in another scene). Mutants
  caught: dungeon clears always allowed; dungeon clears never sent.
- Real games (two Dolphins, net bridge over loopback): synthetic bit in A's live dungeon word reached B live and
  B's save copy; clearing it in A cleared both in B; chest bit set in B reached A live and save copy. All values
  restored afterwards. A real dungeon has not been played with it yet.
- Protocol magic SSC3: an SSC2 peer rejects FLAGS entries with array ids 3/4, never acks, and the ordered flag
  channel would stall. Package v0.3 (`release/ss-coop-v0.3.zip`); tester's install updated, save unchanged.

## Phase 5h: real animations from the remote Link, v0.4 (2026-10-25)

Puppet build 11 plays the remote Link's own animations: `daPlayerModelBase_c::mAnimations[0..1]` (+0x121A, u16
sAnimations index; slot 0 lower body, slot 1 upper body, 443 = none) and the frames of `mAnmChrs[0..1]`
(+0x728 + i*0x38, `fanm_c::mCurrentFrame` +0x30), sent in STATE (magic SSC4) and written to the shared block
(now 0x60 bytes, +0x40 anim[2], +0x44 frame[2], mail flag 4). The puppet blends two `anmChr_c` in an
`anmChrBlend_c` like `loadBody`/`loadAnmChr` (slot 0 bound WAIST partial + CENTER one, slot 1 BACKBONE_1 partial),
loads each animation from PLCommonAnime's External folder (`<name>.rchaC`, resident from boot) into its own
buffer (4 x 0x2400 per slot), and takes the remote frame on every new mail.

### Observed

- sAnimations at 0x804DF2FC (443 x 12 bytes, name pointer first); idle Link reads 26 (`waitS`) in all slots.
- Fixed-animation showcase on B's puppet (screenshots): rollF, jumpSt, cutT on the upper body over waitS on the
  lower body, all posed correctly; climbHang and swimwait sink the body into flat ground as they should.
- Real path: tester rolled, jumped and swung the sword in A while B was recorded every 1.5 s. Received in B:
  atsDL, rollBSt, rollBEd, atsJRSt, waitBtoA, each with a matching puppet pose in B's screenshot.

### Bugs found on the way

- `fanm_c::setFrame` is not in the game binary: the REL link failed, and build_mod.ps1 still said "Build OK" with
  the previous REL. Fixed: `getAnimObj()->SetFrame`; build_mod now builds the REL target explicitly and checks it
  is newer than its object.
- `getExternalAnmChr` may return an animation inside the local Link's `mCurrentRes` (situational resource) that
  the game can unload; the puppet loads only from PLCommonAnime into its own buffers and keeps the old animation
  when a name is not there.
- **Hang on the second puppet (outfit change):** progress markers in Shared::reserved[0] showed the hang at the
  animation-buffer allocation in `workExHeap` right after the ~820 KB BRRES copy. Heap walk (`heap_free.py`,
  RVL ExpHeap free list): work1Heap ~1.2 MB free, workExHeap ~850 KB, work2Heap 334 KB. A failed allocation
  hangs the game. Fix: copy only the MDL0 block (~87 KB) and rebase every name offset that points outside it
  (13 dictionaries' ofsString, ResNodeData/ResMatData::name, toResFileData, name) to the resident original;
  copy and animation buffers live in the actor heap. workExHeap is no longer touched.
- One tester Shift+F1 did not save (game picture not focused) and a test ran on the old state's build; the agent
  then killed the tester's window with the new build. Now: check the state file's timestamp first.

## Phase 5i: sword, sheath and shield on the puppet, v0.5 (2026-10-25)

### Derived from the decomp (zeldaret/ss, d_player_mdl.cpp/.h, asm of d_player_act / file_manager)
- daPlayerModelBase_c keeps mSwordMdl +0x598, mSheathMdl +0x654, mShieldMdl +0x5E0 (m3d::smdl_c), mMainMdl
  +0x3E0. Models come from archive "PLCompItem", file `dat/<name>.brresC` (getItemResFile, public static).
- Sword: daPlayerActBase_c::sCurrentSword (u8, 0x80571C74), set by getCurrentSwordType from item flags 10..14;
  6 = no sword. Name: getSwordName(idx) (0x8005C160); sheath model in the same file: EquipPodA..E, EquipPodMaster.
- Shield: SaveFile+0x53B1 = equipped pouch slot (FileManager::getShieldPouchSlot), pouch = 8 x u32 at
  SaveFile+0x7C0 (getPouchData), item id in low 16 bits; shield id = item - 116 (0..9), type = id / 3
  (Wood, Iron, Holy, Hylia), texture-pattern frame id % 3 = variant.
- m3d::scnLeaf_c holds its g3d ScnObj at +0x14. Item placement code (dAcPy_c) is not decompiled, and the body's
  world matrix array is indexed by mtx id, not node id (head node 4 did not match entry 4), so no node math.

### Design
- Bridge reads each model's MTX_WORLD (ScnObj+0x3C) relative to the body's MTX_LOCAL (+0xC), quantized s16
  (rotation x 8192, translation x 16), plus a "drawn this frame" bit: the ScnObj's G3dObj parent (+0x4) is set
  only while it is in the scene root (m3d::scnLeaf_c::entry -> pushBack, cleared every frame).
- Wire: SSC5, STATE + 76 bytes. Mailbox: hook block grown 0x60 -> 0xB0, Shared::equip* at 0x60, MAIL_EQUIP = 8.
- Puppet build 12: loads the same models at create (own loader with null checks, actor heap), places them at
  its own world matrix x the relative matrix, draws only the bits set. Sword/shield change = respawn
  (event 11), load failure = event 12 and no model.

### Observed
- Live A (early save, Skyloft clothes): head/face/hands/body have a parent, sword/sheath/shield none; sword 6,
  no shield. Setting the practice sword item flag + UPDATE_MODEL_SWORD did not change the models until an area
  reload (not done), so real sword capture is untested.
- Synthetic mailbox in B (made-up matrices): Goddess Sword + sheath + Wooden Shield drawn on the puppet
  (screenshot); combos (5,9) (0,5) (4,7) each respawned once (event 11), no event 12; work1Heap free 925 KB with
  equipment loaded (1072-1081 KB without).
- Bot QA build 12: RESULT: PASS, 15 gates incl. new "equipment" (A's equip bytes reach B's mailbox unchanged;
  in this save that is the nothing-drawn case).
- Build script: the "first run after a change fails" quirk was dtk coloring FAILED (": ESC[31mFAILED"); colors
  are stripped now.

### Not tested / gaps
- Real sword in hand and on the back from a live Link (needs a save where Link carries the sword).
- Held items (bow, slingshot, beetle, ...): three more smdl_c at dAcPy_c +0x169C/+0x16B8/+0x16D4 next to
  mHeldResFile +0x16F0; their source archive is unknown.
- Sword material colour animation ("appearance" from Alink) is not applied on the puppet.

## Phase 5j: defeated enemies, v0.6 (2026-10-25)

### Derived from the decomp
- EnemyflagManager::sEnemyDefeatFlags (0x805A7AD8, 4096 u16) live; committed copy at SaveFile+0x32F4
  (getEnemyFlags0/1). EnemyflagManager::checkFlag reads the committed copy; room loading
  (d_stage_parse.cpp handleBzsObj/Sobj/Stas) skips objects whose bit is set.
- Bit id = (room << 10) | (placement id & 0x3FF) (getEnemyDefeatFlagFromId), stored in dAcBase_c::mObjID
  (+0xB2) at create. Same in both games for the same stage.
- Bits are set by dAcBase_c::killWithFlag / deleteWithFlagNoItemDrop / preDelete (item-dropping objects) and wiped
  on every scene change (updateFlagIndex -> clearAll): enemies come back after leaving an area.

### Design
- ENEMY message (SSC6): stage name + non-zero (index, bits) words, resent every 0.5 s and at once on a change; no
  acks. Receiver ORs only when in the same stage for >= 1 s (SETTLE); bits are never cleared by sync. Both live and
  committed copy are written.
- Live removal: hook block 0xB0 -> 0xD0 with killSeq/killAck/kills[8]; the bridge hands new remote bits to the
  puppet 8 at a time and waits for the ack. Puppet build 13 walks fManager_c's ACTOR and STAGE groups and calls
  deleteWithFlagNoItemDrop on matching mObjID (event 13). The enemy vanishes in the other game (no death
  animation, no drop there).
- mod/tools/actor_list.py lists live actors from fManager_c::m_connectManage (0x805B84D8) with their mObjID.

### Observed
- Unit tests: test_enemy_sync.py 12 pass (same stage only, settle, area change drops pending kills, batching
  waits for the puppet, never clears, resend, wire format, two bridges over UDP).
- Live (Skyloft F000, no enemies near): set the bit of OBJ_FLAG 0x00E7 in A -> 0.2 s later gone from B's actor
  list, bit live + committed in B, one event 13, kill ack.
- Bot QA build 13: RESULT: PASS twice with new gate "enemy kill". One run in between never started (games not
  hooked within 60 s while the exe was being built at the same time) and crashed the runner without a RESULT line:
  startup is now its own gate ("games start", 90 s), so that case reports RESULT: FAIL. Mutation twin `no_enemy_apply`: only
  "enemy kill" TIMEOUT, RESULT: FAIL.

### Not tested / gaps
- A real enemy killed by sword (no enemies near the test save's spot, and no button input).
- Enemy movement, health and attacks are not synced; each player sees their own living enemies.
- Objects other than enemies that use the same bits (item-dropping breakables) are shared too: untested.

## Phase 5k: held items on the puppet, v0.7 (2026-10-25)

### Derived
- dAcPy_c has three more m3d::smdl_c at +0x169C, +0x16B8, +0x16D4 next to mHeldResFile +0x16F0; every store to
  +0x16F0 in d_a_player.s comes from daPlayerModelBase_c::getItemResFile, i.e. PLCompItem dat/*.brresC.
- PLCompItem (inside Object/ObjectPack.arc.LZ) lists 30 files; held items: EquipBeetle, EquipBottle (16 models),
  EquipBowA/B/C, EquipHarp, EquipHookShot (+Chain, +Tip), EquipMoleGloveA/B (L, R), EquipNetA/B,
  EquipPachinkoA/B, EquipStole, EquipVacuum, EquipWhip (Grip, Tip): 37 models, table in mod/bridge/held_models.py
  and mod/puppet/held_models.inc (names only).

### Design
- Bridge: per slot drawn bit (G3dObj parent, as for equipment), model index from the ResMdl name (+0x48), body-
  relative matrix. SSC7, STATE + 76 bytes; hook block 0xD0 -> 0x120 (Shared::held*), MAIL_HELD = 16.
- Puppet: own 0x30000 frame heap from work1Heap (like the player's LinkSword/LinkShield heaps); reloads only when
  a drawn set differs from what is loaded (event 14), keeps it when the item is put away.

### Bugs found on the way
- Build 14 crashed B (invalid read in EGG::FrmHeap::free, this = null): a fresh mHeapAllocator_c points at
  mHeap::g_assertHeap, so "getHeap() == nullptr" never created the heap and toFrmHeap(assert heap) was null.
  Build 15: own mHeldHeapReady flag + null guard.
- Build 15 placed held items only if the mailbox also had equipment (early return in placeParts). The bridge
  always sends both, a synthetic test did not. Build 16 fixes the early return. Checked in the game: held-only
  mailbox (bow) drawn on the puppet; bot QA build 16 RESULT: PASS (18 gates).

### Observed (build 15, synthetic mailbox in B)
- Bow, clawshot set, bottle: one load each (event 14), put away + take out again: no reload; B alive, work1Heap
  free 883 KB with the held heap. Pictures: bow; whip parts + Gust Bellows drawn on the puppet (made-up positions).
- Bot QA build 15: RESULT: PASS, 18 gates incl. new "held items" (nothing held in this save). Mutation twin
  `no_held`: only "held items" TIMEOUT.

### Not tested / gaps
- A real held item from a live Link (the test save owns none). Moving parts (bow string, whip, clawshot chain)
  are static at the hand.

## Phase 5l: live enemy position and health, v0.8 (2026-10-25)

### Derived
- fBase_c DISABLE_EXECUTE (mProcControl +0xF) would stop an enemy entirely, but its model is only recalculated
  in its own execute, so a stopped enemy cannot be moved from outside without per-class code. Chosen instead:
  the enemy keeps running in both games; the non-tester pulls position/facing to the tester's every frame.
- dAcEnBase_c::mHealth at +0x342 (u16, public). 56 E_* profiles in f_profile_name.h; 46 shared (parts and
  projectiles such as arrows, cannon shots, tails, leaves, wires, shields, rocks, and E_SIREN excluded).
- Skyloft F000 by day has two E_REMLY (0x170, Remlits) with placement ids 0x1CA and 0x1D1, health 100.

### Design (coop-anything playbook actor-ownership.md, cut to two players)
- Puppet build 17 lists placed shared enemies each frame into an EnemyTable in its heap (address in
  Shared::enemyTable +0x120) and applies orders (FOLLOW pose, health cap). Shared::testProfile +0x124 for dev.
- enemy_motion.py: host decides tester = nearer Link (hold 1.5 s, challenger 25 % nearer), epoch from one counter
  that never goes down; OWN (8) repeated, joiner keeps only newer epochs; MOTION (9) used for following only from
  the tester at exactly the known epoch; health = min of both, no ownership needed. Same stage + 1 s settle.
- Handoff without Transfer/Ack/Commit: synced state is only pose, which the new tester already shows (it was
  following). Noted as a deviation from the playbook for this state.

### Observed
- test_enemy_motion.py: 15 pass (tester choice, handoff epoch, no flapping, hold time, replayed old OWN ignored,
  past-tester MOTION ignored, mutation twin "no_epoch" caught, health both ways, stage guard, wire, UDP pair).
- Bot QA build 17 (real Remlit 0x1CA): tester A, B's copy followed a 30-unit nudge (gap 0.0); B's Link moved next
  to it, tester B, A's copy followed (gap 0.0). RESULT: PASS twice, 19 gates. First version of the gate assumed the
  host wins ties and failed; the gate now finds the tester first.

### Not tested / gaps
- Animations and attacks are not synced (each copy runs its own AI); enemies attack only their own game's Link.
- Health merge on a real hit, and a real fight, untested (no button input). Flying/burrowing enemies untested.

## Phase 5m: enemy animation for Keese, ChuChu, Remlit, Skytail, v0.9 (2026-10-25)

### Derived
- Decomp coverage: E_KS (Keese, matching), E_REMLY, E_SM, E_SKYTAIL are worked out; E_BC (Bokoblin) and most others
  are stubs. These four keep model + animation in a d3d::AnmMdlWrapper (m3d::mdlAnmChr): KS +0x388, SM +0x378,
  REMLY +0x3BC, SKYTAIL body +0x378. Vtables: AnmMdlWrapper 0x80500A20, AnmMdlWrapper2 0x805009D0.
- mdlAnmChr::setAnm(name) looks the name up in its anim file, then its model file; the wrapper's virtual setAnm
  also loads the animation's sounds. The g3d pose is computed in the scene calc after all actors executed, so the
  order of our actor relative to the enemy does not matter.
- ResFile::GetResAnmChrNumEntries / GetResAnmChr(index) are not in the binary (REL link failed on them); only
  GetResAnmChr(name). The puppet reads the BRRES "AnmChr(NW4R)" dictionary itself for index <-> name.

### Design
- EnemyEntry/EnemyOrder grow to 0x1C: anim (dictionary index, bit 15 = model file), play mode, frame; order flag
  ORDER_ANIM and frameSeq. The follower switches animation when its index differs and jumps to the tester's frame only
  when frameSeq changes (new MOTION), playing on by itself in between. Offsets are used only if the vtable at that
  offset is an AnmMdlWrapper(2). Protocol SSC9 (MOTION + anim u16, mode u8, frame f32). Dev command
  Shared::testAnimObj/testAnimIdx (+0x128) plays an animation on one enemy, for QA.

### Observed (build 19, Skyloft F000)
- Both Remlits report animation 0x13 in both games (offset + dictionary walk correct in the real game).
- test_enemy_motion.py 19 pass (4 new: tester animation/frame reach follower, frame jumps only on new motion,
  unknown type gets no anim order, twin no_enemy_anim caught).
- Bot QA: RESULT: PASS, 21 gates; "enemy animation": tester A switched Remlit 0x1CA 0x13 -> 0x0, B's copy followed
  (frame difference 2.0). Twin `no_enemy_anim` in the game: only "enemy animation" TIMEOUT.

### Not tested / gaps
- Keese, ChuChu, Skytail offsets only from headers (vtable check guards them); no such enemy in the test save.
- Enemies damage only their own game's Link; other enemy types animate separately.

## Phase 7b: Xbox controller layout, v0.9.1 (2026-10-26)

- Players have Xbox controllers, no Wii Remotes. Dolphin's emulated Wii Remote + Nunchuk + MotionPlus is mapped to
  an XInput gamepad (`XInput/0/Gamepad`, any Xbox controller on Windows): left stick = Nunchuk stick, right stick =
  Swing directions, right stick click = Swing forward (stab), LT/LB = Z/C, RB = Nunchuk shake, pointer (IR) on the
  mouse. Based on the layout the tester set up and played with on an Elite 2 (SDL device), generalized.
- Package: `controls\Xbox controller (ss-coop).ini` (Dolphin profile), `controls.ps1` (setup adds the profile and uses
  it only if `WiimoteNew.ini` does not exist; `-Apply` / `4 Xbox controls.cmd` replaces it with a timestamped backup).
- Observed: the script in a scratch folder (fresh, existing kept, -Apply with backup); every XInput input name used
  is present in Dolphin 2609a's binary. Not tested: how it plays (no person has used this exact profile yet), the
  spin attack mapping.

## Phase 5n (in progress): reaching hostile enemies for the bot (2026-10-26)

- Puppet build 20: dev/QA warp (Shared::warp* at +0x12C, mod/tools/warp.py) calls dScGame_c::triggerEntrance once.
  Works: F000 at night (night sky, puppet respawned), F100 Faron Woods by day.
- Teleporting Link ~4000 units in one write loses him (he reappeared at the entrance, camera stayed): move enemies to
  Link instead, never Link far.
- Skyloft at night in this early save has only the two Remlits, and they still take the day branch (ANM_Sleep 19).
  A night Remlit attacks when Link is within mPatrolAreaSize (1000) of its mStartingPos (dAcObjBase_c +0x1FC), so
  moving the Remlit alone never triggers it. No Keese at night in this save.
- Faron Woods (F100 room 0) has 7 placed Octoroks (E_OR, ids 0xDC..0xE2). Entering it the first time starts Fi's
  dialogue, which needs a button press; the bot cannot continue past it. Plan: tester clicks through it once in both
  test windows and makes savestates there.
- Done (tester pressed through Fi's dialogue, savestates in F100). Moving an Octorok and its mStartingPos next to Link
  did not make it attack (Octoroks pop up from their mound when they see Link). Guided session: tester walked COOP A,
  B's Link was kept 80 units next to A by the script; ownership of the nearest Octorok followed (B owned it while B's
  Link was nearer). The reachable part of F100 room 0 ends at a story-locked wall of trunks; all Octoroks sit on
  ledges 1700-2300 units higher that cannot be reached this early. No hit recorded, so whether the non-tester's copy
  hits its own player is still unknown. Enemy hits on the other player wait for a save with reachable hostile
  enemies.

## Phase 6: bot-driven QA (2026-10-25)

`mod/qa/coop_qa.py [--expect-build 11] [--keep-open] [--mutant NAME]`: two test Dolphins boot from their
savestates (`start_test.ps1 -FromState`), host and join bridges connect over loopback on a free port, and the bot
drives both games through memory only. One line `RESULT: PASS|FAIL`, TIMEOUT kept apart from FAIL, JSON in
`game/qa-out/result.json`, bridge logs next to it. Cleanup: bridges stopped, Dolphins killed and checked gone,
test words zeroed in both games, tester's saves (the play folder and the original Dolphin) hash-compared before/after.

Gates: scorer self-test; boot (puppet build byte at block+0x2F == expected, Link + flags readable, same stage,
game advancing); bridges connect (mail flags 7 both sides); position A->B and B->A (teleport Link +60 X by
writing mPosition, precondition: it stuck; puppet within 10 units in 4 s; restored); facing + animation (A's
rotY and mAnimations[0..1] in B's mailbox); story flag set/clear (free word, live and save copy); small key +
chest (dungeon word scene*8+6 A->B set and clear, tbox word scene*2+1 B->A); mass-clear brake (3 story words
wiped in B must not clear A, B gets them back); outfit respawn (toggle A's mTunicType: B's puppet logs event 6
then 1, twice, B keeps running, work1Heap free unchanged within 64 KB); disconnect + reconnect (kill join: A
mail flags drop to 1 = hidden; restart: 7 again); invariant monitor (5 s: puppet-Link gap < 25 both ways, no
puppet events, both games advancing).

### Observed (runs on this PC, SOUE01 rev 0, Dolphin 2609a, puppet build 11, both in F000 Skyloft)
- 2 consecutive full runs: RESULT: PASS (14/14 gates incl. cleanup). Puppet gap after teleport 0.0-0.2 units,
  monitor worst gap 0.0, work1Heap free 1081 KB before and after two respawns.
- Mutation twins (`COOP_MUT` env var, read only by the bridge, test-only): `no_position` (mail position frozen)
  -> both position gates FAIL (gap 60 / 63.5); `no_flag_apply` (peer flags acked but not written) -> story,
  key+chest, mass-clear gates TIMEOUT; `no_mass_clear_brake` -> mass-clear gate FAIL (A lost [0,0,0]).
  Each mutant caught by exactly the intended gates; every run still ended RESULT: FAIL.

### Bugs found in the runner itself (fixed)
- Liveness first diffed 4 MB of low RAM: only 1-4 words change there, so a running game looked dead. Now: puppet
  execute counter or Link's animation frame advances.
- A perfect match (gap 0.0) was falsy in the poll and timed out. Poll results are now tuples.
- Teleport precondition `|dx| < 2` failed when Link slid 3.5 units on a slope; now "moved > 40 from start".

### QA gaps (not covered by the bot)
- No button input: rolls, jumps, sword swings, doors, loading zones, cutscenes, dialogue are not driven. The
  animation gate only checks the idle animation (26 waitS) gets through; real moves were checked by hand
  (Phase 5h recording).
- One stage (Skyloft, F000), loopback network only (chaos relay is covered in the unit tests, not here).
- Flags are written synthetically into free words, not set by real game events; no real dungeon visited.
- Dolphin's reset/reload path and stage changes are not exercised.

## Tools

- `mod/tools/ss_mem.py` (outside this hub; will move to the game's own repo): stdlib-only read-only reader.
  `status`, `watch`, `flags OUT.json`, `diff A.json B.json`. `SS_DOLPHIN_PID` selects a Dolphin instance.
- `mod/tools/u8.py`: U8 archive list/unpack/pack (pack keeps the original layout via `--like`).
- `mod/tools/nlz.py`: Nintendo LZ77 (0x10/0x11) decompressor. `mod/tools/puppet_status.py`: hook block + puppet.
- `mod/scripts/build_mod.ps1`: build the puppet REL, check only it changed, repack rels.arc from the backup.
- `mod/scripts/start_test.ps1 -PuppetProfileHex 14E`: test Dolphin with its own user dir (copy of the tester's
  controls and save), hook installed, boots `game/extracted`.
- `ss_mem.py` can now write MEM1 (`Dolphin(writable=True, pid=...)`).
- `mod/bridge/local_bridge.py PID_A PID_B`: two-way local bridge (60 Hz), clears mailboxes on exit.
- `mod/tools/pair_status.py PID_A PID_B`: puppet vs. other Link gap, mail seq, event history per side.
- `start_test.ps1 -Instance B`: second test user dir; B copies A's save.
- `mod/bridge/net_bridge.py --pid PID host [--bind 127.0.0.1] | join HOST[:PORT]`: networked bridge.
  `mod/bridge/test_net_bridge.py [--chaos]`: its tests without the game.
- `mod/bridge/flag_sync.py`, `test_flag_sync.py [--chaos]`: flag sync core and tests (fake game with commits).
- `mod/tools/flag_compare.py PID_A PID_B [--dump F]`, `mod/tools/save_flags.py OLD.sav NEW.sav [--slot N]`.
- `mod/scripts/label_windows.ps1 -PidA -PidB`: titles "COOP A/B" on all windows of each test Dolphin.
- `mod/qa/coop_qa.py [--mutant no_position|no_flag_apply|no_mass_clear_brake]`: two-window bot QA (Phase 6).
- `mod/tools/actor_list.py PID [--all]`: live actors with placement ids, nearest first.
- `mod/bridge/enemy_sync.py`, `test_enemy_sync.py`: enemy defeat sync core and tests.
- `mod/bridge/enemy_motion.py`, `test_enemy_motion.py`: enemy ownership/motion core and tests.
