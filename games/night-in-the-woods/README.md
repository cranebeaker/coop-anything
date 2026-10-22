# Night in the Woods (2017)

| | |
|---|---|
| Engine | Unity 5.6.2p4 (Mono) |
| Versions tested | Steam PC build 5940043 (AppID 481510) |
| Project | [Night in the Woods Co-op prototype source](https://github.com/SamuelAustin2001/night-in-the-woods-coop) (experimental, not a verified playable release) |
| QA level | none (bot-driven smoke checks; no repeatable suite) |
| Last updated | 2026-10-21 |

## Where things stand

A local host/client prototype synchronizes input, scene and Mae position, plus the game's numeric Yarn story-variable dictionary. It is not released or established as playable co-op: choices, scripted effects, persistent save state and a full story run have not been verified.

## Existing work

GitHub and this repository were searched for prior Night in the Woods co-op work. ModDB, Discord and archival sources have not been searched, so existing work there is unknown. The experimental plugin source is published in the [game-specific repository](https://github.com/SamuelAustin2001/night-in-the-woods-coop); no packaged player release is available.

## How the game works (for co-op)

The tested Steam build is a Windows x64 Unity 5.6.2p4 game using Mono. `Player` samples Rewired actions, Yarn's `DialogueRunner` handles dialogue, and `Continuity` stores numeric story variables and persistent/save data. The assembly has no reference to Unity networking. Save files are serialized as `.dat` files under Unity's persistent-data directory.

The prototype uses host-authoritative shared-story co-play: both participants contribute movement, jump, interaction and dialogue-confirm input for the same Mae. The host sends scene and position snapshots and the numeric `Continuity.vars` dictionary. Guest saves are suppressed. Numeric variables are sent in bounded UDP fragments and applied only after a complete snapshot arrives.

## What we changed

- Mapped player input, Yarn dialogue, `Continuity` story variables and save paths in an isolated copy of Steam build 5940043.
- Built a BepInEx host/client prototype for shared input, host scene/position following and numeric story-variable replication.
- Added a bot-only variable mutation command. A client-side mutation of `AngusConstellations` was corrected by the host snapshot; both clients then reported 316 variables with hash `0F68BB8C`.

## QA

These were one-off bot-driven checks on two instances of the Steam build connected over loopback, not a maintained automated suite.

- A client confirmation moved both clients from `Title` to `IntroPoem`.
- In the QA-selected `MaeRoom`, host and client movement pulses moved Mae; positions were within 0.02 world units after each pulse.
- A host-only QA scene change moved the client to `BusStation`; positions differed by about 0.001 units after loading.
- The client bot mutated `AngusConstellations`; after the host sent six variable fragments, both status files reported 316 variables and hash `0F68BB8C`. The client reported `host snapshot applied`.
- Failures: a host confirmation at the title was observed by the client but did not advance the host. In `MaeHouse`, a host left-movement pulse did not move Mae, and host/client interaction did not open a Yarn option menu; the cause is unknown. In an earlier `MaeRoom` mutation check, the client reported `host snapshot applied` while its whole-dictionary hash still differed for at least six seconds. The hashes later matched after a forced `Title` transition, but the guest was still in `TransitionScene` while the host reported `Title`. A later v0.6.1 repeat corrected the mutation and produced matching hashes.
- A later fresh-launch attempt failed to connect: the guest reported `UDP receive error: An invalid argument was supplied.` No result is claimed for that attempt.

The direct scene setup bypasses normal story flow. The variable test checked dictionary correction only, not dialogue choice progression.

## How to play

The source repository includes experimental build and installation instructions, but there is no packaged player release. The prototype is not established as playable co-op; use only if you can build and troubleshoot an unfinished mod.

## Known issues

- There is one shared Mae; players do not appear as separate characters.
- Normal dialogue-choice behavior and scripted events have not been tested for divergence.
- Persistent journal/counter data and save progression are not replicated; late join and disconnect recovery are not implemented.
- Networking is unauthenticated direct UDP intended for a trusted LAN or VPN. Real-network behavior is unknown.

## Next steps

- Test normal dialogue choices and scene transitions with bots while comparing host/client story state.
- Synchronize or explicitly document persistent journal/counter state and save ownership.
- Add a repeatable smoke-test runner and test packet loss, reconnects, and a real LAN/VPN session.
- Continue investigating the live choice, movement/interact, story-hash and UDP failures before offering it to players.

