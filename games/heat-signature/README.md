# Heat Signature (2017)

| | |
|---|---|
| Engine | GameMaker: Studio 1.4 (engine 1.4.1804), compiled with **YYC**; 32-bit; the data chunk (`FORM`/`GEN8`) is embedded in `Heat_Signature.exe`, there is no `data.win` |
| Versions tested | Steam app 268130, build 6124596 (in-game 2021.1.20.1), Windows build run under Proton Experimental on Linux. `Heat_Signature.exe` SHA-256 `a5f8befe28786206ed1e2a52385051049524fea51e9e80660e3115e7126274d2` |
| Projects | from-zero mod in progress (claim: issue 274); code in a separate repo, not published yet |
| QA level | automated: 24-check suite on two real game copies (host + 1 guest), with ten mutation runs; nothing tested with more than one guest or over Steam yet |
| Last updated | 2026-10-31 |

## Where things stand

Nobody can play Heat Signature together yet, but the core pieces of a co-op mod for up to 4 players run between two
game copies on one PC (localhost UDP, headless test displays). Working and tested with bot input: each player sees the
other as a puppet character or pod; shared pause, slow motion and fast-forward with a "paused by" banner; the host's
mission and its ship are rebuilt identically on the guest from the mission text plus a seed; pods fly together in
space (with an optional "follow the host" mode) and both players can board the shared ship; doors and guard positions
are synced from the host; the host's guards notice guests, a guest's hits reach the host's guards, and guards' hits on
a guest's puppet reach the guest; a puppet has its player's keycard clearance and walks through a keycard door on the
other copy; the guest's copies of guards decide nothing (no alarms or attacks of their own); alarms, the mission
target's escape and the mission's end are the same on both copies. Not done: sentry guns, loot, guard animations on
guests, more than one guest, Steam lobbies, an installer.

## Existing work

