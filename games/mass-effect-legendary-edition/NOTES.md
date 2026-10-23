# LE3 research notes

These are author-run observations, not an independently completed campaign.
Immutable public sources: [progress](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/docs/PROGRESS.md),
[validation](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/docs/VALIDATION.md),
[activation packaging](https://github.com/MatrimAl/ME3SquadCoop/blob/10a575fa0f6e3cf668f8f4e1b2a905e14733e691/docs/DLC-PACKAGING.md), and
[ordinary-entry source used for the later run](https://github.com/MatrimAl/ME3SquadCoop/blob/ff397b0ad41e15f2ae1c7b38549a73a42217ecc3/src/squad_coop.c).

## Observed failures

### Ordinary entry and stable admission

An earlier protected ordinary pair opened the host listener but did not establish
a sustained guest connection. Its one-shot join briefly entered native pending
travel; all 213 valid guest world observations remained standalone. Dispatch
occurred at about 0.43 seconds of initial-world time.

A later completed rerun used source `ff397b0` and the new readiness policy. It
observed five stable samples spanning 1,000 ms before dispatch, at native world
time 68.026 seconds. The immediate console response was empty. A reflected
viewport observer recorded `Connecting...` and ended its bounded 60-second
window. No sustained native connection, Ashley acquisition, collision repair or
combat was established. Stored viewport progress is not a captured native
rejection event; the exact join error remains unknown.

The change therefore demonstrably delayed admission into a stable ordinary
player/story context, but did not solve the measured guest-join failure. Initial
readiness alone is not an established explanation for the two failures.

### Activation DLC

During the later run, the owner supplied an in-game corrupt-DLC dialog naming
`DLC_MOD_ME3SquadCoop`. The screenshot was not independently attributed to one of
the test processes. It is evidence of the displayed rejection, not proof that a
particular native call failed or that the active container was mounted.

Static review found that the authored Mount header and zero GUID/build tail
match the official LE3 writer. The prototype activation container has no
DLC-root `PCConsoleTOC.bin`, a concrete scaffold omission. Separate native review
identified a conditional coalesced-file failure path that also reaches corruption
UI. Neither finding establishes which check produced the observed dialog.

Primary format references are the pinned
[LE3 Mount writer](https://github.com/ME3Tweaks/LegendaryExplorer/blob/b5e746233491ec5a7cbfc077685996af37a3d125/LegendaryExplorer/LegendaryExplorerCore/GameFilesystem/MountFile.cs#L345-L365),
[DLC TOC construction](https://github.com/ME3Tweaks/LegendaryExplorer/blob/b5e746233491ec5a7cbfc077685996af37a3d125/LegendaryExplorer/LegendaryExplorerCore/Unreal/TOCCreator.cs#L223-L241), and
[current DLC starter generator](https://github.com/ME3Tweaks/ME3TweaksCore/blob/b5f006add38dbaea91dde3f16f45ecbc3b0155a9/ME3TweaksCore/ME3Tweaks/StarterKit/DLCModGenerator.cs#L103-L185).
They establish format/tool behavior, not exact-build engine acceptance. The
current generator creates named coalesced and language TLKs for Game3; startup
packages are optional, and an SFAR is added only for original ME3, not LE3.

### Combat, collision and scene handling

Earlier protected research runs consumed authoritative ammunition in some
bounded cases. Unchanged-build repeats also failed while the host weapon's
port-arms animation flag remained set. RPC arrival, client prediction and
ammunition consumption do not establish repeatable firing/stop or authoritative
enemy damage.

A read-only paired capture observed guest collision clearing during scene
release and staying disabled after reacquisition. A real landing conversation
occurred in ordinary mode 0 and held the scene gate for about 13.39 seconds;
mode 7 alone is therefore not a complete scene predicate for that route.
Synthetic handoffs remain separate from actual full-conversation validation.

## Inferences and unknowns

The delayed join rules out treating the first transient control frame as a
sufficient admission test. It does not isolate transport, DLC rejection or any
other root cause. Missing TOC and conditional coalesced handling are investigation
leads; changing arbitrary Mount GUIDs is not supported by the reviewed writer.

Collision restoration, reliable equipment and combat, full conversations,
sustained mission travel, save/load, two physical PCs over Radmin, more than one
guest and end-user Mod Manager installation/disable/rollback remain open.
Original-file/profile protection and clean bounded shutdown were checked for
the reported runs; they are not a guarantee for all future sessions.

## Next bounded investigations

1. Attribute the DLC rejection to a process and native predicate. Verify an
   independently authored minimal scaffold without copying game payloads or
   disabling the corruption check. Separate parser acceptance from engine mounting.
2. Retain actual native travel/error evidence and establish an ordinary guest
   connection before claiming an ordinary ownership or collision-repair result.
3. Continue combat through the protected research entry, which omits the
   activation DLC. Measure real host firing/stop and authoritative damage, then
   scene control return and progression; preserve failed repeats as evidence.

Keep raw captures, saves, profiles, executable identity and machine details
private. Publish only authored code, sanitized observations and reproducible
offline fixtures in the project's own repository.
