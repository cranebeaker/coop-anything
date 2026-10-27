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
| Host stability (hangs / silent deaths) | **Fixed**: minimap name-table race (2026-10-24) and a per-call buffer leak in our tooling (2026-10-26) |
| Looting / doors / searching for the joining player | Doors work on her machine: state, door and player animations, collision (`maya_record.py`, `door_sync.py`); searching untested |
| Missions, community, journal data for the joining player | Not started (she now has a community record on the host; nothing mirrored to her machine) |

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
`zcomb_conds.py`, `reaction_ab.py`, `contact_ab.py`.

## Next steps

1. Optional: more zombies around her when the host's player is far (population pass density, `zm_focus`), so
   far fights are as dense as near ones. Zombies near her spend much time in `Reaction.Basic` (not from her
   attacks; cause unknown).
2. World interaction for the joining player: her record and door use on the host work (`local/maya_record.py`);
   next: a host -> client door relay (pool entities, see log), mirror her interaction states to her machine, then
   searching containers. Run `maya_record.py` after `coop_setup.py` (not yet part of it).
3. Commit/PR only when the user asks.

## Findings log

Newest first. One entry per discovery; link the NOTES section for details.

### 2026-10-27 - door gaps closed: host collision, stuck pairing, Marcus turned into Maya
- Host collision for doors she closes out of his camera's view (the gap in the previous entry): the host never
  computes the closing pose, so the door's static physics stayed where the open leaf was. `settle_stalled` now,
  for a stalled clip that ends closed, reloads the door model (default pose = closed) and physicalizes it static
  before the jump-to-end replay; the game's finalizer then sets state and part flags as usual. Four out-of-view
  toggles: closed 9/9 rays on both machines, open 0/9 on both; the host player's in-view use of the same door still
  works. Not fixed by `ENTITY_UPDATE_ALWAYS` + `Activate` + `ForceCharacterUpdate` together either.
- Doors stuck in "clip playing" (0x4) from before the hook (seen on a fresh boot: 0xC) are settled too, using the
  clip of the state they are heading for.
- Stuck door pairing: a player whose state machine left a door state without DoorRelease (action 55: 0x1440960(actor,
  door) + blackboard +0x14E0 = 2) stays in the pair list 0x1E7E754 and never gets a door context again (her
  host copy stuck in Locked.Loop, then reset). `release_stuck` releases players paired but outside Interact
  states for 1.5 s.
- Bug: `maya_record.py` used the re-skin function 0x15472D0, which also re-skins the current player: the host's
  player had been showing Maya's model (both machines) since the record was introduced. It now writes the
  definition index (+0xF0) directly; models restored with `coop_setup.load_model`.
- The Radio Assistance menu that blocked her input yesterday could not be reproduced (held "use" 0.3-2 s, no
  target); unexplained. Test harness: `door_walk.py` now forces focus (ALT tap) and starts 0.15 m above the floor;
  `door_full.py [rounds] [door]` (pool names change between boots; Door_5 was locked in this save).
- Fresh-boot `door_full.py 2 Door_0`: her states mirrored, door state/seq equal, collision equal, walk blocked
  closed (0.49 m short) / through when open.

### 2026-10-26 - doors fully on the joining player's machine: animations, collision, state
- Initial state: door spots keep their state in a flags byte (`[0x1E7E770][spot]`: 0x8 open, 0x4 clip playing,
  0x2 other side, 0x1 locked); placed doors: count 0x1E7E760, spot ids `[0x1E7E758]`, positions `[0x1E7E74C]`;
  door types `[0x1E7E788]` (8 x 0x34: clip names, model at +0x28). Doors placed before the hook get the clip of
  their current state (`door_sync.state_clip`), so every door shows open/closed correctly from the start.
- Stalled doors: a door clip only advances while the host's camera renders the door. A door she used out of his
  view stayed at flag 0x4 with clip time 0 forever, and then no actor could use it again (the door search accepts
  flags & 0xC in {0, 8} only). `door_sync.settle_stalled` replays a clip that hasn't moved for 1.2 s with
  jump-to-end; the game's finalizer 0x143FE20 then sets the state bits and collision (0x143FB50). Not fixed by
  `ENTITY_UPDATE_ALWAYS`, `ForceCharacterUpdate` or `ca_ForceUpdateSkeletons` (cvar block 0x19627F8 +0xD8).
