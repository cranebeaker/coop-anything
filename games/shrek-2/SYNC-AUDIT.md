# Shrek 2 Enhanced: audit of the synchronisation (2026-11-01)

Why: the owner asked to check everything done for synchronisation against how UE2.5 games do it, to get rid of frame-rate drops and lag.
Method: counters in the engine layer (bytes and packets of the game's UDP, frame rate and longest frame per process), A/B runs with one part of the mod switched
off at a time (`?EnhOff=...`), host + one guest on one PC (second monitor, quiet windows), the guest's own log of its pawn. Tools: `netbench.ps1`, `fpsbench.ps1`,
`netsum.py`, `jitsum.py`, `[NET]` and `[FPS]` lines of `enhnative.log` (local, not in the repository).

## What exists

| Part | How it works | Standard? |
|---|---|---|
| Movement of the players | the engine's own: the guest's controller is the autonomous proxy, the pawn is predicted on his machine | yes |
| Animations (`EnhancedAnimSync`) | one always-relevant actor; every 0.04 s the host packs every channel of up to 32 pawns into strings (`Data[32]`, unreliable), the client applies a record when it changes | custom (the engine replicates only channel 0) |
| World (`EnhancedWorldSync` x4) | for actors that exist on the host only (`RemoteRole = None`: torches, fire, pollen, coin sparkles, gnats): the host picks the 80 nearest per part, sends `Rec` (reliable: class and name) and `Pos` (unreliable: position, angle, animation, ...), the client spawns a local twin and moves it | custom |
| "Wide" relevance | every replicated actor within 3500 uu of a player is made `bAlwaysRelevant` | custom |
| Cutscene camera (`EnhancedCam`) | always-relevant actor, 60 Hz | custom |
| Pause | `EnhancedGame.SetPause` ignored every request (fixed for single player) | no |

## Measurements (stock-like profile, swamp, host + guest)

* Host sends ~3.0 KB/s on average (peaks 9, 15 with simulated lag); the guest ~0.5 KB/s; ~18 packets a second each way.
* World sync is ~80 % of the host's traffic (578 B/s without it). Animations ~140 B/s.
* After making the repeat stamp per record (was one counter for ~100 records, every 2 s): 2.66 KB/s. The rest is the positions themselves.
* Loss of 5 %, or lag 40 ms + 30 ms jitter each way: the guest's pawn is not less smooth (snaps 19-26 a minute in every case).
* Guest frame rate, no test log: 38.8 fps with everything; 59.5 with the world and animation syncs off. Alone: twins ~16 fps, wide relevance ~7, animations ~7.
  The guest spends only 4-6 % of its time inside `Present`: it is limited by script and replication work, not by the graphics card.

## Findings

1. The twins carry mostly static decoration (torches, fire, pollen, sparkles) but their position is streamed all the time, and the guest walks 4 x 80 slots every frame.
   For decoration the standard way is local actors on every machine (copies are not linked and need no sync); a one-time list "this class stands here" is enough.
2. "Wide" relevance and the animation sync each cost the guest ~7 fps: every actor made always relevant has to be received and processed; the animation sync
   compares 32 records per frame and applies them.
3. Pickups that turn (coins, money bags, energy bars): the server makes them dumb proxies, the client keeps `PHYS_None` for them and a dumb proxy is not simulated.
   Fixed on the client: role `ROLE_SimulatedProxy` + `PHYS_Rotating` for those with a rotation rate (log: yaw 0 -> ~7800 in 0.51 s = the host's 15000 per second).
4. Rate caps of the stock ini (internet 10000, `MaxClientRate` 20000) sit close to the peaks (9-15 KB/s); with the fixes the average is far below.
5. Not a problem: loss and lag as such; movement; the bandwidth on average.

## Plan, by gain and risk

1. Decoration: send the list once (`unreliable if (bNetInitial)` or reliable, no `Pos` stream), create the actors locally; keep streaming only for creatures that matter.
2. Narrow the wide relevance to enemies and gameplay objects, smaller radius.
3. Guest: process the slots at 10-15 Hz, only the filled ones; animation sync only for heroes and near enemies.
4. Raise `ConfiguredInternetSpeed`/`MaxClientRate` (tuned by the layer where still stock).
5. `NetPriority` for hero pawns and the sync actors.

Sources: [Role](https://unrealarchive.org/wikis/unreal-wiki/Legacy:Role.html), [Replication Idioms](https://unrealarchive.org/wikis/unreal-wiki/Replication_Idioms.html), [Introduction To Replication](https://unrealarchive.org/wikis/unreal-wiki/Legacy:Introduction_To_Replication.html).

## Follow-up: UE2.5 comparison and current stop point (2026-11-01)

The older plan above is historical; several throttling/rate changes were implemented later. The current three-file, uncommitted patch adds staggered animation retransmission heartbeats, reliable cutscene-mode RPCs, and 10 Hz unposed-pawn probes. UE2 does not automatically resend an unchanged variable after loss, and `reliable` on a variable does not change that; discrete RPC transitions and continuously replaced unreliable pose data are separate concerns. Sources: [UE2 replication reference](https://www.unrealarchive.org/wikis/unreal-wiki/Everything_you_ever_wanted_to_know_about_replication_(but_were_afraid_to_ask).html), [Killing Floor UE2.5 source](https://github.com/InsultingPros/KillingFloor) (`KFMonster.uc`, `KFPawn.uc`, `KFGameType.uc`; no permissive license found).

Final reduced patch builds with 0 errors / 0 warnings. Its clean sandbox run completed at 32 fps and 12.9 snaps/min versus the unmodified baseline's 42 fps and 18.3 snaps/min; this is **not a measured performance win**. A dot-product distance experiment regressed to 17-19 fps and was fully reverted. The reduced patch's lossy run, cutscene exit, four-player coverage, and the owner-reported periodic client T-pose remain unverified. Testing was stopped by the owner to conserve usage. Full measurements, sources, file state, and resume steps are in `NOTES.md` under **UE2/2.5 replication review and stop point**.
