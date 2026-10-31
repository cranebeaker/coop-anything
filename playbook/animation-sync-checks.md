# Checking animation sync

A co-op mod can have perfect positions and still look broken: the friend slides across the floor on an idle
animation, stands in a T-pose, holds the wrong gun, aims at the sky while the owner aims at you, or floats a
meter above the ground. Players notice this before they notice a desync in quest state. This page is how to
catch it with a script, with no human watching, in any game.

The tool is [tools/animcheck](../tools/animcheck/README.md). It reads probe streams (JSON lines written by a
small piece of code in the game), runs checks, and prints PASS or FAIL with an HTML report. The stream format is
in [FORMAT.md](../tools/animcheck/FORMAT.md).

## What "the avatar looks right" means, as numbers

| Question | Counter |
|---|---|
| Does the puppet strike the same pose as the real player? | `avatar.pose_err_p95` (world units, after aligning the root bone and facing) |
| Does it play the same animations? | `avatar.anim_mismatches`, `avatar.group_mismatch_pct` |
| Same weapon, same aim pitch, same facing? | `avatar.wrong_weapon`, `avatar.wrong_aim`, `avatar.wrong_facing` |
| Same stance (sneak, swim, knocked down, aiming)? | `avatar.stance_mismatch` |
| Does it behave on its own (no slide, T-pose, float, jitter, rubber band)? | `avatar.glitches` and `avatar.glitch.<kind>` |
| Is it in the right place, allowing for the interpolation delay? | `avatar.pos_err_p95` |

The same numbers without the `avatar.` prefix cover shared NPCs, so one run checks players and NPCs and keeps
them apart.

## Steps

