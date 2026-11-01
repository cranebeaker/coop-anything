# Captain Tsubasa: Rise of New Champions (2020)

| | |
|---|---|
| Engine | Native TamEngine/Tamlib, inferred from executable RTTI; exact version unknown |
| Version investigated | Windows Steam manifest build 10135064, executable version 1.4.1.0; SHA-256 in [NOTES.md](NOTES.md). Executable depot integrity unverified. |
| Project | [Local story co-op investigation](https://github.com/cranebeaker/coop-anything/issues/286) |
| QA level | None; no mod or in-game test yet |
| Last updated | 2026-11-01 |

## Where things stand

There is no tested local story co-op mod from this investigation yet. The [Steam listing](https://store.steampowered.com/app/1163550/) advertises local co-op and two story modes separately. [Bandai Namco's Versus page](https://captaintsubasa-csgame.bn-ent.net/mode/index62.html) specifies up to four offline players in that mode. Its [console specification](https://captaintsubasa-csgame.bn-ent.net/spec/) says multiple people on one console are supported only in offline Versus. The PC story mode still needs its own test.

## Existing work

- Searches of this hub's directory, game folders, issues and PRs found no prior entry before the [claim issue](https://github.com/cranebeaker/coop-anything/issues/286).
- Public GitHub, Nexus and web searches for a story co-op mod did not surface one. Search coverage and URLs are in [NOTES.md](NOTES.md); absence remains unproven.
- The [RONC Editor](https://www.nexusmods.com/captaintsubasariseofnewchampions/mods/49) edits game data and saves; it does not claim to add story co-op. Its author says source access is by private invitation.
- The [RONCE save editor](https://github.com/pdvnqsr/ronce) edits save files. It is not a co-op project.

## How the game works for co-op

The installed PC executable contains `TamEngine`/`Tamlib` RTTI names and separate `binary/Story` and `binary/CasualMatch` data. The story mode, local match controller assignment, save loading, and anti-cheat behavior still need mapping. The PC release has [Easy Anti-Cheat listed](https://www.pcgamingwiki.com/wiki/Captain_Tsubasa:_Rise_of_New_Champions); this must be confirmed on the exact build. No online services or anti-cheat code will be modified.

## What we changed

No game code has been changed. An isolated install copy and static analysis workspace were prepared. No live test has run.

## QA

No in-game checks have run. A valid result will need two independent controllers through a story match, cutscenes, result screen, and saved progression in both Episode: Tsubasa and Episode: New Hero.

## How to play

No playable story co-op package is available from this investigation.

## Known issues

- Story co-op remains unimplemented and untested.
- Steam Remote Play Together shares a supported local co-op mode; it does not add a second player to story mode.

## Next steps

Map the story match setup and controller allocation against Versus. Establish save isolation before any live test, then test a local-only patch and package only original mod code, never game files.
