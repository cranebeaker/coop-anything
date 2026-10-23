# Granny (2018)

| | |
|---|---|
| Engine | Unity **6000.0.58f2**, **IL2CPP** (global-metadata v31), D3D12 |
| Versions tested | Steam **buildid 21006637** (v1.3.4), Windows 11, Steam app **962400** |
| Projects | [granny-coop](https://github.com/roosterdevv/granny-coop) — MelonLoader mod, MIT, protocol v2, report below (README also in [Russian](https://github.com/roosterdevv/granny-coop/blob/main/README.ru.md)) |
| QA level | `automated` — scripted two-instance runs against real game clients |
| Last updated | 2026-10-23 |

## Where things stand

Two players can host and join and see each other move. Granny is simulated on the host
and pinned on the guest, and doors open and close for both. Item flags cross the wire in
both directions together with an owner byte: the picker drives the game's real hand slot and
the other player sees a copy in the picker's avatar hand, so one pickup shows in one hand
(v0.7.0). Avatars stand at 1.85 m (0.95 m crouched) on the peer's real feet. The host's
settings menu no longer stops the world: the menu opens, but `Time.timeScale` is put back to
1 and Granny keeps walking for both. The catch sequence, the van and the day cycle are
not synced. A guest follows the host between
scenes, including through the game's own boot, and nobody has played a full round in
co-op. Every run so far was two game processes on one PC talking over
loopback or through a local chaos relay; no run has crossed a real network.

## Existing work

There is no co-op mod for this game. What exists targets other builds or other games:

- *Granny: Escape Together* (Steam **3070520**) — a separate commercial game by a
  different developer, not a mod and not this build.
- FedoraV3's Granny TAS tooling, GDmio's legacy menu mod, lucaandri007's mods and
  an666667's fan game — studied for technique only; the type names they patch
  (`MobileFPS`, `AI_Granny`, …) have **0 occurrences** in build 21006637, so none of them
  loads here.

Details and links in `games/granny/NOTES.md` §3–§4.

## How the game works (for co-op)

- No netcode of any kind. Steamworks.NET is compiled in but unused by gameplay; the mod
  does not touch Steam.
- All logic is managed C# compiled to IL2CPP. The metadata keeps class, field and method
  names, so Harmony patches through MelonLoader work; the loader's generated
  `Il2CppAssemblies` are the reference assemblies.
- Player controller `FirstPersonController_Egen`; the body is found with
  `GetComponentInChildren<CharacterController>()`.
- Doors are `Door` components under `All doors/…`. Enumeration order differs between
  installations, so the mod keys each door by an FNV-1a hash of its transform path.
- Inventory is a fixed table of `have<Item>` flags on `InventoryController`.
- Play path for a bot: Play → difficulty panel → Continue → `Scene`. MelonLoader's file
  log only flushes on process exit.

## What we changed

Everything is in [granny-coop](https://github.com/roosterdevv/granny-coop); the notes
file there is the same one as `games/granny/NOTES.md`.

| Feature | What it does | Test that proves it |
|---|---|---|
| Player sync | 20 Hz transform stream, remote avatar with name label | two-instance runs, §11–§12 |
| Granny sync | host publishes transform + AI flags at 20 Hz; guest disables `NavMeshAgent` and pins the transform with a 2.5 m snap | joint tracking to 0.1 m, §12 |
| Doors | 48 doors, `State` / `Snapshot` / `Request` / `SnapshotRequest`, host validates (locked stays locked), 10 s snapshot repair | 0 mismatches in 13 paired samples with one flipper, §23; guest→host request path, §16 |
| Items | `have` flags, change events plus a 10 s snapshot, both directions, owner byte (none/host/guest); the picker drives its real hand slot, the peer gets a clone in the peer avatar's hand | `-ItemTest` v0.7.0: guest's own hand stayed empty while the host drew `peer holds Hammare`, host applied `owner=guest`, §29; human pickup untested, §27 |
| Avatar height | feet position + crouch flag on the wire; avatar built from the feet at 1.85 m / 0.95 m crouch, lerped ~0.2 s | `-ItemTest` v0.7.0: `avatar created at feet (…), height 1.85`, crouch `cc.height 1.16` vs `2.45`, §29 |
| Pause | while connected in `Scene`, restore `Time.timeScale = 1` and clear `AudioListener.pause` every frame; skipped during the death sequence and spectator view | `-PauseTest` v0.7.0: two simulated menu pauses, each restored on the next frame, Granny moved 3 m through the window, §29 |
| Scene follow | the host owns the scene; the guest loads what it announces, outside the socket callback, with retries | one client + scripted host: Menu → Scene, Scene → Menu, and a boot-time request held until SplashScreen ends, §24 |
| Late join | a guest that loads after the host is repaired from snapshots, not an event log | §16 |
| Failure paths | kill client and rejoin; kill host; restart host under a live guest | §17–§19 |
| Impaired link | survives the `bad_wifi` chaos profile and a full 8 s two-way outage | §20, §22 |

Protocol: UDP, magic `GCP1`, 9 message types, 20 Hz state, 5 s peer timeout; **v2** (adds the item owner byte and the feet/crouch state), v1 peers are rejected. The sequence
number is written but not used for ordering — the design relies on idempotent snapshots.

## QA

```
powershell -ExecutionPolicy Bypass -File tools/run-pair.ps1 -Seconds 300 -DoorTest
powershell -ExecutionPolicy Bypass -File tools/run-pair.ps1 -Seconds 240 -DoorTest -Relay `
    -RelayProfile bad_wifi -RelaySeed 1042 -ChaosRoot <coop-anything checkout>
python -I tools/analyze-soak.py <run file>
python -I tools/fake-host.py --port 47778 --scene Menu --scene-at 10 --scene-until 20 `
    --scene Scene --scene-at 25 --scene-until 40 --seconds 60
```

`tools/fake-host.py` plays the host side of protocol v2 (a `Welcome` plus scheduled
`Scene` messages and an idle heartbeat), so a join-side feature can be tested with one
game instance (~2.7 GB) rather than a pair (~5.4 GB).

The harness starts two isolated copies of the game with MelonLoader, refuses to run if a
`Granny.exe` is already up, tracks only the processes it started, and stops them at the
end. It can kill/relaunch either side on a timer and can put the co-op-anything chaos
relay between the two copies.

Last suite results, 2026-10-23 (v0.7.0): `-ItemTest` 150 s — guest picked up the hammer,
its own hand stayed empty while it drew the item in the host's hand, host applied
`owner=guest`, avatars created at the feet at 1.85 m, crouch detected; `-PauseTest` 150 s —
two simulated menu pauses each restored on the next frame (`Time.timeScale 0 -> 1`, after
15 s `timeScale=1.00 audioPaused=False`), Granny travelled ~3 m through the window and the
guest's `gRemote` matched the host. Earlier the same day: door test with the host as the only
flipper, **0 door mismatches in 13 paired status samples** (cut short at 220 s by memory
pressure, ~2.7 GB private bytes per instance), late join, client→host door requests, client
kill and rejoin, host kill, host restart, `bad_wifi`, an 8 s outage (recovered in 10.07 s),
and scene follow in both directions with the boot request held until `SplashScreen` ended
(§24).

Not covered: a real network, three or more players, a full playthrough, re-entry after a
catch.

## How to play

There is a release: [granny-coop v0.7.0](https://github.com/roosterdevv/granny-coop/releases/tag/v0.7.0)
(`GrannyCoop.dll` plus a RU/EN install guide). To build from source:

1. Copy your Granny install to a separate folder (do not mod the only install).
2. Install MelonLoader **0.7.3** into that copy and run the game once so it generates
   `Il2CppAssemblies`.
3. `set GRANNY_TEST=<that folder>` then `dotnet build -c Release` in the mod repo; copy
   `bin/Release/GrannyCoop.dll` into `<game dir>/Mods/` (or unzip the release into it).
4. Host: `Granny.exe --coop-host --coop-name=Host`. Guest:
   `Granny.exe --coop-join=<host ip> --coop-name=Guest`.
5. UDP **47777** must reach the host. Two machines on one LAN, or on a virtual LAN
   (ZeroTier/Tailscale/Radmin), is the whole setup.

## Known issues

- **The pause fix was tested against a simulated pause, not the real menu.** A mod cannot
  reliably press the game's own settings button (the canvas is il2cpp-owned and the button
  has no stable name), so the test performs what the menu does to the world
  (`Time.timeScale = 0`, `AudioListener.pause = true`). If the real menu also blocks
  something else, that is untouched.
- **The peer item is a replica.** The picker uses the game's real hand slot; the other
  player sees a clone with colliders and scripts off. It may not match the game's own hand
  prefab visually.
- **The peer avatar is a capsule.** Height (1.85 m / 0.95 m crouch) is now right, but it is
  not a character model and has no crouch pose.
- **The item flag revert was our own `CheckInventory` call.** A probe found the write
  holds for at least 60 s and the game's `Update` leaves it alone; only an explicit
  `CheckInventory()` (which re-derives flags from world objects) reverts it, and the only
  caller was the QA refresh. v0.6.0 stopped calling it and mirrors the objects itself.
  What is still untested is a **human** pickup: whether `PickUp`/`PickUpNew` write the
  same `have` flags, and whether the hand replica matches the game's own hand prefab (§27).
- **The catch is not synced.** No jail, no death screen, no scene change is sent. What
  happens when the caught player is let back in is untested.
- **Scene follow skips the difficulty panel.** A guest that follows the host into `Scene`
  loads the scene directly instead of using Play → difficulty → Continue. The world builds
  and the player becomes active, but the path has only been checked against a scripted
  host, not a real pair, and whether the skipped choice matters later is unknown.
- **Door visuals after a remote apply are unverified.** The state is read back from the
  game's component; animation, collider and sound were not checked on screen.
- **One machine only.** No run has crossed a real network.
- **No GUI.** MelonPreferences plus command-line flags.

## Next steps

1. Make the peer avatar look like the player (skin the character model, crouch pose), then
   catch the human pickup path. The assembly has no "pickup" action method to call — all 37
   name hits are property accessors (§28) — so patch `PickUp.Update`/`PickUpNew.Update`,
   watch the flags around one real pickup, and replace the hand replica with the game's own
   prefab if it differs.
2. Sync the catch: the jail/scene transition and the respawn, then re-entry.
3. Confirm scene follow in a real pair and decide whether the guest should also see the
   Play/difficulty step before entering `Scene`.
4. Run a pair across a real network with the chaos relay in front, then a 30 min soak.
5. A third player; the protocol has no hard limit but none has been tested.

## License

The mod is MIT-licensed ([`LICENSE`](https://github.com/roosterdevv/granny-coop/blob/main/LICENSE)).
The chaos relay it tests against is MIT too, from
[coop-anything](https://github.com/cranebeaker/coop-anything).
