# S.T.A.L.K.E.R.: Shadow of Chernobyl (2007) and Clear Sky (2008)

| | |
|---|---|
| Engine | X-Ray (Shadow of COOP runs on the xrMPE multiplayer extension, Call of Pripyat based) |
| Projects | Shadow of COOP 8.0 (Arcanum Team, xrMPE addon) + our fix patch [stalker-soc-coop-fixes](https://github.com/cranebeaker/stalker-soc-coop-fixes); Clear COOP 0.5 (abandoned Nov 2024) |
| QA level | automated (fix patch: boot test + in-engine self-test); none (Clear COOP) |
| Last updated | 2026-10-16 |

## Where things stand

- **Shadow of Chernobyl:** Shadow of COOP is playable through the story. A fix patch
  (an extra `.db` archive appended to the addon) fixes an endgame crash and other bugs.
  Release: [stalker-soc-coop-fixes](https://github.com/cranebeaker/stalker-soc-coop-fixes/releases).
- **Clear Sky:** Clear COOP 0.5 ships 8 levels and the story was playable only to
  Agroprom. We started a port to xrMPE 0.9 but stopped; it isn't being worked on. The notes
  on this page are there for anyone who wants to pick it up.

## How the game works (for co-op)

- `coop.addon` is a tar of X-Ray `.db` archives; the launcher loads every `.db` inside,
  so patches are appended rather than repacked. The server checksums `*.addon`, so all
  players need identical files.
- X-Ray `.db` index: chunk 1, LZHUF with MAX_FREQ=0x4000 (the textbook 0x8000 corrupts
  output after about 97 KB); file data LZO1X.
- Quest flags are server-global and dialog uses whoever talks, so clients can likely
  hand in quests.
- xrMPE 0.9 loads `<name>.dscript` files appended to base scripts: use them instead of
  forking base scripts.

## What we changed

- Endgame crash: levels referenced breakable models (`meshes\brkbl#N.ogf`) that never
  shipped; 1 KB stand-ins for the missing indices fix all 18 levels.

## QA

- Boot test every level on a dedicated server with a local client, watching the log for
  `Connection finished` or `FATAL ERROR`.
- In-engine self-test archive hooked from a space restrictor's logic (20/20 checks).
