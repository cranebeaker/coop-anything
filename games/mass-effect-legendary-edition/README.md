# Mass Effect 3 in Legendary Edition (2021)

| | |
|---|---|
| Engine | Unreal Engine 3, Windows x64 |
| Version observed | LE3 executable 2.0.0.48602; storefront equivalence unknown |
| Project | [ME3SquadCoop](https://github.com/MatrimAl/ME3SquadCoop), MIT source research prototype |
| QA level | none; author-run bounded experiments, no gameplay release |
| Last updated | 2026-10-23 |

## Where things stand

ME3SquadCoop researches campaign co-op with host Shepard and one guest controlling
an existing squadmate, initially Ashley. The source and offline test tooling are
public; players cannot install a validated campaign release from this write-up.
The [claim issue](https://github.com/cranebeaker/coop-anything/issues/135) links the work.
Additional participants are a future goal, not supported player counts.

## Existing work

The directory separately lists [LEMP](https://www.nexusmods.com/masseffectlegendaryedition/mods/3463)
for the restored wave mode. The [original ME3 campaign-mission proof of concept](https://www.nexusmods.com/masseffect3/mods/723)
is a historical reference for a different game build. This entry replaces only
the Legendary Edition campaign gap and preserves the wave-mode project.

## Research observations and changes

- Protected two-process research runs exercised native networking, Ashley
  possession, short movement, synthetic scene release/reacquisition and
  disconnect-to-AI restoration. These bounded observations are documented in
  [validation](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/docs/VALIDATION.md); synthetic scene tests are not full conversations.
- The separate ordinary runtime opened native Listen in its current Mars world
  without a mission reload. An independent world reader observed listen mode,
  and the operating system attributed the configured local UDP endpoint to the
  host process. This establishes local host startup, not transit between PCs.
- Startup inspection corrected a command-buffer address by deriving it from a
  pinned native instruction. It also corrected the player-class predicate to
  the observed `SFXPawn_Player` hierarchy. Native fixtures reject the previous
  address and class assumptions.
- A measured collision regression after scene reacquisition motivated a
  default-off restoration candidate with offline coverage. The research result
  is the captured regression and guarded candidate, not a gameplay repair.
- Ordinary join now waits for stable reciprocal player/story context and
  captures the immediate console response plus bounded viewport progress text.
  The actual rerun exercised that policy but failed to sustain a connection.
  Failures and alternative explanations are separated in [NOTES.md](NOTES.md).

See the dated [progress record](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/docs/PROGRESS.md),
[ordinary-runtime design](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/docs/RUNTIME-INTEGRATION.md), and
[admission/response source snapshot](https://github.com/MatrimAl/ME3SquadCoop/blob/ff397b0ad41e15f2ae1c7b38549a73a42217ecc3/src/squad_coop.c).
The implementation lives in its own repository; no game code or assets are included here.

## QA and reuse

The author's integrated offline suite ran **257 tests: 255 passed, 2 skipped** on
2026-10-23. Native harnesses and compiled negative controls also ran. Those checks
exercise project contracts; they do not establish a playable campaign. The
conservative directory label remains `qa: none` rather than treating short
research sessions as automated campaign validation.

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

Build and native-harness commands are in the source project's [README](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/README.md).
Real-client experiments require a separately verified independent copy, private
profiles, original-file protection and bounded cleanup. A fresh source clone
provides no game files or private test prerequisites.

## Availability

Use the linked source repository for research and contributions. Source, offline
fixtures and dated observations are available; a gameplay release is not.
Failed ordinary joins, inconsistent combat, activation-DLC rejection and next
investigations are recorded in [research notes](NOTES.md).
