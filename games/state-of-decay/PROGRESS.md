# State of Decay (2013) co-op: progress and handoff

Single source of truth for the current state of the co-op work. Read this first, then the relevant parts of
[NOTES.md](NOTES.md) (full research log, by phase) and [README.md](README.md) (public summary).

**Rule for every agent:** after each discovery, fix or verified result, add a dated entry at the top of the
[Findings log](#findings-log) and update [Current state](#current-state) if it changed. Keep it free of private
data (no machine names, user folders, IP addresses, account names).

## Current state

Last updated: 2026-10-26.

Two copies of the game run side by side (host = "Marcus", joining player = "Maya"). Everything is applied at
runtime by Python scripts under the gitignored `local/` folder; no game file is modified except the loose
`Game/scripts/gamerules/singleplayer.lua` (extra RMIs) in both test copies.

| Area | State |
|---|---|
| Join, walking, camera, HUD, minimap, mission banner on the joining copy | Works |
| Attacks, dodges, jumps replicate both ways | Works |
| Zombies visible on both machines, clothed, animated on the client (no T-pose), host's hit reactions shown | Works (verified 2026-10-24) |
| Zombies target, chase and damage her, also with him 300-450 m away | Works (soaks 2026-10-24) |
| Her melee hits and kills zombies; her kills counted for her, not the host | Works (final build 2026-10-26: her +1, host +0) |
| Host player's own weapon kills zombies, counted for him | Works (final build 2026-10-26: his +4, hers +0) |
| Zombies attack the host player | Works (final build 2026-10-26: 90 -> 1 in 15 s) |
| Zombies attack her with the host's player near (20 m) | Works (final build 2026-10-26: 100 -> 64 in 9 s) |
| Zombies attack her with the host's player far (300 m) | Works, weaker (fresh boot 2026-10-26: 100 -> 86 / 72 in 15 s vs 100 -> 17 near; fewer zombies around her) |
| Weapons: her starter hammer, cycling from community storage (middle mouse), host's journal equip | Works |
| Host stability (hangs / silent deaths) | **Root cause found and fixed** (minimap name-table race, see log); two 40-min soaks clean |
| Looting / doors / searching for the joining player | Not working yet (findings in NOTES "World interaction") |
| Missions, community, journal data for the joining player | Not started (she has no survivor record) |

## How to run a session

1. Never start or modify the Steam install. Test copies live in `local/test-install` (host) and
   `local/client-install` (client); both gitignored.
2. Keep the PC awake and the display on: `python local/keep_awake.py 4`. With the display off a new game
   process finds no Direct3D device ("Could not find any compatible Direct3D devices"); waking from Modern
   Standby crashes both renderers. A locked screen also blocks booting.
3. `local/restart_session.sh` (kills only the test copies, boots host + client, OKs the "Unsupported video card"
   dialog via `local/dismiss_gpu.ps1`).
4. `python local/coop_setup.py` installs everything on both machines and starts `local/hud_relay.py`
   (must keep running on the host).
5. Optional diagnostics: `Proc.trap_faults()` on both processes (in-process crash catcher), exit watchers
   (`local/exit_watch.py host|client`).

Use a real Python 3 (the Windows Store stub does not work); `capstone` and `pillow` are needed by the tools.

## Co-op layer (local/, installed by coop_setup.py)

| Module | What it does |
|---|---|
| `sodlib.py` | Process access, mailbox calls on the game's main thread (PeekMessage hook), hooks, Lua/console exec, hardware breakpoints (`count_exec`, `watch_writes`), `trap_exits`, `trap_faults` |
| `prep_client.py`, `client_tick.py` | Joining copy: UI gate, front-end/HUD update replacement |
| `player_sm.py`, `peer_input.py` | UL state machines for both players on both machines; input sent as chat messages (`coop:`/`cooh:`) |
| `peer_melee.py` | Her hits applied to zombies on the host (CombatDamage with her params) |
| `zombie_targets.py` | Zombies pick her as a target; contact/strike on her |
| `zm_focus.py` | Zombie manager: focus point for her, cull uses the nearer player, population pass around her (<=300 m) |
| `kill_counts.py` | Her kills counted for her (chat `kilh:`); HUD banner off (`SHOW_BANNERS`) |
| `weapon_relay.py` | Melee weapon hash relay both ways, starter hammer, cycling |
| `pause_sync.py`, `remote_fx.py`, `turn_clamp.py`, `sync_models.py` | Shared pause, no host screen FX on client, turning, NPC models |
| `hud_relay.py` + `zombie_sync.py` | Host -> client over RMIs: banner, minimap icons, zombie bodies + animations + visibility (matched by position), split into <=400-char parts |
| `crash_guards.py` | Null guards, NaN-position guard, **radar_cache** (stability fix) |

Main test tools: `zm_soak.py` (long stress cycles), `soak_ab.sh`, `zm_freeze_test.py`, `kill_verify.py`,
`goal_check.py`, `path_hit.py`, `zombie_spawn.py`, `dbg_watch.py`, `exit_watch.py`, `anim_twice.py`,
`zm_dupes.py`, `use_test.py`, `door_pose.py`, `use_conds.py`, `cond_snap.py`, `far_strikes.py`, `anim_rate.py`,
`zcomb_conds.py`.

## Next steps

1. Optional: more zombies around her when the host's player is far (population pass density, `zm_focus`), so
   far fights are as dense as near ones. Zombies near her spend much time in `Reaction.Basic` (not from her
   attacks; cause unknown).
2. World interaction for the joining player: register her with the interaction system (`local/interact.py`)
   and find how UL creates a real survivor record for a recruit (borrowing records is unstable).
3. Commit/PR only when the user asks.

## Findings log

Newest first. One entry per discovery; link the NOTES section for details.

### 2026-10-26 - far combat works; the "never attack" result was a test artifact
- Fresh boot, `local/far_strikes.py` (her among a group, host's player moved, 4 s settle, 15 s): with him 300 m
  away her health went 100 -> 86 and 100 -> 72; with him 20 m away 100 -> 17 and 94 -> 2. Far fights are weaker
  because fewer zombies stay around her (about half the zombie-samples within 4 m), not because combat is
  blocked. The damage is the game's own: zombie_targets' fallback strike hook fired 0 times in all runs.
- Teleport hitch: for 2-3 s after the host's player jumps 300 m the host streams the new area and runs at about
  8 fps, and each frame advances at most about one normal frame of game time, so every zombie's animation clock
  (UL time-in-state, blackboard +0x1340) runs at about 0.12x. Measure only after a settle delay
  (`local/anim_rate.py`). Animation-state time: actor vt+0x158 -> vt+0x16C -> state +0x28, written each frame
  at 0x81FBD4 from the animation's normalized time.
- The earlier "0 strikes in 20 s" run was in a long session where her host state machine was stuck in
  `Stunned.Basic.Down` and her health read a constant 60/80 even after `SetHealth(100)`. Reboot before judging
  combat.
- Console variables `ca_ForceUpdateSkeletons`, `g_forceFastUpdate` and `es_VisCheckForUpdate` do not change at
  runtime (`System.SetCVar` and the console both leave them at the defaults 0/0/1), so the earlier "not
  caused by" checks with them proved nothing.
- Condition snapshots: the UL flag buffer (0x1F36D60) is shared by all records. Each evaluation writes only the
  first `[0x1F36D58]` flags, and which condition sits at which index depends on the active states. Compare by
  (condition type, data), not by index (`local/zcomb_conds.py`; `cond_snap` now keeps the flag count and
  arguments).

### 2026-10-26 - final-build zombie checks, animation NaN guard, far-combat gap
- `local/goal_check.py` on fresh boots: her kills counted for her (+1, host +0), the host player's weapon kills
  counted for him (+4, hers +0), zombies take him from 90 to 1 in 15 s, and her from 100 to 64 in 9 s with him
  20 m away.
- Test-harness trap: teleports can leave a player in a fatal-landing state (client) / lying downed (host); zombie
  hits then never apply and `SetHealth` reads oddly (she read 80). `goal_check.stand_up()` resets both players'
  state machines to idle and heals them. Earlier soak "80 (from 100)" readings were this, not damage.
- New client exit: assert `!_isnan(val)` in `CAnimationGraphState::SetInput` (0x7C9F60). `crash_guards.
  anim_input_guard` (both machines) replaces non-finite values with 0.
- (Resolved in the next entry: a test artifact.) With the host's player 300 m away, zombies targeting her walk up and stop 2-4 m away in Basic.Move/Taunt
  states and never enter Combat (0 strikes in 20 s; with him at 20 m they close to ~1.5 m and hit her). Not the
  physics range (es_MaxPhysDist 2000), skeleton updates (ca_ForceUpdateSkeletons 1), g_visibilityTimeout (already
  0) or g_forceFastUpdate 1. zombie_targets' strike reach (1.8 m) is not the limit either (2.6 m: no strikes).

### 2026-10-26 - repository housekeeping
- Moved the old handoff notes and chat backup from the parent folder into the repo root (excluded from git via
  `.git/info/exclude`), deleted all logs, created this file. `restart_session.sh` now starts `dismiss_gpu.ps1`.

### 2026-10-24 - host crash root cause: minimap renderer races the name table
- `Proc.trap_faults` caught the host crash on the render thread: access violation in the engine's global
  name-table hash map (0x112700E), reached from the minimap renderer 0x1380B00 asking the UI manager for
  "Class3HUDRadar" every frame (0x1380D7F). The table is not thread-safe; the main thread changes it constantly.
  Explains the hangs (self-linked bucket node), heap-corruption exits (0xC0000374) and dump-less crashes.
  A debugger hides it (timing).
- Fix `crash_guards.radar_cache`: element looked up once on the main thread, renderer uses the pointer. Two
  fresh-boot 40-minute soaks clean on both machines (before: crash within about a minute).
- Not the cause: relay rate/size, pooled zombie characters shared by two slots (engine returns early).

### 2026-10-23 - zombie animation on the client, position matching
- Client zombies were in T-pose: only the host runs their UL state machine. Host clips (0x820BC0 by name) are
  relayed and played on the client; entity ids/names differ between machines, so zombies are matched by
  position; client ghosts the host removed are hidden. A NaN-position assert exit on the client led to
  `crash_guards.nan_guard`.
- Joining player once showed a zombie body on her own screen: the old model relay matched by entity index.

### 2026-10-23 - zombies around her at any distance
- Cull at 0x13AD9C6 (175 m from camera) now uses the nearer player; population pass run a second time around
  her (<=300 m from camera); physics range raised (es_MaxPhysDist 2000). Spawn density is per map area.

### 2026-10-23 - test environment
- Laptop sleep/Modern Standby and display-off break or kill test sessions; run `keep_awake.py`.
- Never place 5-byte hooks inside functions (branch targets); count with `Proc.count_exec` instead.
