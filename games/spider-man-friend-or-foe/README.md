# Spider-Man: Friend or Foe (2007)

| | |
|---|---|
| Inspected platform/build | Native Windows x86, original English DVD and known English/Russian 1.0.0.111 build |
| Engine | Unknown for the supported builds |
| Online project | [Friend or Foe Online](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online) |
| Latest published player build | [v1.3.0-beta](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/releases/tag/v1.3.0-beta), scenario 13 / protocol 6 |
| QA | **automated**, bounded real-client checks; not a verified full playthrough |
| Last checked | 2026-11-07 |

The launcher verifies the executable and associated game content before importing the player's copy. Storefront provenance for the English/Russian pack remains unknown. The player download requires a separate game copy and Steam installation. [Published compatibility details](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/49ef73e6799c45a1028237573d7033af0e7bf126/README.md#compatibility).

| Supported `Game.exe` | SHA-256 |
|---|---|
| Original English DVD | `b9130323b5d842cf11672aac440c5f880ab637d83c99b720cf110baa81feb129` |
| English/Russian 1.0.0.111 | `ebd30047d7e58c95bf7dc4bd1fb4a5780e16aa472b61c25a8c421632662083db` |

## Official co-op and separate listings

[Activision's announcement](https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3) describes drop-in sidekick co-op. Contemporary [PC testing](https://www.swiftworld.net/2007/10/13/pc-spider-man-friend-or-foe-review/) describes two players on one computer; the [GameSpot review](https://www.gamespot.com/reviews/spider-man-friend-or-foe-review/1900-6180451/) reports no online play.

The separately indexed [Discord listing](https://discord.com/channels/1561411702082510879/1562185850777772152) remains distinct: its relationship to this repository/executable has not been established. Keep its evidence and engine attribution separate.

## How the online mod works

Two independent Windows game instances exchange delayed inputs through a native Steam relay. Each player supplies a supported build. Exact-build hooks synchronize gameplay timing, selected RNG calls, native menus, area transitions and cutscene readiness. The host controls campaign decisions and in-mission dialogs; the guest controls the companion. See [architecture](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/ARCHITECTURE.md), [protocol/source](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/probe/probe.cpp) and [Valve transport documentation](https://partner.steamgames.com/doc/features/multiplayer/networking).

v1.1.0-beta supports joining/reconnecting in the host's current mission. The host waits while a bounded input journal reconstructs the incoming game, verifies sampled state, and admits the guest at ordinary gameplay. The host is not sent to the Helicarrier and the area does not restart. This is replay catch-up, not an instantaneous snapshot: up to one hour of history is retained per mission, and unavailable/unverifiable history refuses the guest while preserving host play. See [live-admission evidence](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/STATUS.md#v110-beta-mid-mission-joining).

## Checks that ran

All automated game checks below used isolated English/Russian copies on one PC. They do not certify new two-PC Steam admission, physical controllers or complete world-state equivalence. [Test procedures](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/49ef73e6799c45a1028237573d7033af0e7bf126/docs/TESTING.md), [detailed results and failed attempts](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/49ef73e6799c45a1028237573d7033af0e7bf126/docs/STATUS.md).

| Check | Observation and limit |
|---|---|
| Campaign 488 | 20 missions and seven bosses in one shortened campaign run, approximately 60,000 frames, equal return-to-hub progress. The runner jumps each mission to its ending; this is not a complete playthrough. |
| Tutorial 493/494 | 1,801 matching frames each, normal/simulated poor network; host-only hint close agreed on both clients. |
| Save selection 495, 502–504 | Later empty host slot and native overwrite Yes/No/full-peer-list cases passed; failed earlier bot navigation attempts remain failed. Shared save/Continue recovery is still unfinished. |
| Live join 530/531 | Cancellation/exhausted history preserved host play for 463/448 additional ticks; normal exits. |
| Live join 532/535 | Simulated poor-network first arrival and swapped-character reconnect passed, with 1,904/1,898 matching post-admission frames. |
| Doc Ock 548 | Cross-area boss admission: 2,074 replay ticks and 1,887 subsequent frames matched, normal exits. Earlier readiness failures remain recorded. |
| v1.3.0-beta DVD compatibility | 608 matching frames for the original English DVD pair and 606 for a DVD/Russian pair. Games and launchers exited cleanly. The packaged player DLL also passed a peer handshake with bot and fixture flags off. |
| Joiner menu 005 (1 November) | Guest New Game/Load rows hidden, host unchanged, host-selected New Game still starts both games; 601 matching frames, game/launcher exits 0, no forced cleanup. Direct guest Options/Quit clicks were not separately tested. |

The owner reported playing the first Tokyo mission with a friend on two PCs over real Steam. Treat that as an owner report, not an independently reproduced WAN gate or proof of later missions. A separate one-account lobby smoke carried no game traffic. `qa: automated` is supported by the real-client harness, not by those reports.

## Launcher and use

Both players use the same release and import their own supported game copies. The host shares the lobby code and the guest joins with that code. The v1.3.0-beta single-EXE package passed digest, extraction and upgrade checks with synthetic player data. The WPF launcher passed eleven-language/five-state checks, and cancellation and retry passed before game or relay startup. [Release and install instructions](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/releases/tag/v1.3.0-beta).

## Remaining work and reusable lessons

Shared campaign save/Continue recovery, physical-controller validation, new two-PC Steam admission tests, broader ordinary later-mission/long-history coverage and rare desync investigation remain. [Roadmap](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/09b36a3/docs/ROADMAP.md). A shortened full-campaign gate must not be labeled `verified-playthrough`.

See [replay-based late joining](../../playbook/how-coop-mods-work.md#late-join-with-an-input-journal) for the admission sequence and test cases, and [menu presentation QA](../../playbook/automated-qa.md#menu-presentation-must-preserve-automatic-startup) for the guest-menu regression. Further observations and unknowns are in [NOTES.md](NOTES.md).
