# Firewatch — working notes (2026-11-14)

Local notes from the session that produced `README.md` and `patch/`. Not player
documentation. Paths are relative to the workspace; the owner's install was never
modified.

## Identity (pinned)

| Field | Value |
|---|---|
| Steam appid / buildid | 383870 / 7926499 |
| Game version | v1.1.2 (player log) |
| Unity | 2017.4.40f1 (exe version 2017.4.40.7214086), Mono, x64 |
| `Firewatch.exe` SHA-256 | `170AA58530CEFBD07F24B1A10C95BB8CC1DDE8CB36642627D1E4D976FA92D7C0` |
| `Assembly-CSharp.dll` SHA-256 | `145B56AEBE921C0FDC660A4E64008272E09A283332146958CE2F356C7490522A` |
| Loader | none shipped; BepInEx 5.4.23.6 x64 added for testing |

## Build

- Upstream `StoryCoop_src/` compiles against this install's `Managed` DLLs with Roslyn
  (C# 7.3, `/nostdlib` against the game's `mscorlib` 2.0). Zero warnings of substance.
- The repo's own build entry points are unusable: `Shoshone-Co-op.csproj` compiles the old
  UNET prototype (`Class1.cs`) and references a `Libraries\` folder that is not in the
  repo; `StoryCoop_src/build.sh` hardcodes `/mnt/user-data/uploads/Firewatch/...`.
- Shipped 0.5.0 DLL references BepInEx 5.4.23.5 + 0Harmony 2.9.0.0; BepInEx 5.4.23.6
  bundles 0Harmony 2.9.0.0, so it binds.

## API audit (observed, reflection against the shipped assembly)

All 75 type/member checks resolve, including every Harmony target:
`vgMainMenuController.OnNewGameStart` (private), `vgEventManager.TriggerSingleEvent`
(private, 3 args), `vgTargetInteract.TriggerEvent` (protected), `vgSaveManager.Load(vgSaveData)`
(private), `vgEventManager.SetFact(string, vgFactOperation, float, vgBlackboardLocationHint)`,
`vgDialogTreeManager.activeDialogPool` / `ClearDialogTree`, `vgUtils.IntentionalDestroy`
(`delay` defaults to 0), `SECTR_Sector.All`, `SECTR_Chunk.IsLoaded/IsLoading`.

## Observations vs derived vs unknown

Observed:
- BepInEx + mod load, menu entry appears, host/join, snapshot 827 KB raw / 46 KB gz,
  214 saveables, 9 chunks.
- 0.5.0 world interaction fails: client logs `object not loaded` while holding the object.
- `ObjectId` ids are transform paths; SECTR root name contains `/`.
- Client did not spawn beside host (identical coordinates).
- Both peers diffed facts; `TimeSinceLastSpeech` oscillated.
- Client day -1 while host 0.
- Avatar: no runtime animator controller on the clone, screen-filling label, black head sphere.
- Disconnect handling is clean (avatar destroyed, back to hosting).

Derived (confirmed by test after the patch):
- Percent-escaping names fixes every path-based lookup.
- Host-authoritative facts stop the oscillation.
- `currentDayValue` + `currentDayFact` carry the day.

Unknown / not tested:
- Two-machine LAN, firewall/NAT, host joining mid-level.
- Dialog mirroring end to end, scripted trigger volumes end to end.
- Day transition and save/load handoff inside a full playthrough.
- Whether cutscenes tolerate two cameras.

## Security notes

- TCP 7778, no authentication, first connection wins.
- `BinaryFormatter` deserialization of peer bytes in `GameApi.ApplySnapshot` and
  `WorldSync.ApplyState` — a known arbitrary-object/RCE surface.
- Peers can trigger story events by name, destroy objects and teleport the other player.
- Consequence: LAN-only, trusted players. Do not port-forward this build.

## Test harness

`harness/` contains the driver and the suite used for the QA claim. It lives outside the
mod: a BepInEx plugin that reflects into `StoryCoop.CoopRunner`, plus file-based commands
(`host`, `join`, `interact`, `fact`, `fsmevents`, `tp`, `shot`) and a JSON state dump.
The suite drives two copies and prints `RESULT: PASS|FAIL`.

Environment used: Windows 11, two copies of the install in a temp workspace, save folder
redirected with a directory junction so the owner's saves were untouchable (24/24 files
hash-identical after restore).

## License

Upstream has no LICENSE file and no license field on GitHub. Treat it as all-rights-reserved:
the fix is published as a patch; no rebuilt DLL and no upstream source are included here.
Ask the author before redistributing a build.
