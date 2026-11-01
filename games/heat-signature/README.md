# Heat Signature (2017)

| | |
|---|---|
| Engine | GameMaker: Studio 1.4 (engine 1.4.1804), compiled with **YYC**; 32-bit; the data chunk (`FORM`/`GEN8`) is embedded in `Heat_Signature.exe`, there is no `data.win` |
| Versions tested | Steam app 268130, build 6124596 (in-game 2021.1.20.1), Windows build run under Proton Experimental on Linux. `Heat_Signature.exe` SHA-256 `a5f8befe28786206ed1e2a52385051049524fea51e9e80660e3115e7126274d2` |
| Projects | from-zero mod in progress (claim: issue 274); code in a separate repo, not published yet |
| QA level | automated: 28-check suite, a 10-check mission scenario, a 9-check security scenario, a 7-check items scenario, a 13-check flying-home scenario, a 9-check leaving-early scenario and an 11-check knocked-out scenario on two real game copies (host + 1 guest), with 35 mutation runs; nothing tested with more than one guest or over Steam yet |
| Last updated | 2026-11-01 |

## Where things stand

Nobody can play Heat Signature together yet, but the core pieces of a co-op mod for up to 4 players run between two
game copies on one PC (localhost UDP, headless test displays). Working and tested with bot input: each player sees the
other as a puppet character or pod; shared pause, slow motion and fast-forward with a banner naming who paused, and a "<name> was spotted" banner on both
screens; the host's
mission and its ship are rebuilt identically on the guest from the mission text plus a seed; pods fly together in
space (with an optional "follow the host" mode) and both players can board the shared ship; doors and guard positions
are synced from the host; the host's guards notice guests, a guest's hits reach the host's guards, and guards' hits on
a guest's puppet reach the guest; a puppet has its player's keycard clearance and walks through a keycard door on the
other copy; the guest's copies of guards and sentry guns decide nothing (the host's shoot and notice every player);
witnesses and other mission stats are counted for the right player; bullets fired aboard the shared ship show on both
copies (the other copy's are harmless look-alikes; hits travel separately); after an alarm, reinforcements and their
jammer devices are the same on both copies; guards carry the same gear and drop the same items, and an item or loot
taken off the ship on one copy is gone on the other; a loot box opened on one copy opens on the other (its item is the
opener's); an item a player drops or throws aboard is the same item on the other copy (name, traits, every value) and
rests at the same place; a player who has left the ship in their pod is gone from it on the other copy, and each player's
mission completes when they dock at home (the end screen pauses only that player); a guest knocked out aboard is
carried off by the host's guards and thrown out of an airlock on both copies, then floats until it is lost, as the
game does with its own player, and a death no longer crashes the game; alarms, the target's death or escape and the
mission's end are the same on both copies. Not done: grenades and hull damage on both copies, a thrown item's hit on a guard (untested), other mission types, guard
animations on guests, more than one guest, Steam lobbies, an installer.

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
at the door, with a level-2 card both go through; walking on into a sentry's view, the host's sentry shoots the
guest's puppet and the guest is hurt while the guest's own sentries hold fire; the guest's pause and the guest being spotted show
banners with the guest's name on the host ("You ..." on the guest); the guest sees the host's bullets as
look-alikes that hurt nobody (every hit on the guest came from the host); the guest's witnesses and times seen
equal the host's count for its puppet, with none counted by the guest's copy; a guest's hit reaches the same guard on the host, which reacts, and a
knockout shows on the guest; keycard clearance reaches the puppet; a hit on the guest's puppet hurts the guest, not the
puppet; the guest's copies of guards asked for no alarm; alarm on in both copies and passed between them;
the target's escape on the host reaches the guest; the mission ends on both copies; the host's puppet aboard the guest's
copy of the ship; no error dialogs; both copies still advancing. The time checks run before anything that alerts the
ship: an alarm ends the mission (the target escapes), and the mission end screen holds time at 0.

For the keycard walk the host's guards and sentries ignore the guest's puppet (`unseen`, a test-only switch); for the
sentry and witness checks they see it again.

`tools/mission.sh` (10 checks) tests the kill, which the suite cannot (its target must live to escape): both copies dock,
an assassination runs on both, the guest's hit kills the target on the host, `AssassinationCompleted` is set on the
host and on the guest's copy, the target's drops lie at the same places on both, an item and the money the guest takes
are gone on the host too, a loot box the guest opens opens on the host too (no inventory menu there, the item on
the guest's copy only, and the guest takes it), no `FailMission` on either, no error dialogs, both advancing. It uses another draw of the
guards' gear (`GEAR_SALT=1`): with the default one this ship's target has a shield, which stops the guest's punch.

`tools/items.sh` (7 checks): dock; the guest drops its last item and the host shows the same object at the same
place with the same name and the same variables (count, texts, lists, numbers; references to other objects are not
copied); the host takes it (gone on the guest, in the host's inventory under the same name); the host throws it and,
once it has stopped, both copies have it at the same place (at most 2 units apart, moved at least 5); the guest takes it
back (gone on the host); no error dialogs; both advancing.

`tools/home.sh` (13 checks): dock; the guest kills the target (done on both); the guest takes its pod's controls and
leaves (on the host its puppet is hidden and nothing hurts the guest in its pod); in flight the guest's body (and so
the camera) stays with its pod; the host is told by name that the
guest left; the guest flies home (test helper `gotohome`: the pod and the player's body are put next to the home
station, stopped, instead of the flight) and its mission completes with the end screen open; the host is told; the
guest's end screen does not pause the host, still aboard; the host leaves and flies home and its mission completes; the
guest is told both; no `FailMission`; no error dialogs; both advancing.

`tools/fail.sh` (11 checks): dock; the target is killed first (an alarm would let it escape and end both missions);
a short knockout: the host's puppet goes down with the guest and both get up together; the host's guards knock the
guest out for real: a guard carries its puppet off on the host while the guest's copy holds it down; the host throws
the puppet out of the airlock and the guest floats in space; the guards did not shoot the knocked-out guest; the host
is told the guest was thrown out (not that it left); the guest's oxygen runs out and it is lost (character select,
no error dialog); the host is told; the host's mission goes on; no error dialogs; both advancing.

`tools/early.sh` (9 checks): dock; the guest leaves at once and flies home with the target alive: it docks and its
mission stays open (not failed: the game's own rule); the host is told it left, nothing more; the host's mission goes
on, unpaused; the host kills the target alone, which completes the guest's mission at home, and the host is told; the
host flies home and its mission completes; the guest is told; no error dialogs; both advancing.

`tools/security.sh` (9 checks) raises the alarm on a ship with reinforcements. The random test galaxy may offer
none, so the shared mission's text gets `ReinforcementProtocol = 2` and `EscapeProtocol = 0` on both copies (test
helper `MISSION_SET`). Checks: dock; the protocol on both copies; the guest beams in exactly the host's reinforcements;
the same reinforcements (tag, name, mercenary type), all mirrored on the guest; the same jammer devices; jammed at the
same places (`PositionIsScrambled` at every device and at the airlock); a device the guest destroys goes on the host
too; no error dialogs; both advancing.

Last runs, on the knockout build (2026-11-01, core 86c9d6a7): suite **28/28 twice** (16:04, 16:07 UTC), knocked-out
scenario 11/11 (16:10), home 13/13, early 9/9, mission 10/10, items 7/7, security 9/9 (16:12-16:18). On the builds
just before: knocked-out scenario 11/11 in 6 of 7 runs (the other: the guest was picked up by its own pod at once, now an
accepted ending), suite check 4d2 failed in 3 of 5 runs (one hit on the guest from its own copy: once a blast of the
guest's own grenade, now counted apart; once a bullet, cause unknown; once not examined), suite check 5 failed once (a
patrolling guard that did not react to the guest's hit, a known flake). Earlier
runs are in the history of this file.

Mutation runs (`COOP_MUTATION=<name>` breaks one feature in the core), each caught by its own check unless marked. The suite ones ran
against the 14:32 build, before the 27th check (`nopuppethits` and `mirrorsees` were re-run on the 15:30 build with the
same result; `noghosts` and `ghosthurts` ran on the final build):

| Mutation | Result | The check that catches it |
|---|---|---|
| `noseed` (shared ship not seeded) | 18 passed, 8 failed | identical ship: 87 vs 81 modules |
| `notime` (guest ignores the host's time) | 25 passed, 1 failed | host pause: host 0, guest 1 |
| `noguards` (guards not mirrored) | 20 passed, 6 failed | guards mirrored: guest 0 of 38 |
| `nohits` (guest hits not forwarded) | 25 passed, 1 failed | guest's hit: applied 0 on the host |
| `noclearance` (puppet keeps clearance 0) | 21 passed, 5 failed | keycard door: the host's puppet stuck at fore 1750 (door 3); clearance 0 -> 0 |
| `nopuppethits` (hits on a puppet stay on it) | 25 passed, 1 failed | guest injuries 2 -> 2, puppet 6 |
| `noalarm` (alarm not synced) | 25 passed, 1 failed | alarm passed between copies: 0 and 0 |
| `noescape` (escape not synced) | 24 passed, 2 failed | escape sent 1, applied 0; mission ends only on the host |
| `noparking` (puppets not parked) | 21 passed, 5 failed | puppets parked: 0 on both |
| `mirroralarm` (guest's copies of guards may raise alarms) | 22 passed, 4 failed | the guest's copy asked the host for an alarm |
| `nosentrysync` (sentries not decided by the host) | 24 passed, 2 failed | host's sentry shots at the puppet 0 |
| `nostats` (mission stats not sent) | 25 passed, 1 failed | host saw the puppet 11 times, guest shows 0 |
| `mirrorsees` (guest's copies of guards may see the guest) | 25 passed, 1 failed | guest's copy counted 6 witnesses itself |
| `noghosts` (bullets not sent) | 26 passed, 1 failed | host sent 0 bullets, guest shows 0 |
| `ghosthurts` (shown bullets hurt) | 26 passed, 1 failed | 6 hits on the guest not from the host |
| `noreinforcements` (each copy beams in its own), security scenario | 7 passed, 2 failed | the guest's reinforcements differ from the host's |
| `nojammers` (devices not synced), security scenario | 6 passed, 3 failed | host 3 devices, guest 0; jammed at 3 places on the host only |
| `nogearseed` (gear not seeded), mission scenario | 7 passed, 2 failed | the target's drops differ (4 lines); a taken item not found on the host |
| `notaken` (taking not sent), mission scenario | 8 passed, 1 failed | guest sent 0; host contents 77, guest 75 |
| `noboxes` (box openings not synced), mission scenario | 9 passed, 1 failed | the host's copy opened 0 boxes for the guest |
| `nodrops` (drops and throws not sent), items scenario | 4 passed, 3 failed | the host shows nothing where the guest dropped its item |
| `nospotted` (sightings not counted), suite | 27 passed, 1 failed | no "You were spotted" on the guest, none named on the host |
| `noleave` (a puppet stays after its player left), home scenario | 6 passed, 3 failed | on the host the puppet stayed visible; 17 hits reached the guest in its pod, knocked out into space |
| `sharedendscreen` (end screen pause shared), home scenario | 7 passed, 2 failed | host time scale 0 while the guest's end screen was open |
| `nonews` (no banners about the other player), home scenario | 9 passed, 3 failed | no "left the ship" or "mission complete" banner on either copy |
| `nopilotfollow` (the pilot's body not kept with its pod), home scenario | 12 passed, 1 failed | the guest's body stayed where its pod left the ship |
| `nodown` (the puppet stays up when the guest is knocked out), fail scenario | 4 passed, 7 failed | 31 hits on the knocked-out guest; no guard carried it |
| `nosaviour` (the carrying guard looks at the nearest player), fail scenario | 7 passed, 4 failed | the puppet was not thrown out; the guest stayed aboard |
| `nobody` (the guest's copy does not follow its body), fail scenario | 5 passed, 6 failed | the host threw the puppet out 10 times; the guest stayed aboard |
| `noeject` (the host does not throw the puppet out), fail scenario | 7 passed, 4 failed | the puppet lay in the airlock until it got up |
| `nonewlife` (no stale building removed at a new life), fail scenario | 10 passed, 1 failed | "no chooseable players exist" dialog on the guest |
| `noguestsave`, `nodownpod`, fail scenario | 11 passed each | not caught: the scenario does not reach those cases (see NOTES.md) |
| `nohits`, mission scenario | 6 passed, 1 failed | host's AssassinationCompleted 0 (the guest's copy killed its own target) |
| `noguards`, mission scenario | 6 passed, 1 failed | guest's AssassinationCompleted 0 |

Some mutations fail more checks than their own (a feature broken early disturbs later checks); the table names the
check aimed at each.

## How to play

Not playable yet.

## Known issues

Not applicable yet.

## Next steps

- Grenades, explosions and hull damage on every copy; other mission types and docking home; guard states and animations on guests; a thrown item's hit on a guard; armed traps; items thrown off the ship; rescuing a floating player, a new character after a death, a pod destroyed.
- Up to 3 guests, Steam lobbies and invites, mission sharing from the game's own menus.
- Suite with 3 and 4 copies, then an installer.
