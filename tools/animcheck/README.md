# animcheck: animation and pose checker

Checks that a co-op mod shows other players (and shared NPCs) the way they really are: same animation, same
pose, same weapon, same facing, no sliding, no T-pose, no floating. It works for **any game**. It never touches the
game: it reads JSON-lines streams that a small probe inside the game wrote, runs checks on them, and prints PASS or FAIL
plus an HTML report. Python 3, standard library only (the tests were run with Python 3.11).

What you provide per game:

1. a **probe** in the game that writes the stream (format: [FORMAT.md](FORMAT.md)),
2. a **profile**, a small JSON file with the game's bone names and animation group table
   (example: [examples/fnv/profile.json](examples/fnv/profile.json)).

The how-to page for agents is [playbook/animation-sync-checks.md](../../playbook/animation-sync-checks.md).

## Try it in one minute

```
python -I tools/animcheck/cli.py demo --out out/animdemo
```

This writes two synthetic streams (a host and a client that shows the host player's puppet with planted
defects), analyzes them, prints the findings and writes `out/animdemo/report.html`. It exits with 0; the demo
analysis itself reports FAIL on purpose.

## What it checks

**On one machine** (each actor's own stream, also on the real player and on a puppet):

| Finding | Meaning |
|---|---|
| `sliding` | Moves with no locomotion animation, or faster or slower than the playing walk or run animation allows. |
| `tpose` | No animation playing at all, or both arms straight out (bind pose). |
| `floating`, `sinking` | Height away from the tracked ground, with no jump, fall, swim, knock-down or ragdoll to explain it. |
| `jitter` | Position flips back and forth from sample to sample. |
| `rubberband` | A sudden jump in position, noted when it snaps back. |
| `stuck` | Should be walking (you say when) but stands on an idle animation. |
| `deathpose` | Alive animation on a dead actor, death animation on a live one, or no death animation. |

**Between machines** (owner against observer, with the receiver's interpolation delay taken into account):

| Counter | Meaning |
|---|---|
| `pose_err_p50/p95/max` | Mean distance between matching bones after root alignment, in world units. |
| `pos_err_p95`, `facing_err_p95_deg` | Position and facing error. |
| `group_mismatch_pct`, `group_mismatch_episodes` | The observer plays different animations than the owner. |
| `wrong_weapon`, `wrong_aim`, `wrong_facing`, `stance_mismatch`, `dead_mismatch` | Episodes longer than 400 ms where that state differs. Shorter ones are transitions and are not counted. |

Avatars (a player's own character against the same player's puppet on another machine) get the same numbers under
`avatar.*`, apart from the NPC numbers.

## The probe stream, in short

JSONL, one probe per line, one file per machine. Each probe has a time and a list of actors. Each actor has an id,
position, rotation (Z up), and optionally the playing animations, world-space bone positions, weapon and stance.

```json
{"machine":"host","t_ms":1250.0,"actors":[{"ref":"0x14","pos":[1000,2000,100],"rot":[0.2,0,1.57],
  "anim":{"seqs":[{"slot":0,"group":1,"state":1}]},"bones":{"pelvis":[1000,2000,140],"hand_l":[1000,1988,146]}}]}
```

The full field list, what each check reads, and how to handle several machines are in [FORMAT.md](FORMAT.md).
The Fallout: New Vegas probe is in the fnv-coop repo, under `plugin/animprobe`
(https://github.com/cranebeaker/fnv-coop). It reads engine memory, so it stays there; it is the example of a
probe, and its output follows this format.

## The profile

A profile JSON tells animcheck the game's names. Every key is optional.

```json
{
  "name": "mygame",
  "bones": {"root": "Hips", "left_upper_arm": "UpperArm_L", "left_hand": "Hand_L",
            "right_upper_arm": "UpperArm_R", "right_hand": "Hand_R"},
  "groups": ["Idle", "Walk", "Run", "Jump", "Death"],
  "categories": {"idle": ["Idle"], "walk": ["Walk"], "run": ["Run"], "jump": ["Jump"], "death": ["Death"]},
  "category_prefixes": {"attack": ["Attack"], "reload": ["Reload"]},
  "default_category": "misc",
  "angle_unit": "rad",
  "units_per_meter": 70.0,
  "move_flag_bits": {"sneak": 9, "swim": 11},
  "thresholds": {"slide_min_speed": 50.0}
}
```

- `bones`: the node names for the roles the analyzer uses: `root` (pelvis or hips) and the four arm bones.
- `groups`: the animation group table. A list (position = id) or an object `{"5": "Walk"}`. If your probe sends
  string names as `group`, you can leave `groups` out and list the names in `categories`.
- `categories` and `category_prefixes`: which group belongs to which category. The categories are `idle`,
  `walk`, `run`, `dodge`, `turn`, `aim`, `attack`, `reload`, `jam`, `equip`, `hit`, `death`, `jump`, `block`,
  `misc`. Entries are group names or ids; prefixes match the start of a name. Locomotion checks use `walk`,
  `run` and `dodge`, the airborne check uses `jump`, the death checks use `death`.
- A group the profile does not know is `unknown`, and the report lists it ("groups the profile does not know").
  Fill the table until that list is empty, otherwise `sliding` can fire on animations the profile never heard of.
- `angle_unit`: `rad` or `deg`, for `rot`. `units_per_meter`: all distance thresholds are written for 70 units
  per meter and are scaled by `units_per_meter / 70`. `thresholds`: override any value of `config.Thresholds`.
- `move_flag_bits`: optional, bit numbers inside the probe's `move_flags` integer that mean sneaking and
  swimming. Probes that send `stance.sneaking` and `stance.swimming` do not need it.

Check a profile with `python -I tools/animcheck/cli.py profile my.json`.

The Fallout: New Vegas mapping lives in [examples/fnv/](examples/fnv/): its 245 animation groups and the
bone names. Without `--profile`, a small generic profile is used (bone names such as `pelvis`, `hand_l`).

## Command line

```
python -I tools/animcheck/cli.py analyze host.probe.jsonl client.probe.jsonl --profile my.json \
    --avatar "player=host:14>client:FF000A" --html report.html --json report.json \
    --assert "avatar.pose_err_p95<=12" --assert "glitches==0"
```

- Files: `name=path` or just a path; the first file is the owner. Add `--owner NAME` to choose another.
- `--avatar NAME=OWNERMACHINE:OWNERREF>OBSERVERMACHINE:OBSERVERREF`, refs in hex. Repeat for several players.
- `--assert "<counter><op><number>"`, ops `<= >= < > == !=`. A counter that does not exist is a failure, so a typo
  cannot pass. Without `--assert`, the defaults apply: `glitches==0`, `wrong_weapon==0`, `wrong_facing==0`, and
  the pose error bound (`--pose-tol`, 12) when there is a pair to compare; with avatars also `avatar.*` equivalents.
- `--expect-moving REF:T0-T1` says an actor must be walking between two times (ms, in the stream's clock);
  it turns the `stuck` check on for that actor.
- `--suppress KIND:REF:T0-T1` ignores matching findings, for example a deliberate teleport (`*` for any kind or id).
- `--delay-ms`, `--latency-ms`: the receiver's interpolation buffer (100) and the extra latency window (150).
- `--auto-offset`: estimate the clock difference between streams that use different clocks.
- Exit code 0 = PASS, 1 = FAIL, 2 = bad arguments.

Collect from running games over TCP (see FORMAT.md, "Collector protocol"), then analyze:

```
python -I tools/animcheck/cli.py collect --connect host=127.0.0.1:9100 --connect client=127.0.0.1:9101 \
    --seconds 10 --hz 20 --out out/run1
```

From Python:

```python
from animcheck.analyze import analyze
from animcheck.counters import check
from animcheck.model import load_stream
from animcheck.profile import Profile

p = Profile.load("my.json")
streams = {"host": load_stream("host.probe.jsonl", profile=p), "client": load_stream("client.probe.jsonl", profile=p)}
report = analyze(streams, p.thresholds(), profile=p,
                 avatars=[{"name": "player", "owner": "host", "owner_ref": 0x14, "observer": "client", "observer_ref": 0xFF000A}])
assert not check(report, {"avatar.pose_err_p95": "<=12", "glitches": "==0"})
```

Add `tools/` to `sys.path` first so `animcheck` imports.

## HTML report

`--html report.html` writes one self-contained file (no scripts, no external files, light and dark): the PASS or
FAIL line with the failed assertions, all counters, the groups the profile does not know, a table per machine
pair, the avatar table, and every finding with machine, kind, actor, start and end time and a one-line reason.
`--json` writes the same data as `animcheck.report.v1`, with a flat `counters` object for scripts.

## Tuning

Thresholds are in [config.py](config.py) in world units (70 per meter) and milliseconds. They are starting
values. Record a clean run on one machine first (a bot that walks, runs, turns, jumps and draws a weapon), run
`analyze` on it, and fix whatever it reports: either the profile (an unknown group, a wrong bone name) or a
threshold in the profile's `thresholds`. Findings you expect, such as a scripted teleport, go to `--suppress`.
Only then record the two-machine run.

## How it is tested

One command from the repository root:

```
python -I tools/animcheck/run_tests.py
```

It prints `ALL OK` as its last line only when both of these hold:

1. the unit suite passes on the real code (synthetic probe streams only; no game, no network beyond loopback);
2. every **mutation twin** is caught. A twin is the same code with one deliberate bug, switched on with the
   `ANIMCHECK_MUTANT` environment variable (one per detector, one per comparison, one for the assertion checker,
   one for the profile bone names). The suite must fail on each twin, and must fail on the specific test named
   for it in `run_tests.py`, not on an unrelated one.

The synthetic streams come from [synth.py](synth.py): scripted walkers and idlers with injected
glitches (sliding, T-pose, floating, jitter, rubber banding, death pose) and a delayed copy that plays the client.
The tests cover every detector clean and defective, the cross-machine counters, avatars, profiles (the New
Vegas example and a name-only game), the command line, the HTML and JSON output, and the collector against a fake
bridge and against a TCP server on loopback.

The tests use synthetic streams only. Calibrating against a real game is a per-game step: collect a clean run and
follow "Tuning".

## Files

| File | What |
|---|---|
| `cli.py` | The command line: `analyze`, `collect`, `demo`, `profile`, `selftest`. |
| `profile.py`, `config.py` | Game profile and thresholds. |
| `model.py` | Stream and actor model, JSONL reading and writing. |
| `detectors.py` | The single-machine detectors. |
| `compare.py` | Owner against observer comparison. |
| `analyze.py`, `counters.py`, `report_html.py` | Report, assertions and HTML. |
| `collect.py` | Poll running games and save streams. |
| `synth.py` | Synthetic streams for tests and the demo. |
| `run_tests.py`, `tests/` | The test command, the suite and the twins. |
| `examples/fnv/` | Fallout: New Vegas profile. |

Ported from fnv-coop commit f57aa78. The game-side probe, the static engine checks and the trace exporter of that
project were left behind because they depend on that game's engine.
