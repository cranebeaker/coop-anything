# Probe stream format

animcheck never touches a game. It reads **probe streams**: JSON lines that a small piece of code inside
(or next to) the game wrote while the game ran. This page says what that code must emit. Any game, any engine:
if you can read an actor's position, its playing animations and its skeleton once per frame, you can produce
this stream.

The reference probe for the format is the Fallout: New Vegas one, in the fnv-coop repo under `plugin/animprobe`
(https://github.com/cranebeaker/fnv-coop). It is game specific (it reads engine memory), so only the format is
shared here. Use it as an example of what a probe looks like, not as code to reuse.

## The file

One JSON object per line (JSONL), UTF-8, one file per machine (or per game instance):

```
host.probe.jsonl
client.probe.jsonl
```

Lines that are blank, start with something other than `{`, or are not valid JSON are skipped, so comments and
partial last lines are harmless. Any extra fields are ignored.

## One probe (one line)

| Field | Type | Required | Meaning |
|---|---|---|---|
| `actors` | array | yes | The actors sampled in this probe, see below. |
| `t_ms` | number | yes, unless `rx_ms` is on every line | The game's own clock in milliseconds when the sample was taken. Must only grow. |
| `rx_ms` | number | no | Wall clock of the collector in milliseconds, shared by every machine on one PC. `collect` adds it. When every line of a file has it, the analyzer aligns machines with it. |
| `machine` | string | no | A name for the machine ("host", "client"). The file name or the `name=path` argument can give it instead. |
| `frame` | integer | no | The game's frame counter. |
| `v` | integer | no | Your own format version. |

Sample at 10 to 30 Hz. Faster is fine. A gap of more than 600 ms between two samples of one actor breaks a
streak (`gap_ms`), so do not sample slower than a few times per second.

## One actor

An actor is anything that has a body in the world: the player, a companion, an NPC, a puppet that stands in
for a remote player.

| Field | Type | Required | Meaning |
|---|---|---|---|
| `ref` | integer or string | yes | Stable id of the actor. An integer, or a hex string (`"0x0010A2B3"`, `"FF000A"`). The same id every sample. If your game uses names for ids, turn them into numbers in the probe. |
| `pos` | [x, y, z] | yes | World position in game units. **Z is up.** Convert in the probe if your engine uses Y up. |
| `rot` | [pitch, roll, yaw] | yes | Euler angles. `rot[2]` is the facing (yaw about Z), `rot[0]` is the aim pitch. Radians unless the profile says `"angle_unit": "deg"`. |
| `cell` | integer or hex string | recommended | Id of the area or level the actor is in. A change of `cell` is a teleport, not movement. Use 0 if the game has no such thing. |
| `valid3d` | bool | no (default true) | False while the body is not loaded or not visible. Such samples are skipped by the glitch detectors. |
| `dead` | bool | recommended | The actor is dead. |
| `player` | bool | no | True for a real player character. |
| `first_person` | bool | no | The camera is first person. The skeleton in `bones` must still be the full third-person body (see below). |
| `anim` | object | recommended | What is playing, see below. Without it the actor is never judged on animation. |
| `bones` | object | recommended | World positions of bones, see below. Needed for the pose score and the T-pose check. |
| `weapon` | object | no | `{ "out": bool, "aiming": bool, "form": id }`. `out` = a weapon is drawn, `form` = which item. |
| `stance` | object | no | `{ "knocked": int, "swimming": bool, "ragdoll": bool, "sneaking": bool }`. `knocked` is 0 when upright, above 0 when knocked down or staggered. |
| `move_flags` | integer | no | A bit field of movement states. Only read if the profile has `move_flag_bits` (see README), as a way to get `sneaking` and `swimming`. |
| `ground_z` | number | no | Height of the ground under the actor, if the probe can ask the game for it. Makes the floating and sinking checks exact. Without it, the analyzer tracks the last grounded height itself. |
| `ai` | object | no | `{ "in_combat": bool, "combat_target": id, "package": id }`. Read, but not used by any check. |

### `anim`: what is playing

```json
"anim": { "seqs": [ { "slot": 0, "group": 0, "state": 1 }, { "slot": 1, "group": 3, "state": 1 } ] }
```

Each entry of `seqs` is one animation that the game's animation system holds for this actor:

| Field | Meaning |
|---|---|
| `slot` | Which channel or layer it plays on (any small integer). Two samples are "the same animation" when slot and group match. |
| `group` | The animation group, state or clip. An **integer id** or a **string name**. The profile turns it into a category (idle, walk, run, and so on). |
| `state` | `1` = playing. Anything else (or missing) = loaded but not playing. Only playing entries count. |
| `t`, `begin`, `end`, `freq`, `weight` | Optional playback time, range, speed and blend weight. Read, but not used by any check. |

How to map an engine onto this: take whatever the game calls the current animation state, one entry per
active layer or channel. For example, an animator with layers: the layer index is `slot` and the current state
name is `group`. A sequence-based engine: the sequence slot is `slot` and the sequence's group id is `group`.
Report every animation that is blending in or out too, with `state` 1 while it still has weight.

An actor with an `anim` block but no playing entry is judged as "no animation" (the T-pose check). An actor with
no `anim` block at all is not judged on animation.

### `bones`: the skeleton

```json
"bones": { "pelvis": [1000, 2000, 140], "head": [1000, 2000, 186], "hand_l": [1000, 1988, 146] }
```

An object from bone name to **world-space** position `[x, y, z]` (not local, not relative to the parent).

- The names are the engine's own node names. The profile tells the analyzer which name is the root bone and
  which are the upper arms and hands. Nothing else needs a profile entry.
- Send 10 to 16 bones: root (pelvis or hips), spine, neck, head, both upper arms, forearms, hands, calves and
  feet are a good set. The pose score needs at least 3 bones besides the root present on both machines.
- Send the **same set of names on both machines**. The score compares only the bones both sides report.
- Send the full third-person body even when the player is in first person. A first person arms model gives a
  false T-pose and a false pose error. If the engine hides the third-person body in first person, read its
  skeleton anyway; if that body is not animated then, report `valid3d: false` so the samples are skipped.
- Position error is measured after aligning the root bone and turning each skeleton by its own `rot[2]`, so a
  different spot or facing does not count as a pose error. Only the shape of the pose does.

## Example line

```json
{"machine":"host","t_ms":1250.0,"frame":75,"actors":[{"ref":"0x00000014","pos":[1000.0,2000.0,100.0],"rot":[0.2,0.0,1.57],"cell":"0x0000DEAD","valid3d":true,"dead":false,"player":true,"anim":{"seqs":[{"slot":0,"group":1,"state":1}]},"weapon":{"out":true,"aiming":true,"form":"0x00001234"},"stance":{"knocked":0,"swimming":false,"ragdoll":false,"sneaking":false},"bones":{"pelvis":[1000.0,2000.0,140.0],"head":[1000.0,2000.0,186.0],"upper_arm_l":[1000.0,1990.0,172.0],"hand_l":[1000.0,1988.0,146.0],"upper_arm_r":[1000.0,2010.0,172.0],"hand_r":[1000.0,2012.0,146.0]}}]}
```

`python -I tools/animcheck/cli.py demo --out DIR` writes two full example streams you can open.

## Several machines

To compare what a client shows against what the owner has, record one stream per machine **at the same time**
and pass them together: `cli.py analyze host.jsonl client.jsonl`. The first one is the owner (the source of
truth); the others are observers.

- **Same ids.** An NPC that exists on both machines should have the same `ref` on both. If the ids differ, give
  an id map in code (`analyze(..., ref_maps=...)`).
- **Avatars.** A player's own character on their machine and that player's puppet on another machine have
  different ids. Tell the analyzer with `--avatar NAME=host:14>client:FF000A` (owner machine and id, then
  observer machine and id). Avatars are scored apart from NPCs, under the `avatar.*` counters, and an avatar
  is never counted in the NPC numbers.
- **Clocks.** Two game processes have two clocks. On one PC, poll them with `collect`, which stamps `rx_ms` on
  every probe, so both streams share one clock. If you only have `t_ms` of different processes, use
  `--auto-offset`, which finds the clock difference that makes positions agree best.
- **Interpolation buffer.** A client usually shows remote actors about 100 ms in the past. The comparison
  expects that (`--delay-ms`, default 100) and searches 150 ms either way for the best match (`--latency-ms`).

## What each check reads

| Check | Fields it reads |
|---|---|
| sliding | `pos`, `cell`, `anim.seqs` (group category), `dead`, `stance` |
| tpose | `anim.seqs`, `bones` (upper arms and hands), `dead`, `stance` |
| floating, sinking | `pos`, `ground_z`, `cell`, `anim.seqs` (jump), `stance` |
| jitter | `pos` |
| rubberband | `pos`, `cell` |
| stuck | `pos`, `anim.seqs`, plus time windows you give with `--expect-moving` |
| deathpose | `dead`, `anim.seqs`, `stance.ragdoll` |
| pose error | `bones`, `rot` |
| group mismatch | `anim.seqs` (slot and group) |
| wrong weapon, wrong aim, wrong facing, stance | `weapon`, `rot`, `stance` |

A probe that leaves a field out simply turns the checks that need it off for that actor.

## Collector protocol (optional)

If your probe cannot write files, it can answer requests, and `cli.py collect` polls it. One JSON object per
line over TCP:

```
request:  {"method": "anim.probe", "params": {}}
reply:    <one probe object as above>        or   {"error": "message"}
```

`--method` changes the method name. An optional second method (`--stats-method`, default `anim.stats`) may return
the probe's own cost numbers (for example microseconds per actor); `collect` prints them. If the probe writes
JSONL itself, skip all of this and run `analyze` on the files.

## Keep the probe harmless

- Read only. The probe must not change the game, send input or move anything.
- Run it where the game state is consistent (the main or game thread, once per frame or on a timer).
- Guard every read of engine data, so a wrong offset or a freed object gives a missing field and not a crash.
- Measure its cost and keep it small, so sampling does not change the frame time you are checking.
