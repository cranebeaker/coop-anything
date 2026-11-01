# Spider-Man: Friend or Foe (2007)

| | |
|---|---|
| Inspected platform/build | Native Windows x86, 1.0.0.111; coherent English/Russian packs |
| Engine | Unknown for this executable; do not infer from a separate listing |
| Online project | [Friend or Foe Online](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online) |
| Latest published player build | [v1.2.0-beta](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/releases/tag/v1.2.0-beta), scenario 13 / protocol 6 |
| QA | **automated**, bounded real-client checks; not a verified full playthrough |
| Last checked | 2026-11-01 |

Supported `Game.exe` SHA-256: `ebd30047d7e58c95bf7dc4bd1fb4a5780e16aa472b61c25a8c421632662083db`. Windows pack provenance is a privately supplied repack; original storefront provenance is unknown. Hooks and fixes are specific to these bytes. Game files and Valve libraries are not distributed. See [build/content identity](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/BUILDING.md).

## Official co-op and separate listings

[Activision's announcement](https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3) describes drop-in sidekick co-op. Contemporary [PC testing](https://www.swiftworld.net/2007/10/13/pc-spider-man-friend-or-foe-review/) describes two players on one computer; the [GameSpot review](https://www.gamespot.com/reviews/spider-man-friend-or-foe-review/1900-6180451/) reports no online play.

The linked native online project now has beta player releases; the old source-only/no-release description is obsolete. The separately indexed [Discord listing](https://discord.com/channels/1561411702082510879/1562185850777772152) remains distinct: its relationship to this repository/executable has not been established. Keep its evidence and engine attribution separate.

## How the online mod works

Two independent Windows game instances exchange delayed inputs through a native Steam relay; each player can use an English or Russian pack. Exact-build hooks synchronize gameplay timing, selected RNG calls, native menus, area transitions and cutscene readiness. The host controls campaign decisions and in-mission dialogs; the guest controls the companion. See [architecture](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/ARCHITECTURE.md), [protocol/source](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/probe/probe.cpp) and [Valve transport documentation](https://partner.steamgames.com/doc/features/multiplayer/networking).

v1.1.0-beta supports joining/reconnecting in the host's current mission. The host waits while a bounded input journal reconstructs the incoming game, verifies sampled state, and admits the guest at ordinary gameplay. The host is not sent to the Helicarrier and the area does not restart. This is replay catch-up, not an instantaneous snapshot: up to one hour of history is retained per mission, and unavailable/unverifiable history refuses the guest while preserving host play. See [live-admission evidence](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/STATUS.md#v110-beta-mid-mission-joining).

## Checks that ran

All automated game checks below used isolated English/Russian copies on one PC. They do not certify new two-PC Steam admission, physical controllers or complete world-state equivalence. [Test procedures](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/TESTING.md), [detailed results/failures](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/STATUS.md).

| Check | Observation and limit |
|---|---|
| Campaign 488 | 20 missions and seven bosses in one shortened campaign run, approximately 60,000 frames, equal return-to-hub progress. The runner jumps each mission to its ending; this is not a complete playthrough. |
| Tutorial 493/494 | 1,801 matching frames each, normal/simulated poor network; host-only hint close agreed on both clients. |
| Save selection 495, 502–504 | Later empty host slot and native overwrite Yes/No/full-peer-list cases passed; failed earlier bot navigation attempts remain failed. Shared save/Continue recovery is still unfinished. |
| Live join 530/531 | Cancellation/exhausted history preserved host play for 463/448 additional ticks; normal exits. |
| Live join 532/535 | Simulated poor-network first arrival and swapped-character reconnect passed, with 1,904/1,898 matching post-admission frames. |
| Doc Ock 548 | Cross-area boss admission: 2,074 replay ticks and 1,887 subsequent frames matched, normal exits. Earlier readiness failures remain recorded. |
| Joiner menu 005 (1 November) | Guest New Game/Load rows hidden, host unchanged, host-selected New Game still starts both games; 601 matching frames, game/launcher exits 0, no forced cleanup. Direct guest Options/Quit clicks were not separately tested. |

The owner reported playing the first Tokyo mission with a friend on two PCs over real Steam. Treat that as an owner report, not an independently reproduced WAN gate or proof of later missions. A separate one-account lobby smoke carried no game traffic. `qa: automated` is supported by the real-client harness, not by those reports.

## Launcher and use

Download the matching player release on both PCs and supply a supported copy of the game. The host shares the lobby code; the guest pastes it and joins. Release binaries and source changes are distinct: the [v1.2.0-beta source revision](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/commit/09b36a3) adds a WPF launcher, fixed 900x600 layout, accessibility mode, a single-EXE bundler, cross-drive Steam-library discovery, hidden guest startup rows and a translated host connection notice. **These launcher changes now ship in v1.2.0-beta as a single-EXE download.** [Launcher build and behavior](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/LAUNCHER.md).

The new UI passed eleven-language/five-state checks, native fixed-window and nonactivating-notice tests, and actual bundle integrity/upgrade checks preserving synthetic player data. A read-only scan verified compatible signed Steam components on a non-system drive. No fresh Steam-account test was used for this update.

## Remaining work and reusable lessons

Shared campaign save/Continue recovery, physical-controller validation, new two-PC Steam admission tests, broader ordinary later-mission/long-history coverage and rare desync investigation remain. [Roadmap](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/ROADMAP.md). A shortened full-campaign gate must not be labeled `verified-playthrough`.

Keep simulation RNG, readiness barriers and local presentation timing separate; replay catch-up must include native loading/control-release gates, not just positions and RNG. For menu presentation changes, preserve native indices and automatic host-selected actions, test both seats, and exercise the actual derived renderer. A static design preview is not a substitute for real UI layout/accessibility/localization and packaged extraction tests. Further observations and unknowns are in [NOTES.md](NOTES.md).