- Collision on her machine: the client handler physicalizes closed doors (`Physicalize(0, PE_STATIC)`, 2 s after a
  normal closing clip / 0.15 s after a jump, so the closed pose is used) and removes physics from open ones.
  Ray probe through the doorway (`local/door_ray.py`): closed 9/9 hits on both machines, open 0/9. Walking with
  real input (`local/door_walk.py --flip`): open -> she walks through (5.4 m past), closed -> stopped 0.47 m short.
- Players' door animations on her screen: the client has no door context, so the host mirrors each player's
  animated `MachineDriven.Interact.Door.*` states (not the logic states) through RMI `ClCoopPeerState` -> cvar
  `coop_peer_state` -> a stub chained in front of the client's pass-1 detour that writes the state index into her
  record or the client's copy of the host's player. Verified: her client went `OpenAway` / `CloseFar` /
  `CloseClose` with the host, and the client's copy of Marcus `OpenStart -> OpenAway -> idle`.
- Latency: the host's per-client send budget (`sv_bandwidth`, default 30000 bit/s) was full of co-op RMIs, so
  door changes reached the client ~3.4 s late; `coop_setup` sets 300000 -> 0.4-0.7 s.
- Test-harness notes: the client's "use" key with no client-side target opened the Radio Assistance menu
  (blocks input; Escape closes it). Teleports to exact floor height can leave her stuck (lift 0.15 m).
- Known gap: on the host, a door she operated out of his camera's view is settled by the jump-to-end replay, but
  its host-side collision does not match the closed pose (rays pass; the host never rendered the pose). Her
  machine is correct. Affects host zombies and the host player at doors he never looked at.

### 2026-10-26 - doors on the joining player's screen (door relay)
- The door system plays door clips through 0x143F0A0 (thiscall door entity; clip name such as
  `door_door_handle_left_open_away` / `..._right_close_close` / `..._close_far`; flag = jump to the last frame,
  used when a spot is set up). Clips are started by id (skeleton anim vt+0x28) with "repeat last key", so the last
  frame is the door's open/closed pose. Door models are animated `.cga` characters (slot 0: `animated_door`,
  `anmated_door_glass`, `animated_door_firedoor`, ...; path at character +0xB8). Lua: the script table's
  `__this` is the IEntity pointer.
- `local/door_sync.py` + new RMI `SinglePlayer.Client:ClCoopDoors` (loose `singleplayer.lua`, both installs):
  the host hooks 0x143F0A0 (ring of entity, flag, clip); `hud_relay.py` sends the placed doors every 0.5-3 s
  (`index,pos,angles,model,clip,flag,seq`, split into parts). The client handler loads the model on the same
  `Door_N`, places it and plays a new clip when the sequence changes (jump-to-end clips at speed 100). Doors the
  host no longer places go back under the map.
- Verified on a fresh boot: the client's door pool matches the host's (9 doors, same positions and angles).
  Marcus opened the restroom door, and it showed open on her screen. She pressed the interact key at it: her
  host copy ran `Interact.Door.Front.CloseClose`, the client handler applied the close clip (seq 1 -> 2), and
  her screenshot shows it closed. `local/door_check.py` prints the host clip/seq and the client's applied seq
  for the door at `door_pose.txt`.
- Not yet: her own door animation on her screen (her client state machine stays idle; the door interaction runs
  on her host copy), and collision for closed doors on the client (the client door has no physics). Doors set up
  before the hook was installed show their default pose until their next clip.
- `coop_setup.py` now runs `maya_record.install` (her survivor record + door-system registration).

### 2026-10-26 - doors are a host-side entity pool; the client has none
- `Door_N` entities (class `ULDoor`, native, empty script table) are a pool that UL's door system assigns to door
  spots near the player: the same name moves between doors (Door_2 was at 3100,3323, later at 3175,3366); unused
  ones sit at 0,0. On the host the assigned ones have one geometry slot and their open/closed angle; on the
  client every `Door_N` is at 0,0 with no slots (no UL world tick there). So her screen shows no door state at
  all, for either player's doors. Mirroring needs a door relay like the zombie one: for each assigned host door,
  position, angle and geometry (slot 0 static object), applied to the client's pool entities.
- Unlocked doors found for tests: Door_5 at (3173,3349) at the time of the test (the host player opened it:
  `Interact.Door.Back.OpenStart -> OpenAway`). Door_1 at (3102,3339) is locked. Pool names change, so look doors
  up by position.

