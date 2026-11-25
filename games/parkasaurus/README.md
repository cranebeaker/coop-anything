# Parkasaurus (2020)

| | |
|---|---|
| Engine | Unity 2020.3.47f1, Mono |
| Versions tested | Steam for Windows x64, Parkasaurus 2.21a, build 14392464 |
| Projects | [Parkasaurus Co-op](https://github.com/oshoubber/parkasaurus-coop) (MIT) |
| QA level | Automated, two local game clients |
| Last updated | 2026-11-15 |

## Where things stand

Parkasaurus Co-op is a two-player alpha. Players manage the same park with shared finances, inventory, research, and simulation speed. A full campaign playthrough and mission completion remain unverified. [Project README](https://github.com/oshoubber/parkasaurus-coop#readme)

## Existing work

Parkasaurus Co-op adds an in-game session panel for a Steam friends lobby or a Direct IP connection. Both peers use their own Steam copies. [v0.1.1 preview](https://github.com/oshoubber/parkasaurus-coop/releases/tag/v0.1.1)

## How the game works for co-op

The host loads and saves the native park and runs the simulation. On join, the guest loads a snapshot, then receives authoritative updates. The mod routes supported guest actions through the host and checks shared-state digests. [Runtime source](https://github.com/oshoubber/parkasaurus-coop/tree/master/src)

## What changed

The mod adds shared building, terrain, finance, staff, and progression actions. The latest preview closes both day summaries after the host advances the day and cleans up the lobby before the game shuts down Steam. [Release notes](https://github.com/oshoubber/parkasaurus-coop/releases/tag/v0.1.1)

## QA

The two-client native harness passed eight checks on October 27, 2026, covering snapshot join, building placement and removal, replay rejection, pause and speed, and disconnect and reconnect. Separate native checks covered two day transitions and an own-account Steam lobby exit. The author also reported initial two-PC Steam play on an earlier build. [Test instructions](https://github.com/oshoubber/parkasaurus-coop/blob/master/CONTRIBUTING.md#tests)

## How to play

Install BepInEx 5.4.23.6 Windows x64 from its official release. Copy the mod ZIP’s BepInEx folder into the Parkasaurus game directory. Load a park, press F8, and invite a friend from a Steam friends lobby. [Installation and play guide](https://github.com/oshoubber/parkasaurus-coop#install)

## Known issues

The host handles saves, Next Day, quests, and some construction and management actions. The guest can still see camera or UI stutter. Large parks, DLC combinations, mission completion, and repeated internet reconnects need more coverage. [Known limits](https://github.com/oshoubber/parkasaurus-coop#what-works-and-what-is-still-in-progress)

## Next steps

Validate guest frame pacing and mission transitions across two PCs, then broaden support for guest management actions. [Contributing guide](https://github.com/oshoubber/parkasaurus-coop/blob/master/CONTRIBUTING.md)