1. **Write a probe.** Code in the game, read only, that once per frame (or at 10 to 30 Hz) writes one JSON
   line: for each actor near the player, its id, position, rotation, playing animations, bone positions in
   world space, weapon and stance. Follow [FORMAT.md](../tools/animcheck/FORMAT.md). Read the full third-person
   skeleton even in first person. Guard every raw read of engine memory so a wrong offset gives a missing
   field and not a crash. The New Vegas probe in the fnv-coop repo (`plugin/animprobe`,
   https://github.com/cranebeaker/fnv-coop) is an example. Other engines do the same through their scripting API
   or a native plugin.
2. **Write a profile.** A small JSON file with the root and arm bone names and the game's animation group table
   with a category for each group (idle, walk, run, jump, death, attack ...). Copy
   [examples/fnv/profile.json](../tools/animcheck/examples/fnv/profile.json). Run
   `python -I tools/animcheck/cli.py profile your.json`. Dump the group table from the game's own data or from a
   modding tool; do not guess it. Names only go in the profile, never game files.
3. **Record a clean run on one machine.** A bot (see [automated-qa.md](automated-qa.md)) walks, runs, turns,
   jumps, draws and holsters a weapon, reloads, swims if the game has water. Then
   `cli.py analyze run.probe.jsonl --profile your.json --html report.html`.
   Every finding in a clean run is a profile problem or a threshold problem, not a mod bug. Fix the
   profile (the report lists the animation groups it does not know) or change a value in the profile's
   `thresholds`. Scripted teleports go to `--suppress`.
4. **Record the two-machine run.** Host and client in the same session on one PC, the same bot script on the
   host's player, and the client's puppet of that player on screen. Write one probe file per machine, or
   poll both with `cli.py collect` so the streams share one clock. Then:

   ```
   cli.py analyze host.probe.jsonl client.probe.jsonl --profile your.json \
       --avatar "player=host:14>client:FF000A" --html report.html \
       --assert "avatar.pose_err_p95<=12" --assert "avatar.glitches==0" --assert "glitches==0"
   ```

   The avatar argument names the real player's id on the owner machine and the puppet's id on the observer.
5. **Make it a test.** Put the command in the suite next to the other co-op tests, with the assertions as the
   pass condition, and run it after every change to the avatar or animation code. The exit code is 0 for PASS.
   A scripted bot scenario that exercises one animation family each (locomotion, weapon draw, aim, reload,
   death, swim) gives findings that point at one cause.
6. **Test the checker, not only the game.** The tool ships with mutation twins: each detector has a copy with
   one deliberate bug, and its tests must fail on it. If you add a check or change a threshold, add a twin the
   same way, so a check that silently stops firing is caught (`python -I tools/animcheck/run_tests.py`).

## Reading a finding

The tool says what happened and when, not why. These are the first causes worth checking; confirm each one in
your game's code or with a second probe run.

| Finding | Likely cause |
|---|---|
| `tpose` with "no sequence running" | The puppet's animation graph is not driven, or the skeleton was swapped after the animations were set up. Check the order of model load and animation start. |
| `tpose` with "arms straight out" | Bind pose: animations stopped or never blended in. Common right after a respawn, a model change or a cell change. |
| `sliding` with no locomotion animation | The puppet moves by network position only. Feed the movement state (speed, direction, stance) to its animation, not only the position. |
| `sliding`, "walk animation but 700 u/s" | The animation chosen from a stale speed, or the position jumps faster than the interpolation. Look at the speed the animation is fed. |
| `floating` or `sinking` | The puppet's height is the owner's height without a ground or navmesh snap, or a stance (swim, crouch) changes the body offset. |
| `jitter` | Two writers move the same body, for example the game's own movement and your position sync. One of them must stop. |
| `rubberband` "snapped back" | Position correction fights the owner. Check for a late or out-of-order packet that is applied after a newer one. |
| `group_mismatch` | The observer plays a different animation than the owner: a missed event (draw, reload, hit reaction) or an animation chosen locally instead of replicated. |
| `wrong_weapon` | Equip state not replicated, or replicated after the animation that needs it. |
| `wrong_aim` | The aim pitch is not sent, or the aim bone is overwritten by the animation. |
| `deathpose` | The death event reached the body but the animation graph kept an alive state, or the other way around. |
| `stuck` | A bot or an AI order was not replayed on the observer. |

## Pitfalls

- **First-person body.** Many games show only arms in first person. A probe that reads that model reports a T-pose
  and a huge pose error for the local player. Read the third-person skeleton.
- **Different bone sets on the two machines.** The pose error only uses bones both sides report. Send the same
  names everywhere.
- **Unknown animation groups.** A group missing from the profile is `unknown`, and `sliding` fires on it. The
  report prints the unknown groups; add them.
- **Ids.** A puppet has a different id than the real player. Without `--avatar` the checker compares nothing,
  and `compared` is 0 in the counters. Look at `avatar.compared` and `compared`: a PASS with 0 comparisons is not a PASS.
- **Clocks.** Two processes have two clocks. Collect on one PC with `collect` (it stamps a shared clock), or pass
  `--auto-offset`.
- **The interpolation delay.** The checker expects the client to show state about 100 ms old and searches 150 ms
  either side. If your mod buffers more, set `--delay-ms`; do not raise the pose tolerance to hide it.
- **Probe cost.** Sampling 16 actors with 16 bones should not change the frame time you are checking. Measure it.
- **World scale.** Thresholds assume about 70 units per meter. Set `units_per_meter` in the profile.
- **Your own corrections.** A loop of one animation on the puppet can come from the mod, not the game. A
  teleport that snaps the puppet back to the sender may run the game's full teleport, which also resets the
  animation; the restarted animation can step the puppet off again and trigger the next snap. Count the
  loop's restarts next to the mod's own corrections over the same window, and run it once with the
  corrections off. In Shadow of War each put-back restarted the stand-in's graph, which then sheathed its
  sword, and the counts matched exactly ([games/shadow-of-war/NOTES.md](../games/shadow-of-war/NOTES.md),
  session bd).

## Where it fits

This is the "does it look right" layer on top of the state checks in [automated-qa.md](automated-qa.md): those
prove that quest, inventory and position state match; this proves the body that shows that state is posed, animated
and armed the same way. Run both in the same bot scenario.