### 2026-10-26 - a real survivor record for her; her host copy uses doors
- Record creation path found: console command `com_addMember` ("Add a new character to your community",
  handler 0x14B6BF0) takes the first free record of the character table ([0x1EA83B8], 100 x 0x178; free =
  flags +0x15C bit 0 clear, handle +0x48 = 0x0FFF8000), initialises it (0x15473C0 random character; with a name
  argument 0x1547580 with a data id), adds it to the community (flags 0x29) and spawns its NPC body `ULNPCnn`.
  The name argument is a `Table.Entry` data-id name, not a character name; character definitions are not in
  that registry, so re-skinning is used instead: 0x15472D0 (thiscall record, definition index).
- `local/maya_record.py`: com_addMember -> re-skin to definition 145 (Maya Torres) -> record +0x58 = her entity id
  -> hide/deactivate/move the spawned body (otherwise it floods the log in her place) -> register her with the
  door system (`interact.register`). Unlike the earlier borrowed "known" record, her player state machine stays
  in UL's extra list, and the "Entity <her id> is not part of the community" flood stops (all three log sites in
  0x1716740/0x17167F0/0x1716880 go to 0; they find records by iterating active records and comparing +0x58).
- Result (fresh boot): at the door pose the host player gets (`door_pose.py`), her host copy now gets the door
  context (blackboard +0x14D8 = 383) and runs `Interact.Door.Front.ContextTest -> OpenStart -> Locked.Start ->
  Locked.Loop` on the host (Door_1 is locked) when she presses the interact key on her machine
  (`local/door_use.py client`). Her own screen does not show it yet: the client runs no door system, so her
  states and the door state must be mirrored from the host.
- In a long session the same setup gave no context: the door routine (0x1442B60, per-actor loop over 0x1E7EBB0)
  visited her entry but her distance test to every door candidate failed. Not explained; it worked again after a
  reboot.
- Tooling: `Proc.count_exec(..., when=(esp_offset, value))` counts only hits where a stack slot (e.g. a loop
  index) matches. Fixed a crash in `count_exec`/`watch_writes`: when the exception handler resumes a thread,
  Windows restores the debug registers from the exception context, which could re-arm a breakpoint after it was
  cleared. The handler would then be removed and the next hit was an unhandled breakpoint exception that killed
  the host (dump 0x80000004 at the breakpoint address). The handler now clears Dr7 itself while disarming.

### 2026-10-26 - host out-of-memory after ~7 h idle: our own leak, fixed
- The host died after sitting idle for about 7 hours with "Memory allocation for 54884 bytes failed" (game log;
  dump save failed for the same reason). Cause: `sodlib.Proc.lua()` / `console()` / `run_code()` allocated a
  buffer in the game per call and never freed it. Each `VirtualAllocEx` reserves at least 64 KB of the 32-bit
  process's address space, and `hud_relay` calls `lua()` every 0.5 s (about 7.5 MB/min). This probably also
  explains the older "long sessions degrade" observations.
- Fix: `Proc.free()` and the per-call buffers are released after the call (kept only on a timeout, when the game
  may still use them). `local/vm_watch.py` (address space per process): after the fix, flat at ~636 allocations
  / 1.3 GB on the host over 5 minutes with the relay running.
- Side note: the host log grows by gigabytes per day from "Entity <her id> is not part of the community"
  (every frame); it goes away once she has a survivor record.

### 2026-10-26 - test-harness side effects in zombie fights (not co-op bugs)
- `goal_check.stand_up()` (forcing her host state machine to idle while she is knocked down) leaves the zombies
  believing she is still down: they cycle `Combat.*.Strike.TargetDown.*` and walk loops without moving, and her
  health does not change. Moving her a few metres frees them (they then wander off). Prefer a fresh group and
  `SetHealth` only (`local/reaction_ab.py`, `local/contact_ab.py`). `stand_up` stays for the fatal-landing case
  only.
- `SetHealth(100)` reads back 90 / 80 after heavy damage: max health drops with injuries (game rule), not a
  sync problem.
- Zombies teleported onto her show `Reaction.Basic` (light hit reactions) for a while; around the host's player
  they never do. In steady state (15 s later) there were 0 reaction samples and `peer_melee` fired 0 times, so it
  is the arrival push of her teleport, not phantom hits. zombie_targets' injected "Hero" contact makes no
  difference (A/B, radius 1.4 m vs 0).
- Fresh-boot baseline (`reaction_ab.py`): the host's player among a group goes 90 -> 1 in 15 s; she goes
  100 -> 74 with him 20 m away. The zombie spawner around the test sites runs dry after ~30 min of tests (reboot).

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
- (Resolved in the newer entry above: a test artifact.) With the host's player 300 m away, zombies targeting her walk up and stop 2-4 m away in Basic.Move/Taunt
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
