# Captain Tsubasa: Rise of New Champions — local story co-op investigation

## Goal

Enable two or more people on one PC to play the story campaign matches together, preserving normal story progression. Target platform supplied by owner: PC, Steam, local co-op.

## Game identity for this investigation

- OS: Windows.
- Store: Steam, app 1163550, manifest build ID `10135064` (manifest metadata does not prove the executable is pristine).
- Executable version: `1.4.1.0` (Windows file version metadata).
- Executable SHA-256: `FC29F752C4688CC91594D8B32A389463281A8F8F6F17ECC7977CDFA64A7397C2`.
- Engine: native `TamEngine`/`Tamlib` strings and RTTI in the executable; precise engine version unknown. This is a binary observation, not an official engine identification.

## Observations

- The Co-op Anything directory and `games/` have no entry for this title on the `main` branch inspected on 2026-11-01. Source: https://github.com/cranebeaker/coop-anything/blob/main/DIRECTORY.md and https://github.com/cranebeaker/coop-anything/tree/main/games
- Search of the hub's issues and PRs for `Captain Tsubasa` returned no results on 2026-11-01. Source: https://github.com/cranebeaker/coop-anything/issues and https://github.com/cranebeaker/coop-anything/pulls
- A claim issue was opened after this search: https://github.com/cranebeaker/coop-anything/issues/286
- Bandai Namco's product specification says more than one player on a single console is supported only in offline `VERSUS`; its online mode supports one person per console. This specification covers PS4 and Switch, so it does not by itself prove the PC implementation. Source: https://captaintsubasa-csgame.bn-ent.net/spec/
- Bandai Namco's mode page explicitly describes up to four offline players in `VERSUS`, with one controller per person for human-versus-human matches. The page does not promise multiplayer in Dramatic Story. Source: https://captaintsubasa-csgame.bn-ent.net/mode/index62.html
- The Steam store lists local and online co-op, Remote Play Together, two story modes, and four-player Versus as separate features. It does not state that story supports co-op. Source: https://store.steampowered.com/app/1163550/
- SteamDB lists `10135064` as the current public branch build ID, matching the local Steam manifest. Source: https://steamdb.info/app/1163550/depots/
- Bandai Namco describes Episode: Tsubasa and Episode: New Hero as the two story modes. Source: https://en.bandainamcoent.eu/captain-tsubasa/news/captain-tsubasa-rise-of-new-champions-out-now
- Public game documentation lists Easy Anti-Cheat for the PC release. This is secondary evidence and must be checked against the installed build; no anti-cheat workaround has been attempted. Source: https://www.pcgamingwiki.com/wiki/Captain_Tsubasa:_Rise_of_New_Champions

## Derived

- Reusing the game's existing local player and controller allocation in story matches may be less work than implementing a new network protocol. This is a hypothesis that needs binary and in-game checks, not an observed capability.
- Steam Remote Play Together could let distant friends join a supported local co-op mode, but cannot by itself enable a second story player.

## Guesses

- Story match setup may force one human controller slot while Versus permits more. Unknown until a matching PC build is inspected.

## Unknowns

- Whether this executable matches the Steam depot's bytes, anti-cheat behavior, and whether a second controller can be assigned in story through existing UI or configuration.
- Whether any existing story co-op mod exists; public search did not find one, but absence is unproven.
- Whether Episode: Tsubasa and Episode: New Hero use the same match setup path.
- Whether the Steam build allows an offline story-only mod without interfering with Easy Anti-Cheat or online services.

## Checks performed

- Read Co-op Anything `AGENTS.md`, `README.md`, `playbook/triage.md`, the directory and game-folder index; searched matching hub issues and PRs.
- Searched official store and publisher descriptions plus public mod searches.
- Searched GitHub repositories, code-indexed web results, [Nexus's game mod index](https://www.nexusmods.com/captaintsubasariseofnewchampions/mods/top), Steam discussions, and Japanese, Spanish, and French web queries for an existing story co-op mod; none surfaced in those searches. This is not proof that none exists.
- Prepared an isolated test copy and confirmed its executable hash matches the source. No mod or in-game test has run.
- The executable includes `SHGetSpecialFolderPathA` and `Savedata` strings. Changing only `APPDATA` is not enough to guarantee save isolation; a separate Windows profile or confirmed path redirection is needed before a game launch.

## Next check

Finish static analysis of story and Versus controller setup. Establish save isolation and verify the test copy's integrity before any in-game test. Do not change the original install or saves.
