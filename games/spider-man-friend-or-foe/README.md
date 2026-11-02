# Spider-Man: Friend or Foe (2007)

| | |
|---|---|
| Engine | unknown for the inspected Windows build |
| Build inspected | Windows 1.0.0.111, coherent English/Russian packs |
| Online project | [Friend or Foe Online research](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online), source only |
| Whole-project QA | none |
| Last updated | 2026-10-21 |

## Existing co-op and project relationship

[Activision's launch announcement](https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3) describes drop-in sidekick co-op. Contemporary [PC testing](https://www.swiftworld.net/2007/10/13/pc-spider-man-friend-or-foe-review/) describes two players on one computer; the [GameSpot review](https://www.gamespot.com/reviews/spider-man-friend-or-foe-review/1900-6180451/) reports no online play. This is the official local-co-op entry, independent of the research below.

The public native Windows source project is an active prototype with no playable release. The existing [Discord project listing](https://discord.com/channels/1561411702082510879/1562185850777772152) is retained separately: its relationship to this source project and its executable has not been established. An engine attribution from one listing is not evidence for the other build; the directory now consistently says unknown. See [NOTES.md](NOTES.md).

## Approach and bounded evidence

The source implements exact-build hooks, separate local/remote player input, fixed gameplay timing, selected-state comparisons, content preflight and a launcher/relay. [Valve's networking APIs](https://partner.steamgames.com/doc/features/multiplayer/networking) are the intended online transport; their availability does not prove this prototype's remote delivery.

Scenario 11 / wire protocol 3 passed these same-machine English/Russian checks:

- 7,200 consecutive selected-state ticks with three fixture-driven Tokyo area transitions, keyboard/controller fixtures, jitter and mock transport. All games, launchers and relays exited 0; each relay sent/received 921,808 bytes, discarded 0.
- 3,600 combat/pause ticks, including 178 frozen frames and native resume. All owned processes exited 0; each relay sent/received 461,008 bytes, discarded 0.
- A separate quiet, fixture-disabled gameplay lifecycle reached 361 common ticks, then host-only window close delivered an ordered peer stop with normal exits. Its controlled pre-game route took about 212 seconds to become ready; the gate completed at about 221 seconds. Physical input was not exercised.

Earlier scenario-10 run 033 failed during native startup. Scenario 11 bounds the unsafe keyboard-name lookup and uses an already-loaded, validated US layout for internal names, preserving display labels and Windows settings. The new runs passed with US layouts; they do not establish the original trigger or broader keyboard-layout compatibility. Failures and full test scope remain in the project's [status record](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/STATUS.md).

A separately permitted Steam smoke check created and left one friends-only lobby, with no game traffic. It does not certify two-PC delivery. Whole-project QA remains **none**.

## How to play and remaining work

No online release package is available. The source [build instructions](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/BUILDING.md) describe a research setup using a supported game copy; game assets and Steam libraries are not distributed.

Retail campaign menus and travel, shared save/resume, mission/boss/NIS coverage, physical input usability and two-account Steam gameplay remain incomplete or unverified. Fixture-driven area travel does not establish normal campaign progression, and sampled hashes do not cover the whole world. The [roadmap](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/ROADMAP.md) records the remaining work.