- No co-op or multiplayer mod found (web search 2026-10-30; GitHub, Nexus Mods, ModDB, Steam Workshop and forums).
- The [Steam page](https://store.steampowered.com/app/268130/Heat_Signature/) says there is no multiplayer. Forum
  threads asking for co-op: [1](https://steamcommunity.com/app/268130/discussions/0/144512526678821083/),
  [2](https://steamcommunity.com/app/268130/discussions/0/1520386297686058374/).
- A small modding scene exists: a QoL mod on Nexus needs a "Heat Signature Mod Loader"
  ([Nexus](https://www.nexusmods.com/heatsignature/mods/4)). How that loader works is unknown.

## How the game works (for co-op)

- **Code:** YYC, so there is no GML in the data chunk. Each compiled function stores a pointer to its own name
  (`gml_Script_PlayerCreate`, `gml_Object_oPlayer_Step_0`) on its stack frame. Matching those `mov [esp+disp], imm32`
  instructions to the name strings binds 2504 of 2562 names to functions. Ghidra 12.1.2 then decompiles them one by one
  without a full auto-analysis (the full analysis of the 73 MB exe takes hours). Details in [NOTES.md](NOTES.md).
- **Variables:** a `{name, id}` table in `.data` names 3200 instance and global variable slots. Globals sit on one
  global instance; `global.Player` is slot 5 and holds the player's instance id as a real.
- **One player:** a single `oPlayer`, reached through `global.Player`. The same pattern as Forager.
- **Time:** the player can pause at any time, and there is slow motion and fast-forward (`TimeScale`, `SlowMo`,
  `FastMo`). Co-op needs a rule; the plan is a shared pause.
- **Saves:** `%APPDATA%\Heat_Signature`. The galaxy is a plain-text file, and each character is its own ~2 KB file with
  base64 lines and a checksum. A guest can bring their own character file to the host.
- **Steam:** the game initialises `steam_api.dll` and does not reach its first room without a running Steam client.
- **Injection:** the exe imports only three functions from `VERSION.dll`, so a `version.dll` proxy loads the mod (under
  Proton with `WINEDLLOVERRIDES=version=n,b`).

## What we changed

Nothing in the game files. The mod is two DLLs next to the exe: `version.dll`, a permanent loader (keeps the game
running unfocused, logs and answers error dialogs in test copies, hosts the hot-reload core) and `coop_core.dll`,
swapped while the game runs. Each mechanism, the evidence and the numbers are in [NOTES.md](NOTES.md).

## QA

`tools/suite.sh` in the mod repo starts host A and guest B (Proton, headless gamescope displays, UDP on localhost),
shares a galaxy mission, docks both pods at the shared ship (`board.sh`, every step checked and retried; the suite stops
if boarding fails) and drives both clients with the core's bot commands. It prints PASS/FAIL per check; exit code =
failures. Checks, in order: connect, share and dock; identical shared ship; same doors; every host guard mirrored;
puppets parked while their players stand in their pods; guest pause, guest unpause, host pause, host unpause on both;
host fast-forward alone refused; fast-forward when both ask; the parked guest left alone by the host's guards; the
guest walks from its pod toward a room behind a security-2 door: with a level-1 card it and its puppet on the host stop
at the door, with a level-2 card both go through; a guest's hit reaches the same guard on the host, which reacts, and a
knockout shows on the guest; keycard clearance reaches the puppet; a hit on the guest's puppet hurts the guest, not the
puppet; the guest's copies of guards asked for no alarm; alarm on in both copies and passed between them;
the target's escape on the host reaches the guest; the mission ends on both copies; the host's puppet aboard the guest's
copy of the ship; no error dialogs; both copies still advancing. The time checks run before anything that alerts the
ship: an alarm ends the mission (the target escapes), and the mission end screen holds time at 0.

For the walk, the host's guards ignore the guest's puppet (`unseen`) and the guest's sentry guns hold fire
(`nosentry`); both are test-only switches (sentries are not synced yet, see Next steps).

Last runs (2026-10-31, 24 checks): **24/24 three times in a row** (12:55, 12:57, 13:02 UTC) after the last suite
change. Before it, two of five runs failed the walk and the old "no alarm at all" check: the guest's unsynced sentry
shot the guest, and once the suite's own guard hit killed a guard whose dropped grenade alerted the host's guards (the
game's rule). The suite now makes the guest's sentries hold fire for the walk and only fails on alarms the guest's copy
asks for.

Mutation runs (`COOP_MUTATION=<name>` breaks one feature in the core), all against the 13:02 build, each caught by its
own check:

| Mutation | Result | The check that catches it |
|---|---|---|
| `noseed` (shared ship not seeded) | 12 passed, 12 failed | same doors: lists differ (the guest's copy then raised an error dialog and stalled) |
| `notime` (guest ignores the host's time) | 23 passed, 1 failed | host pause: host 0, guest 1 |
| `noguards` (guards not mirrored) | 17 passed, 7 failed | guards mirrored: guest 0 of 38 |
| `nohits` (guest hits not forwarded) | 23 passed, 1 failed | guest's hit: applied 0 on the host |
| `noclearance` (puppet keeps clearance 0) | 21 passed, 3 failed | keycard door: the host's puppet stuck at fore 1750 (door 3) while the guest walked on; clearance 0 -> 0 |
| `nopuppethits` (hits on a puppet stay on it) | 23 passed, 1 failed | guest injuries 0 -> 0, puppet 1 |
| `noalarm` (alarm not synced) | 23 passed, 1 failed | alarm passed between copies: 0 and 0 |
| `noescape` (escape not synced) | 21 passed, 3 failed | escape sent 1, applied 0; mission ends only on the host (the guard-hit check also failed in this run: the host's guard did not react) |
| `noparking` (puppets not parked) | 19 passed, 5 failed | puppets parked: 0 on both; 20 hits on the parked guest |
| `mirroralarm` (guest's copies of guards may raise alarms) | 23 passed, 1 failed | the guest's copy asked the host for an alarm |

## How to play

Not playable yet.

## Known issues

Not applicable yet.

## Next steps

- Sentry guns decided by the host; mission success for everyone; spotted pause/zoom on the spotted player's screen;
  guard states and animations on guests; witnesses counted for guests; loot.
- Up to 3 guests, Steam lobbies and invites, mission sharing from the game's own menus.
- Suite with 3 and 4 copies, then an installer.
