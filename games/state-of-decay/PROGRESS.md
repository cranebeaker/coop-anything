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
| Her melee hits and kills zombies; her kills counted for her, not the host | Works on fresh boots before the final crash fix; **re-check on the final build pending** |
| Host player's own weapon kills zombies, counted for him | Same as above, **re-check pending** |
| Zombies attack the host player | Seen during tests; **explicit health-drop check pending** (`local/goal_check.py`) |
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
`zm_dupes.py`, `use_test.py`, `door_pose.py`, `use_conds.py`, `cond_snap.py`.

## Next steps

1. With the display on: fresh boot, run `python local/goal_check.py` and record its four results here
   (her kills, his weapon kills, zombies hurting him, zombies hurting her far from him).
2. World interaction for the joining player: register her with the interaction system (`local/interact.py`)
   and find how UL creates a real survivor record for a recruit (borrowing records is unstable).
3. Commit/PR only when the user asks.

## Findings log

Newest first. One entry per discovery; link the NOTES section for details.

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
