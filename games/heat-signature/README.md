# Heat Signature (2017)

| | |
|---|---|
| Engine | GameMaker: Studio 1.4 (engine 1.4.1804), compiled with **YYC**; 32-bit; the data chunk (`FORM`/`GEN8`) is embedded in `Heat_Signature.exe`, there is no `data.win` |
| Versions tested | Steam app 268130, build 6124596 (in-game 2021.1.20.1), Windows build run under Proton Experimental on Linux. `Heat_Signature.exe` SHA-256 `a5f8befe28786206ed1e2a52385051049524fea51e9e80660e3115e7126274d2` |
| Projects | from-zero mod in progress (claim: issue 274); code in a separate repo, not published yet |
| QA level | automated: 14-check suite on two real game copies (host + 1 guest), with mutation runs; nothing tested with more than one guest or over Steam yet |
| Last updated | 2026-10-31 |

## Where things stand

Nobody can play Heat Signature together yet, but the core pieces of a co-op mod for up to 4 players run between two
game copies on one PC (localhost UDP, headless test displays). Working and tested with bot input: each player sees the
other as a puppet character or pod; shared pause, slow motion and fast-forward with a "paused by" banner; the host's
mission and its ship are rebuilt identically on the guest from the mission text plus a seed; pods fly together in
space (with an optional "follow the host" mode) and both players can board the shared ship; doors and guard positions
are synced from the host; the host's guards notice guests, a guest's hits reach the host's guards, and guards' hits on
a guest's puppet reach the guest; a puppet has its player's keycard clearance; alarms, the mission target's escape and the mission's end are the same
on both copies. Not done: loot, guard animations on guests, more than one guest, Steam lobbies, an installer.

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
host fast-forward alone refused; fast-forward when both ask; the parked guest left alone by the host's guards; a guest's
hit reaches the same guard on the host, which reacts, and a knockout shows on the guest; keycard clearance reaches the
puppet; a hit on the guest's puppet hurts the guest, not the puppet; alarm on in both copies and passed between them;
the target's escape on the host reaches the guest; the mission ends on both copies; the host's puppet aboard the guest's
copy of the ship; no error dialogs; both copies still advancing. The time checks run before anything that alerts the
ship: an alarm ends the mission (the target escapes), and the mission end screen holds time at 0.

Last runs (2026-10-31, 05:27 to 05:41 UTC): **19 of 19 three times in a row, then 21 of 21** after two more checks.

Mutation runs (`COOP_MUTATION=<name>` breaks one feature in the core), all against the 05:41 build, each caught by its
own check:

| Mutation | Result | The check that catches it |
|---|---|---|
| `noseed` (shared ship not seeded) | 16 passed, 5 failed | identical ship: 81 vs 87 modules (doors, hit, escape follow) |
| `notime` (guest ignores the host's time) | 20 passed, 1 failed | host pause: host 0, guest 1 |
| `noguards` (guards not mirrored) | 19 passed, 2 failed | guards mirrored: guest 0 of 38 |
| `nohits` (guest hits not forwarded) | 20 passed, 1 failed | guest's hit: applied 0 on the host |
| `noclearance` (puppet keeps clearance 0) | 20 passed, 1 failed | clearance: host's puppet 0 -> 0 |
| `nopuppethits` (hits on a puppet stay on it) | 20 passed, 1 failed | guest injuries 0 -> 0, puppet 1 |
| `noalarm` (alarm not synced) | 20 passed, 1 failed | alarm passed between copies: 0 and 0 |
| `noescape` (escape not synced) | 19 passed, 2 failed | escape sent 1, applied 0; mission ends only on the host |
| `noparking` (puppets not parked) | 19 passed, 2 failed | puppets parked: 0 on both |

## How to play

Not playable yet.

## Known issues

Not applicable yet.

## Next steps

- A puppet walking through a keycard door (bot walking for players); mission success for everyone; spotted
  pause/zoom on the spotted player's screen; guard states and animations on guests; witnesses counted for guests; loot.
- Up to 3 guests, Steam lobbies and invites, mission sharing from the game's own menus.
- Suite with 3 and 4 copies, then an installer.
