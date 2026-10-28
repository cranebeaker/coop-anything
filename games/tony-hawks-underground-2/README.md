# Tony Hawk's Underground 2 (2004)

| | |
|---|---|
| Engine | Neversoft engine (THPS4 / THUG lineage), QB (QScript) bytecode scripts in `.prx` archives |
| Versions tested | PC, English. `THUG2.exe` SHA-256 `52fc88849654b34839ec2f96bff3a8c0b7a855df9a207aab9f2fca2e6bd440f3`, run as THEMOD 1.3: the tester's `THEMOD 1.3.exe` SHA-256 `2b1ac8a9282c7817d6999967d0d47bfabaf8b29ac220bbff9f634e107f1095c3` (see "THEMOD" below) |
| Projects | [thug2-coop](https://github.com/LeftGamer1/thug2-coop) (story co-op, this write-up) |
| QA level | community-reports (the tester and a friend play it online); a scripted two-client walkthrough also runs, see QA |
| Last updated | 2026-10-26 |

## Where things stand

Two people can play the Story Mode together online today. The host plays their own story and save; the friend
joins the host's game and skates in the same level with the NPCs, cutscenes and goals. It is alpha: the tester and a
friend play it over the internet and report bugs, and a full campaign has not been finished in co-op yet.

## Existing work

Searched 2026-10-24 (web in English and Russian, GitHub, this repo): no story co-op project was found. Details and
the full search list in [NOTES.md](NOTES.md).

- [THUG Pro](https://en.wikipedia.org/wiki/THUG_Pro): online total conversion on THUG2, closed source, no campaign.
- [PARTYMOD](https://github.com/PARTYMANX/partymod-thug2) (MIT): fixes and OpenSpy online; no story co-op.
- [thug2-decomp](https://github.com/CanuckAussieKev/thug2-decomp): C++ reimplementation as a DLL hook. Not used at
  run time here, but its decompiled code named most of the addresses this mod hooks.
- THEMOD 1.3 (TH Mods, 2007-2008): a script mod the tester plays on and this mod is built on. Its site (thmods.com)
  no longer answers and no download page or author was found.

## How the game works (for co-op)

- **Single player is already a network game**: a local server plus one client (`setservermode on; StartServer;
  JoinServer`). Switching the network mode to LAN before `StartServer` makes the server reachable, and the game's
  own direct-IP join (UDP 5153) brings a second skater in. Positions and animations are then synced by the game.
- **Scripts decide what a level loads**: each level's `<LV>_Goals` script adds the classic goals in a net game and
  the story goals otherwise. The script-visible `InNetGame` / `InMultiplayerGame` are two-instruction thunks, so the
  mod answers "no" to them in co-op and levels load their story content. A few native gates (pedestrians, node
  skipping, 19 checks in the goal manager and career code) needed the same treatment.
- **Story state** is the career (`CSkaterCareer` Write/ReadToStructure) plus chapter info (`CHAPTER_STATUS`,
  intros and outros viewed) plus the goal manager's params. Serialized with the game's own `Script::WriteToBuffer`
  (~3.4 KB) and sent over a side TCP link (port + 1, 5154).
- **Scripts**: compiled `.qb` in `Data\pre\*.prx` (PRE v3, LZSS). THEMOD's exe loads its own
  `thethug2mod.prx` instead of `qb_scripts.prx`. NeverScript compiles and decompiles QB; the mod's own scripts are
  one `coop.qb` added to that archive.
- **Native code**: a `dinput8.dll` proxy patches the exe in memory at load time. Each patch checks the bytes it
  replaces first, so another build is left alone.

## What we changed

The mod is a `dinput8.dll` (C++17, MinGW i686) plus `scripts\coop\coop.qb`. The full list of hooks with addresses
is in [NOTES.md](NOTES.md); the main ones:

| Problem | Root cause | Fix |
|---|---|---|
| Story content missing in a net game | Level scripts and native gates check `InNetGame` | Co-op answers "not a net game" to scripts and to the 19 goal/career checks; the engine's own checks stay |
| Host crashed when the friend's skater was made | Career mode has `max_players=1`, so no second skater heap | Restart the server as the two-player start does (`cleanup preserve_skaters`, `InitSkaterHeaps`, players=2) |
| Friend ran their own story | The client loads its own career | Host sends career, chapter info and goal params on join and on every level change; friend's own save comes back when they leave |
| Goals, cutscenes and goal cameras one-sided | Each runs only on the player who triggered it | `WinGoal`, cutscene end/skip (`EndMovies`), `PlaySkaterCamAnim`/`KillSkaterCamAnim`, model swaps and the pro teammate pick are mirrored over the side link |
| Goals felt like "whoever is first wins" | A won goal was mirrored to both at once | Shared goals: a goal taken from an NPC starts for both and is won when both finish it; whoever is first waits |
| Friend's floating name drawn at the screen top | Career mode's name label | Own label projected from the skater camera, with distance, through walls |
| Skaters rode through each other | `GameNet::Manager::PlayerCollisionEnabled` follows a net option that is off in co-op | Hooked, `[coop] player_collision=` in `coop.ini` |
| Chat showed garbage for Russian | Cyrillic arrives as CP1251 bytes, the game's fonts have no Cyrillic | `Sio::KeyInput` hook transliterates (Russian, Ukrainian, Belarusian) by the keyboard layout's code page |
| Only 4 fixed resolutions | The device setup picks one of 1280x1024 ... 640x480 by `Launcher.exe`'s setting | `[display] resolution=` in `coop.ini` fills the last slot and always picks it; the window takes the same size |
| THEMOD 1.3 can't be downloaded any more | Its site is gone | A byte patcher rebuilds THEMOD's exe from the player's own `THUG2.exe` (below) |

### THEMOD

THEMOD 1.3's installer (NSIS, 2,224,553 bytes, SHA-256 `d3de1fa1...`) holds `Game\THEMOD 1.3.exe` (a full copy
of the game's exe, 122 bytes changed) and `Game\data\pre\thethug2mod.prx`. The exe changes: the save checksum's
`jnz` NOPed (0x5A8A01; PartyMod makes the same patch), a `jne` made a `jmp` inside GameSpy's CD-key SDK code
(0x5F95BB, near `gcd_init`; the online key check, not a disc check), the startup script archive renamed to
`thethug2mod.pre`, and dead server names. The tester's copy has three more
changes made by someone else (screen blur off, 800x600 replaced with 1920x1080, chat server back to GameSpy).
Shipping the installer would ship the game's exe, so the mod ships `patch_themod.bat` / `.ps1` instead: it checks
the player's `THUG2.exe` (size, SHA-256, every original byte) and writes `THEMOD 1.3.exe` next to it, SHA-256
`25874001be0c1f12df47bc8f0dad61869cae0fe90964c12245165732a4cbb5ea`, the installer's exe byte for byte.

## QA

- **Scripted two-client walkthrough** (local tools, not in the mod repo): two windowed game copies on one
  PC over loopback, driven by an in-process automation channel (`[debug] automation=1` wraps DirectInput and reads
  a command file per process). 17 steps: lobby, Story start, intro cutscenes skipped by the host, teleport, a goal
  won by the host, chapter change to Boston, pro teammate pick, a shared goal taken by the friend and finished by
  both, Barcelona, friend rejoin, host loss. It prints the log lines that show each step on both sides and fails
  only on a crash; it has no per-step PASS/FAIL asserts or mutation twins yet. Last run 2026-10-25: all 17 steps,
  no crashes. Goal sync, chat and collision were also run as separate scenarios with screenshots (2026-10-26).
- **Real play**: one test session of about half an hour, the host and a friend over the internet through
  Porthole (Steam port sharing). It found more bugs, which were fixed afterwards, and led to new optional settings
  in `coop.ini` (player collision, keys, resolution). The fixed build has not had a real two-PC session yet.
- **Not tested**: the mod on the exact exe the patcher makes (only on the tester's THEMOD copy, which differs in
  the three bytes regions above, none of them hooked); the patcher run on Windows (its byte table was checked by
  applying it in Python and comparing SHA-256).

## How to play

Download `thug2-coop.zip` from [Releases](https://github.com/LeftGamer1/thug2-coop/releases), unpack it into the
game folder, run `patch_themod.bat` if there is no `THEMOD 1.3.exe`, set `name=` and (friend) `host_address=` in
`coop.ini`. Host: F10 in the main menu, then Story Mode. Friend: F11. The host needs UDP 5153 and TCP 5154
reachable (VPN such as Radmin/ZeroTier, or Porthole). Full instructions in the repo's README.

## Known issues

- On some PCs the game (plain THEMOD 1.3 too) crashes on the splash screen, seen on Windows 11 and at 2K;
  `[debug] windowed=1` avoids it.
- In full screen the resolution in `coop.ini` must be one the monitor supports, or the game closes after the
  splash screen.
- If the friend leaves and comes back into the same level, the host's level reloads.
- Story decisions (next chapter, level change, the pro teammate) are the host's only; only the host saves.

## Next steps

- Turn the walkthrough into a suite with PASS/FAIL asserts per step and a mutant per sync feature.
- Play a full campaign in co-op and record what breaks (later chapters and their special goals are less tested).
- Load `coop.qb` from the DLL instead of appending it to THEMOD's archive, so the mod could also run without
  THEMOD's scripts.
