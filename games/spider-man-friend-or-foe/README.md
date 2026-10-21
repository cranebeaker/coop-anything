# Spider-Man: Friend or Foe (2007)

| | |
|---|---|
| Engine | proprietary Next Level Games code; exact engine name unknown |
| Versions tested | Windows 1.0.0.111: isolated English/Russian bot runs and limited paired-client prototype |
| Projects | Official Windows local co-op; [Friend or Foe Online research](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online), no playable release |
| QA level | none |
| Last updated | 2026-10-21 |

## Where things stand

Contemporary firsthand PC reviews describe two-player co-op on one computer and no native online play. A private experimental input relay now connects two independently running Windows clients over loopback. English/Russian native-input fixtures matched 7,200 sampled combat frames and three successive area swaps, including selected progress checks, with normal exits. A real Steam Spacewar lobby smoke check passed; two-account message delivery and full campaign progression remain unverified. No playable online release is available.

The original-code prototype, developer build instructions, test scope and remaining
work now live in the [separate game repository](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online).
It is an active research preview, with no game assets or compiled release distributed.
Its [status page](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/STATUS.md)
records a weighted **40/100** first-release milestone estimate; this is engineering
judgement, not measured coverage or a time forecast.

## Existing work

- [Activision launch announcement](https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3): drop-in co-op, PC among platforms.
- [Windows review](https://www.swiftworld.net/2007/10/13/pc-spider-man-friend-or-foe-review/): two players on one computer.
- [GameSpot review](https://www.gamespot.com/reviews/spider-man-friend-or-foe-review/1900-6180451/): PC/Xbox 360/Wii review, two-player support without online play, Xbox 360 controller reported working on PC.
- [PCGamingWiki compatibility fix](https://community.pcgamingwiki.com/files/file/3759-spider-man-friend-or-foe-fix/): display/FOV/FPS enhancements, not an online project; not downloaded or tested here.

See [NOTES.md](NOTES.md) for searches, provenance, exclusions and limitations. Streaming/emulator demonstrations do not establish native Windows networking.

## How the game works (for co-op)

The local mode provides a second playable character. Static inspection identified structured data/dictionaries, bytecode scripts and shipped automation functions. A common action-state layer follows keyboard/controller mapping; the prototype relays those semantic actions. Native initialization writes saves and settings outside the install folder before graphics initialization, so the test harness installs isolation hooks before entry.

The inspected English and Russian variants share the same executable and gameplay files. Localized audio, video, text and fonts differ. Limited mixed-language loopback tests ran; dialogue, mission progression and long-session compatibility remain unverified.

## What we changed

Research documentation and a directory entry. The private prototype includes an isolated launcher, guarded runtime hooks, a semantic-input bot, and a two-peer input channel. It is not packaged for human play. Coordination: [claim #71](https://github.com/cranebeaker/coop-anything/issues/71).

## QA

Static metadata checks and the own-code isolation fixture passed. English/Russian bot runs reached the level with separate character input slots, movement and attacks. A transport fixture passed 180 ticks, invalid-input rejection and build/scenario mismatch rejection.

Initial 600-frame runs exposed transient random-state timing differences. Canonical task timing resolved the tested ambient timing issue; a longer combat test then exposed CPU-time-dependent navigation work, also reproduced with two English copies. Completing that work at each simulation step removed the observed drift in later tests.

A native-input English/Russian fixture completed 7,200 consecutive frames with matching normalized inputs, common random state, player poses and a sorted character hash covering position, heading, controller slot and health. Damage and hostile defeats were observed. Both processes exited normally with code 0. A separate 3,600-frame gate passed native analog-deadzone checks, a 31-frame simulated local controller disconnection, injected input delays, live sampled-state comparisons and normal exits. The scorer's positive fixture and 12 invalid-result fixtures passed.

A 3,600-frame keyboard-host/controller-peer gate also passed through the native binding layers. Subsequent paired gates placed both party characters inside real enabled exit volumes and invoked their normal enter callbacks after the native transition manager became idle. A 7,200-frame gate crossed three successive boundaries into Tokyo's second, third and fourth areas while matching every sampled state. Another 7,200-frame run added selected serialized progress fields and the current area to the live checksum and also passed. Both games and relay helpers exited0. These use a loopback mock of the Steam lobby/messaging contract, not Valve's network.

The corrected selected-progress checksum and revised session protocol also passed a new7,200-frame, three-area-change English/Russian gate. Each mock relay sent and received921,808 bytes with zero discarded, and all game/launcher/relay processes exited0.

A separately authorized live check created one friends-only Spacewar lobby, left and shut down with exit0; the same bounded check also passed through the subsequently compiled relay. It sent no invitations or game traffic. Own-code contract fixtures and compiled flat-API ABI fixtures passed. A private Windows launcher now verifies either language pack, provides Host/Join controls and an optional scripted connection test, and its unlimited-frame native game lifecycle also passed a separate English/Russian loopback idle check: 306 consecutive common samples, no gameplay input fixture, and normal owned-window shutdown. This does not establish physical keyboard/controller usability. Steam lobby access alone does not establish remote delivery or playable online co-op. See [Valve's networking documentation](https://partner.steamgames.com/doc/features/multiplayer/networking) and [Spacewar example](https://partner.steamgames.com/doc/sdk/api/example) for the intended transport.

These are bounded same-machine gates. They do not cover complete enemy behavior, mission, inventory, dialogue, full campaign progression, real hardware focus paths or WAN behavior. The directory level remains `none` for complete co-op QA.

## How to play

No native online release or human host/join procedure is available from this work yet. The private prototype currently uses bots and loopback connections.

## Known issues

Only the identified executable and two supplied language variants have been inspected. Complete shared-world state, human input usability, remote networking, transport reconnect and full campaign progression remain incomplete or unverified. Three successive area swaps have passed in the Tokyo scenario. Selected initialized progress fields are checked; this is not a complete world or mission-state oracle. Content preflight permits the two complete known language packs while requiring identical gameplay content; this does not prove that localized audio cannot affect progression.

## Current-source pause and storage foundation

Scenario10/wire3 completed a new3,600-tick mixed English/Russian bot gate with
keyboard/controller bindings, injected delays and a mock Steam stream. Both
clients entered native pause at243, retained the same frozen actor sample for178
ticks, resumed at421 and completed the fixture at430 before continuing combat.
All games, launchers and relays exited0; each relay sent/received461,008 bytes
and discarded0. This is a one-machine bounded result, with no Steam gameplay.

The expanded oracle samples full area paths, selected campaign flags and native
mode/restoration state. Failed precursor gates exposed incorrect fixture placement
and inactive history in the checksum; these failures and the fixes are recorded in
the project's status page. They are not counted as passes. A profile-storage
foundation passed39 synthetic tests, but has no native save/load or launcher
integration and does not establish campaign resume.

The following scenario-10 multi-area regression failed on both clients during
native startup, before network readiness or gameplay. No current-source multi-area
pass is credited. Static analysis found an unbounded native keyboard-name lookup;
whether layout-dependent names triggered this failure remains unconfirmed.
Startup reliability remains an open blocker.

## Next steps

Preserve retail New Game/Continue, hub and mission/sidekick selection; synchronize
menu/load/NIS boundaries; integrate verified native saving and mod-owned profile
restore; broaden campaign/boss/state checks; establish two-account Steam delivery
and physical-input usability before offering a playable package. See the project's
[roadmap](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/ROADMAP.md).
Keep private machine details and game assets in ignored `local/`.
