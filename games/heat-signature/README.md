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
a guest's puppet reach the guest; a puppet has its player's keycard clearance. Not done: alarms, loot, guard animations
on guests, more than one guest, Steam lobbies, an installer.

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
shares a galaxy mission, docks both pods at the shared ship (`board.sh`, every step checked and retried) and drives
both clients with the core's bot commands. It prints PASS/FAIL per check; exit code = failures. Checks: connect, share
and dock; a guest's hit knocks a guard down on the host and the guest; identical shared ship (RNG hash and module
counts); same doors; every host guard mirrored on the guest; guest pause, guest unpause, host pause, host unpause on
both; host fast-forward alone refused; fast-forward when both ask; host's puppet aboard the guest's copy of the ship;
no error dialogs; both copies still advancing; keycard clearance reaches the puppet; a hit on the guest's puppet hurts
the guest's own player, not the puppet.

Last run (2026-10-31 03:30 UTC, core build of that time): **16 passed, 0 failed**.

Mutation runs (`COOP_MUTATION=<name>` breaks one feature in the core), each caught by its own check:

| Mutation | Result | The check that catches it |
|---|---|---|
| `noseed` (shared ship not seeded) | 10 passed, 4 failed | identical shared ship: host 87 modules, guest 81 |
| `notime` (guest ignores the host's time) | 13 passed, 1 failed | host pause: host 0, guest 1 |
| `noguards` (guards not mirrored) | 12 passed, 2 failed | guards mirrored: guest 0 of 38 |
| `nohits` (guest hits not forwarded) | 9 passed, 5 failed | guest's hit: host Down 0 -> 0 |
| `noclearance` (puppet keeps clearance 0) | 15 passed, 1 failed | clearance: host's puppet 0 -> 0 |
| `nopuppethits` (hits on a puppet stay on it) | 9 passed, 7 failed | guest injuries 0 -> 0, puppet 8 |

Seen only in mutation runs: a pause that did not release (later time checks read 0 on both). Likely explanation
(derived from `nopuppethits`): the host's mission fails when the guest's puppet is knocked out, and the failure screen
holds time at 0. Under `noguards` and `nopuppethits` the door lists differed (which doors a copy has seen depends on
activity). The `noclearance` and `nopuppethits` rows are from the 03:31 UTC build, the others from 00:04 UTC.

## How to play

Not playable yet.

## Known issues

Not applicable yet.

## Next steps

- Alarms synced from the host (an alarm fails the host's mission while the guest still counts down); a player's puppet
  should not wait in the airlock while the player stands in their own pod; a puppet walking through a keycard door;
  spotted pause/zoom on the spotted player's screen; guard states and animations on guests; loot.
- Up to 3 guests, Steam lobbies and invites, mission sharing from the game's own menus.
- Suite with 3 and 4 copies, then an installer.
