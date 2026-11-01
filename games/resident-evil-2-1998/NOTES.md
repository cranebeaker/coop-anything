# Resident Evil 2 (1998): working notes

Running log, newest last. Observations, derived facts, guesses and unknowns are kept apart.

## Identity (observed 2026-10-27)

- Store: Steam re-release, AppID 4249110, build id 21744144 (April 2026, Capcom + GOG).
- Played through Mulder's "Steam Enhancement Pack" (mulderland.com), which runs
  `rebirth\bio2.exe`: the 2006 Sourcenext Biohazard 2 PC v1.1.0 exe with Classic REbirth.
- `rebirth\bio2.exe`: PE32 i386, 6 131 712 bytes, PE timestamp 1157628588 (2006-09-07),
  SHA-256 `00dd7aab0ae5d5f6afef1cb4a9071030c2e8d4edd00e5a24302a70c800c3c232`.
- `rebirth\bio2.exe.Backup` (original): SHA-256 `21b2a647cbeddac1ec389be847f22a434a0264a18020589de91cb2824dcb1bf5`.
  `fc /b` shows 4 bytes differ: 0x116 (0x0F -> 0x2F, LARGE_ADDRESS_AWARE) and the PE checksum.
  The code is the original 2006 code.
- `ddraw.dll` = Classic REbirth 1.0.9.1, `dsound.dll` = Ultimate ASI Loader 9.7.1.0 (loads `*.asi`),
  `re2_DDraw.dll` + `D3DImm.dll` = dgVoodoo 2.86.5. French patch "mod_fr_RE2 V1.0.2" shows up in
  Classic REbirth's MOD SELECTION dialog.
- The Steam re-release's own exes (`english\LeonU.exe`, `french\LeonF.exe`...) are the GOG build of
  the 1998 port, a different exe. Nothing below applies to them.

## Prior work (searched 2026-10-27)

- No co-op or multiplayer mod for RE2 1998 found (web search, this directory).
- Useful sources:
  - BioRand (github.com/IntelOrca/biorand), `biorandmod/biorand.cpp`: hooks for this exact exe
    (0x4C5E13 load game, 0x4E3FA0 event_exec, 0x4EF9AA NPC init call, SCD task array).
  - biohazard-utils `src/hitscan/Re2Binder.cs`: key word 0x988604, player id 0x98E9A6.
  - MeganGrass/Resident-Evil-2-Starter-Kit `source/bio2/data.h`: PS1 structs (tagGLOBAL,
    PLAYER_WORK, ENEMY_WORK...). **The PC exe keeps the PS1 layouts** (checked offsets below).
  - biohazard-utils `Script/Opcodes/SceEmSetOpcode.cs` + `OpcodeV2.cs`: SCD opcode layout.
  - yanmingsohu/re2re: a full ddisasm of a bio2.exe, but its addresses do not match this exe
    (0x53CAC8 is data there); likely another build. Not used.

## Memory map (derived, all checked in a running game)

| What | Address | How checked |
|---|---|---|
| `G` (tagGLOBAL) | 0x9864E0, pointer to it at 0x52D7E4 | per-frame loop reads `[0x52D7E4]+0x3C2C` = pEnemy |
| `G.Player` (PLAYER_WORK) | 0x989EF0 | Life +0x156 = 200, Max_life +0x162 = 200, pos moves when the bot walks |
| `G.pEnemy[34]` | 0x98A10C; [0] = Player, [1] = partner, [2+n] = em id n | em_set handler writes `[id*4 + 0x98A114]` |
| `G.Em_move_tbl[type]` | 0x98A154 (= G+0x3C74) | NPC types 0x40..0x5B all point at 0x4EF110 |
| ppEnemy_end, pFree_work, current entity | 0x98862C, 0x988624, 0x988628 | used by the loops |
| empty entity | 0x98E544 | unused pEnemy slots point here |
| SaveData | 0x98E79C; stage int16 0x98EB14, room 0x98EB16, player type 0x98E9A6 | BioRand offsets +0x378/+0x37A/+0x20A |
| Pad routine | 0x4D0F30, raw pad from `call 0x43BB00` at 0x4D0F53 | game key word 0x988604 (held), 0x988608 (pressed) |
| Key map (config 0) | 0x5338D8: game bit n <- raw PS1 bits | bit0 up 0x1000, bit2 down 0x4000, bit8 aim R1 0x0008, bit6/7/12 = 0x00A0 (circle/square) |
| SCD handlers | 0x53AE10[opcode](SCE_TASK*), task Data at +0x1C | em_set (0x44) = 0x4E77D0 |
| Per-frame entity loop | head 0x4C03CA, move call 0x4C0449 `call [G + id*4 + 0x3C74]` | |
| Room-init entity pass | 0x4DF180 (called once at 0x4DEE3D) | |
| Emd_load | 0x4B1660 (type, pEm, mem_top); types 0x50..0x5A pick Leon/Claire variants by flags | |

Entity offsets (PS1 = PC): Id +0x08, Pos_x/y/z +0x38/+0x3C/+0x40 (int32), Old xyz +0x44,
Cdir_y +0x76, nFloor +0x106, Parts_num +0x107, Status +0x10E, Move_no +0x14C, Life +0x156,
pKage_work +0x16C, pSin_parts +0x198. PC entity allocation is 0x248 bytes.
`PARTS_W` keeps its 0xAC size but its fields differ on PC (pointers into the renderer's heap).

## Experiments

1. Bot input: hooking `call 0x43BB00` at 0x4D0F53 and returning our raw pad drives Leon
   (UP 40 frames moved x from -24522 to -24932; LEFT turned Cdir_y 1200 -> -320). Circle confirms
   menus, Start/Cross skip the intro.
2. Spawning an NPC with the game's own em_set handler (fake SCE_TASK, Data -> our 22-byte opcode):
   - From the pad hook: crash at 0x4C0669 (shadow loop reads pKage_work before init). The NPC
     routine 0x4EF110 skips entities while bit 0x20000000 of 0x989ED4 is set, which it is outside
     the entity loop.
   - From a trampoline at the head of the per-frame entity loop (0x4C03CA): works. Claire (type
     0x51) appears next to Leon in stage 5 room 0x12, with shadow and idle animation, slot 33.
   - Overriding Pos/Cdir after her routine moves and turns her.
3. The em_set handler is asynchronous: if `[0x68A204]+0x13` (file load busy) is set it returns
   without advancing Data; the caller must re-run it next frame (the scheduler does this).

## Unknowns

- Which motion numbers give walk/run/aim for NPC Claire/Leon (em050/em051).
- Whether the NPC can be killed by enemies or block doors.
- How story flags are stored (SaveData flag arrays) — needed for host-authoritative progress.

## Session 2026-10-27 (continued): puppet, network, follow host

Observed (all in a running game, two copies on one PC):

4. `G` = 0x9864E0 (pointer at 0x52D7E4). Compiling `tagGLOBAL` from the PS1 header gives Player +0x3A10,
   pEnemy +0x3C2C, Sce_aot +0x2370, pSce_aot_on +0x2368, Stop_flg +0x39F4, SaveData +0x82BC: all match.
   pEnemy has 34 entries; the PC move table is indexed `[G + type*4 + 0x3C74]`, which overlaps
   pEnemy[18..33] for the unused types 0..15.
5. Per-frame entity loop head 0x4C03CA (`mov edx,[0x991FC0]`), move call 0x4C0449. 0x4DF180 is a
   room-init pass only (called once from 0x4DEE3D); hooking it does nothing per frame.
6. NPC state (Routine_0 = 4): Routine_1 = 4 walk-to-dest, 5 run-to-dest, 6 idle. `plc_dest` (0x40,
   handler 0x4E7330) writes dest to +0x1C4/+0x1C6 and +0x1D8/+0x1DA, arrival flag index to +0x1CB
   (set in 0x98EB6C via 0x503120), speed to +0x1D4. Kind 8 (back step) leaves the NPC idle.
   `plc_motion` (0x3F, 0x4E72D0): Routine = 4 | a<<8, Move_no = b, +0x1CC = c.
   `work_set` (0x2E, 0x4E5E90) stores the target entity at task+0x154.
7. Player Routine_1 while moving: 1 walk, 2 run, 3 back step, 4 turn (bot test).
8. Door AOT handler 0x4E9460, AOT handlers table 0x53B46C. Success path 0x4E95C4. Game sub-mode byte
   `[0x68A204]+9`, mode 4 = door (0x4C085E), which copies IN_DOOR_WORK into the player, sets cut
   (0x98EB18), previous room (0x98EB1A), room, and stage (calls 0x4DEF00 on a stage change).
9. Single instance: exe mutex name at 0x525138 (opened at 0x441EE2), Classic REbirth checks
   `bio2.658b45ea117473d4.game` and `re2cr.aiory0n5jnqf3r7a.game` (ddraw.dll 0x10065839).
10. Launching through `Popen` then attaching by pid is unreliable here (the pid printed for copy A was
    copy B's process once); resolve pids from the process path.
11. Room change: `ROOM` changes when the door animation starts; the new room's entity loop starts
    ~2-3 s later; the engine itself resets pEnemy[33] at room init.

QA (ran, 2026-10-27): `qa_suite.py steam-mock` 17/17, `qa_suite.py udp` 17/17. Real Steam (one
account): init, lobby created, title shows "en attente du joueur 2". Cross-stage follow by hand:
client from stage 0 room 0x0F to stage 5 room 0x12, in control afterwards.

Guesses (not verified):
- Zombies probably ignore the puppet (it is not in the partner slot pEnemy[1]).
- em slot 31 is probably unused by the stock rooms.

Unknown: behaviour in cutscene rooms, during Ada/Sherry sections, on death; two-account real Steam.

## Session 2026-10-27 (enemies, chunk 1): where enemies take damage and die

Derived from static disassembly of `bio2.exe` (capstone index), **not yet checked in a running game**:

- `pEnemy_damage_tbl[48]` = 0x539F50, indexed by entity Id (0x10..0x2F = enemy types). A second
  table at 0x53A010 is used when byte 0x6805B3 is non-zero (guess: difficulty). Each entry points to
  `ATTACK_WEAPON_EM[19]` (PS1 layout, 20 bytes; PC tables are 0x180 apart), indexed by item id - 1.
  `Damage` packs three 10-bit values (level 0..2, chosen by distance or hit zone), `SubDamage` three
  3-bit values. Zombie (0x10) entries: knife 3, handguns 16/15/14, magnums 900, shotgun 200/60/40.
- `Pl_shot_damage` 0x4D3440 (args: shooter `PLAYER_WORK*`, `(item-1)<<16 | ...`, hit table), 6
  callers (0x4D54F0, 0x4D8660, 0x4D8B40, 0x4D8EA0, 0x4D9140, 0x4F3C70). At 0x4D3BAD:
  `Life -= damage; Tmp_routine = Routine; Routine_0 = 2` (hit), `3` if Life < 0 (dead).
- 0x4D7E30 (18 callers in 0x4BA000..0x4BE000, guess: projectiles) and 0x4D8260 (radius damage over
  every pEnemy slot, also subtracts from 0x98EE7C) apply damage the same way, plus
  `Damage_flg |= 1` (+0x1D0) and `Damage_no = 1` (+0x1D2).
- `Em_dead_flg_on` 0x4B45A0 (ENEMY_WORK*): sets kill bit `Em_set_flg` (+0x1CE) via 0x503120 in
  0x98EB74 (stage < 3 and bit 0x2000000 of 0x989ED0 clear) or 0x98EB94 (the second 32 bytes).
  Called by 32 per-type die routines (0x45708F ... 0x4B0F00).
- 0x4D97B0, which also writes Life from the per-frame loop, is the player's poison tick (0x98A108).

ENEMY_WORK offsets (compiled from the PS1 header): Status_flg +0x154, Down_cnt +0x1C8,
At_hit_flg +0x1CA, Sce_flg +0x1CC, Em_set_flg +0x1CE, Damage_flg +0x1D0, Damage_no +0x1D2,
Damage_cnt +0x1D3, Tmp_routine +0x1FC, pDamage_work +0x200, size 0x244.

Consequence for sync (derived): the guest can report its hits by hooking the three `sub [reg+0x156]`
sites (0x4D3BAD, 0x4D7F49/0x4D80F5, 0x4D8344); "dead" on the host = Routine_0 becomes 3.

## Session 2026-10-27 (enemies, chunk 2): host -> client enemy snapshot

re2coop (local, not released): packet kind 3, protocol version 4. The host sends every entity in
pEnemy[2..33] with Id 0x10..0x3F (not the puppet, not NPCs) every second entity-loop frame:
slot, Id, Be_flg, Move_no, Routine_0..3, pos x/y/z, Cdir_y, Life (20 bytes each, at most 24 so the
packet stays under the Steam queue's 512 bytes). The client keeps the latest one (applied in chunk 3).

Observed (two copies on one PC, stage 5 room 0x12):
- Spawning a zombie (type 0x10) with the em_set handler works in a room without zombies: its EMD
  loads and it gets Life 80, Routine (1, 0, 1, 0), and walks by itself.
- The per-frame entity loop runs at 30 Hz, half the pad hook's rate.
- `qa/enemytest.py`: 11/11 on steam-mock and udp (snapshot 14.5/s, client gets the host-only zombie
  in slot 22, position gap 8, same Life). `qa_suite.py` still 17/17 on both transports.

## Session 2026-10-27 (enemies, chunk 3): the client mirrors the host's enemies

re2coop (local, not released), `src/enemies.cpp`. For each client entity whose slot and Id match an
entry of a fresh (< 0.5 s) snapshot for the client's room, around its own move routine:
- before: if its Routine_0/1 differ from the host's, set them and reset Routine_2/3 to 0 (so the
  state's init step runs); copy Life; if the host's is dead (Routine_0 3 or Life < 0), set
  Routine_0..3 = 3,0,0,0 once. Entities still in Routine_0 = 0 (init) and dead ones are left alone.
- after: write Pos / Old pos / Sca_old from a per-slot smoothed position (halfway to the host's
  each frame) and turn Cdir_y halfway.

Observed (two copies, stage 5 room 0x12, zombie 0x10 spawned in slot 22 on both):
- **Chunk 1 checked live:** aiming (R1) and firing (square, raw 0x80) lowers the zombie's Life
  80 -> -1 and Routine_0 goes 3 then 7 (7 = lying dead; Life ends at -1 here).
- Forcing a state on an entity still in Routine_0 = 0 crashes at 0x4C0669 (shadow loop reads
  pKage_work before the init step sets it up).
- The zombie's routine rewrites Pos every frame from its own state: test-only writes to Pos do not
  stick, and smoothing from the entity's Pos never converged (constant gap ~156). Keeping our own
  smoothed position fixed it (gap 6-10).
- Writing 3,0,0,0 into a zombie already at Routine_0 7 made it get up again (1,0,1,0): never force
  death twice.
- `qa/mirrortest.py --shoot`: 11/11 on steam-mock and udp; `enemytest.py` 11/11; `qa_suite.py` 17/17.

Not tested: a host zombie that walks around (the spawned one stays put, so tracking was only
checked with the client's zombie pushed away), several enemies, other enemy types, room scripts'
own enemies, the client corpse position after death (no longer synced once dead).

## Session 2026-10-27 (enemies, chunk 4): the client's hits reach the host

re2coop (local, not released), `src/enemies.cpp` + packet kind 4 in `src/coop.cpp`:
- Client only: the four `sub word [reg+0x156], reg16` sites (0x4D3BAD ebx/dx, 0x4D7F49 esi/ax,
  0x4D80F5 edi/dx, 0x4D8344 esi/ax; all `66 29 xx 56 01 00 00`, 7 bytes) jump to a stub that reports
  (entity, damage), replays the same `sub` and jumps back. Flags after each site do not depend on
  the `sub` (the next instruction is a `cmp`, `mov` or another `sub`).
- A hit = (seq, slot, Id, damage). The client resends hits younger than 0.5 s with every state packet
  (30/s, at most 16 per packet); the host applies each seq once, only for its own stage/room, the way
  0x4D3BAD does: `Tmp_routine = Routine; Life -= damage; Routine = Life < 0 ? 3 : 2`. Skipped if the
  host entity is in Routine_0 0 (init), already dead, or a different Id. Seqs start at GetTickCount()
  so a restarted client does not reuse old ones.
- The client's own copy still takes the hit locally (immediate feedback); its Life is then copied
  from the host again (chunk 3).

Observed (two copies, stage 5 room 0x12, zombie 0x10 in slot 22):
- One client shot (Claire, R1 + square) took 300 Life on the client; the host applied 300 once
  (80 -> -220) and its zombie went to Routine_0 7 (lying dead); the client's went to 7 as well.
- Once, spawning the test zombie before the client's puppet existed made both em_sets share the same
  entity allocation and crashed the client at 0x4C0669: test-setup race (debug command 4 vs puppet),
  not the hit code. `qa/hittest.py` now waits for both puppets first.
- `qa/hittest.py`: 11/11 on steam-mock and udp; `qa_suite.py` 17/17, `enemytest.py` 11/11,
  `mirrortest.py --shoot` 11/11, on both transports.

Not tested / not synced:
- A hit that does not kill (host Routine_0 2, flinch): every client shot so far was a one-shot kill.
- Per-part sub-damage bytes (+0x151..0x153, limbs) and Damage_no (+0x1D2) are not sent.
- If the client kills its copy with a hit the host does not agree on (Life differs), the client
  corpse stays down while the host's zombie lives.

## Session 2026-10-28 (enemies, chunk 5): several zombies, hits that do not kill, walking zombies

Derived from static disassembly, then checked live (two copies, stage 5 room 0x12, zombies 0x10
spawned with em_set in slots 20-22 on both copies, test-only Life writes on the host):
- **Pl_shot_damage writes more than Life.** After `Life -= damage` (0x4D3BAD) it sets Damage_flg
  |= 0x20/0x40/0x80 (side the shot came from, 0x4D3C7D..0x4D3CB0), Damage_no = level * 3 + zone
  (0x4D3CCE), **Routine_1 = weapon item id** (0x4D3CDD), Damage_cnt (0x4D3D02) and the part lives
  +0x151..0x153 (0x4D3BC5..0x4D3C39).
- **Zombie hit routine** (Routine_0 2, 0x455780) calls `table[Routine_1 * 9 + Damage_no]` at
  0x455B9D (table 0x526ED4): entries 0..8 are data, so Routine_1 0 jumps into data. Chunk 4 set
  Routine 2 with Routine_1 0 on the host: crash at 0x8032008C / 0x803E0096 on the first hit that did
  not kill (chunk 4's tests only ever killed in one shot).
- **Zombie death routine** (Routine_0 3, 0x4585B0) calls `table[Routine_1]` at 0x4586A1 (table
  0x527464) when Status bit 0 is set: entry 0 is null, 1..18 all point to 0x458BF0, 19 to
  0x459E10. Forcing death with 3,0,0,0 (chunk 3) crashes at 0x00000000 once the zombie has Status
  bit 0 (seen with Status 0x2001).
- A killed zombie's death routine sets its Life to -1 itself (host 80 - 300 ended at -1).
- Zombies also kill themselves in their own routines (Life writes at 0x451DB2, 0x451DEB, 0x453A3D,
  `dec Life` at 0x452EE9 and 0x453448). Seen once per run in the earlier test layout: a host zombie
  at Routine 01 01 05 00, Status 0x2001, Life 80 went to 07 00 00 00 Life -1 with no hit (switch on
  Routine_2 at 0x45359E, case 6 = 0x453A19). Cause of entering that sequence: unknown.
- One shot with this save's weapon for Claire (Routine_1 8 after the hit) hit all three zombies for
  300 each; the next shots hit nothing (guess: out of ammo).

re2coop changes (local, not released), protocol version 5:
- Client hit = (seq, slot, Id, damage) plus what the damage routine left on the client's entity,
  read before that entity's next move (`enemies_finish_hits`): Routine_1, Damage_no, Damage_flg,
  Damage_cnt and the part-life deltas. The host applies all of them (Routine = 2 or 3 | Routine_1).
- Snapshot entries carry Damage_no and Damage_flg (the old pad); the client copies them when it
  takes the host's Routine_0 2.
- Forced death on the client: Routine_1 = the host's if 1..18, else 1.
- The host installs the same four hooks in log-only mode (`local hit ... by player/puppet/other`).

Observed (one client shot, host Life 2000 / 80 / 2000): 3 hits recorded, 3 applied once; host
lives 1700 / -1 / 1700; the two hurt zombies went through Routine_0 2 on both copies; the killed one
went 3 then 7 on both and stayed dead 5 s; the hurt ones stayed alive; while the host walked away
its two zombies moved up to 2134 and the client's stayed within 199 of them.

QA (ran 2026-10-28; `qa_suite.py` now has section 6 = `enemylib.multi_enemy_checks`, `qa/multitest.py`
runs it alone):
- steam-mock: `qa_suite.py` 32/32, `enemytest.py` 11/11, `mirrortest.py --shoot` 11/11, `hittest.py`
  11/11, `multitest.py` 15/16 (tracking gap 682).
- udp: `qa_suite.py` 17/18 (section 6: the client's three spawns returned 0, cause unknown),
  `enemytest.py` 11/11, `mirrortest.py --shoot` 11/11, `hittest.py` 11/11; `multitest.py` 7 runs,
  3 passed 16/16, 2 failed only on the tracking gap (959-1077), 2 failed because the client's shot
  hit nothing (0 hits recorded, so 8/16).
- **multitest is flaky; the cause of the tracking failure was observed:** at the worst gap the
  client player was in Routine_0 5 (grabbed/hurt, `05 01 03 00`) and the client's zombie was 338
  from her, 959 from the host's copy. The client's own zombie routine still runs (only Routine_0/1
  are forced), so it grabs the client's player by itself and the grab holds it there. Guess (not
  verified): the shots that hit nothing were fired while she was being grabbed. This is step 2
  (who enemies attack, damage to players), not fixed here.

Not tested: enemies placed by room scripts, other enemy types, part loss (a limb at -1), a host hit
while the client's copy is mid-flinch, network loss on hit packets.

## Session 2026-10-28 (enemies, step 2a): enemies never act on the client's player by themselves

Observed (multitest diagnostics, two copies): the client's mirrored zombies grabbed the client's
player (her Routine_0 5, grabber pointer 0x98A0A4 = that zombie). Cause: a zombie's Routine 1/1 is
the grab itself; its step 0 (0x4535A5) sets 0x98A0A4 and the player's Routine to 5 with no
distance check (the choice to grab is made earlier, on the host). Copying the host's state (its
zombie grabbing the host's player) made the client's copy grab the client's player. Some grabs also
came from a zombie in its first frames, before the first snapshot for it.

Change (re2coop, local): on the client, while the host is in the same room, every enemy's move
routine (Id 0x10..0x3F, mirrored or not) runs inside `PlayerShield`: the player block and the flags
before it (0x989ED0..0x98A10C: SYS_FLAGS, Stop_flg, G.Player, grabber pointer) are saved, the
player stands where the host's player is (so the enemy's choices match the host's), and the block
is put back after. A first try that moved the player 16000 units away instead made the client's
zombies change state every frame (`mirrortest` "same top-level state": 0/50). Also: dead mirrored
zombies keep following the host's position (corpse), and position smoothing went from 1/2 to 3/4
per frame.

Facts (static, checked live through the grabs above): 0x4DCA40 is the common "player takes damage"
function (args: damage, ?), 60 call sites in enemy routines, e.g. the zombie bite at 0x453740.
SaveData.Item_work[11] (Id, Num, Size, pad) is at SaveData + 0x598 = 0x98ED34 (from the PS1 layout,
anchored on Room_flg +0x3D0 and Enemy_flg +0x3D8 which were already checked); this save's Claire
weapon is item 8 with 3 rounds.

Tracking during a lunge (Routine 01 03 08): the host's zombie moved ~530 units between snapshots;
the client's copy was up to ~700 behind for a moment (network latency). `multitest` now checks the
90th percentile of the gap (< 400) and the max (< 1500, the snap distance).

QA (ran 2026-10-28, two copies on one PC): steam-mock and udp each: `qa_suite.py` 33/33,
`enemytest.py` 11/11, `mirrortest.py --shoot` 11/11, `hittest.py` 11/11, `multitest.py` 17/17 in 5 of
5 runs (new check: the client's player is never grabbed). The tests now restart both copies once
if one does not reach the game (`enemylib.start_pair`).

Consequence (not done yet, step 2b): with the shield, the host's zombies cannot hurt the client at
all while both are in the same room; the host's zombies only chase the host's player.

## Session 2026-10-28 (enemies, step 2b): the host's enemies attack the client too

Facts found on the way (static, then checked live):
- **The frame loop caches each enemy's distance to the player** right before calling its move:
  `[enemy+0x1F0] = isqrt(dx*dx + dz*dz)` with 0x451780 (0x4C0416..0x4C043A, just before the move
  call at 0x4C0449). Zombies decide by it: the approach state (Routine 01 02, 0x453010) loads
  `ebp = [esi+0x1F0]` and goes for the player below 0x4B0 (1200) on the same floor (+0x106 vs
  0x989FF6). Moving the player inside our hook without recomputing it did nothing on the host
  (zombie parked at 850 from the puppet for 25 s) and made the client's copies decide by the
  client's own player (Routine_1 3 vs the host's 2).
- `Pl_damage` (0x4DCA40, args damage and mode, returns 0 alive / 1 / 2 dead) writes Life
  0x98A046 and flags 0x98A044/0x98A0C3, and on death sets bit 0x4000000 of 0x989E6C (outside
  the player block): the shield now keeps that bit too.
- Vanilla control (copy A alone, co-op off, three spawned zombies, 45 s): zombies grab (zombie
  `01 03 xx`, Leon `05 01 xx`), bite 20 per bite (Leon 50 -> 30 -> 10 -> 0), are pushed back
  (`01 05`, they move away), Leon dies (`06`). No zombie died by itself.

re2coop (local, protocol 6):
- Host: an enemy nearer the client's puppet than the host's player (20% hysteresis) targets the
  puppet. Its move runs in a PlayerShield with the host's player on the puppet (Pos, Old, Sca_old,
  cached distance), the puppet moved out of the way, and a hook at the entry of Pl_damage
  (`push esi; call 0x50AAA0`, 6 bytes) recording (damage, mode). Packet kind 5 sends those to the
  client (resent 0.5 s, seq once); the client calls Pl_damage itself, so difficulty, death and
  flags follow the game's own rules. Snapshot entries flag "targets the puppet" (Be byte bit 7);
  the client's shield then leaves its player in place for that enemy.

Observed (`qa/attacktest.py`, host walked ~2600 away, zombie spawned 900 from the client): the host's
zombie switched to the puppet, approached, grabbed (`01 03 03`), 4 bites of 20 reached the client
once each (Life 200 -> 170 -> 140 -> 110 -> 90, Pl_damage returned 0), the host's player stayed at
200. Both copies of the zombie kept the same state.

QA (ran 2026-10-28, steam-mock and udp): `qa_suite.py` 33/33 each, enemytest/mirrortest/hittest
11/11 each; `attacktest.py` 8/8 in 5 of 6 runs (once the host's player was seen in Routine_0 5
without losing Life; guess: the pushed-back zombie passed near him); `multitest.py` 17/17 in 5 of 6
runs (once all three host zombies died with no hit).

Known issues / unknown:
- The client's player does not play the grab animation (only Life drops); the zombie grabs air
  next to her.
- After a grab of the puppet the host's zombie is pushed back far (~4000-5000 units; vanilla
  push-back is shorter): guess, the push-back is computed from the host player's real position.
- Rare (about 1 in 6 multitest runs, seen before step 2b too): a host zombie goes from
  `01 01 05 00` to `07 00 00 00` with Life -1 and no hit. Cause unknown; not seen in the vanilla
  control.

## Session 2026-10-28 (night): packet loss, a room's own enemies, kill flags made host-authoritative

Packet loss (re2coop `[test] loss=N` drops N% of every outgoing packet, both copies; `qa_suite.py`
writes it from `RE2COOP_LOSS`). Ran on udp, loss confirmed in both logs:
- 25%: `qa_suite.py` 33/33, `hittest` 11/11, `mirrortest --shoot` 11/11, `attacktest` 8/8 twice,
  `multitest` 17/17 twice.
- 50%: enemy tests all pass (hittest, mirrortest, attacktest, multitest); `qa_suite.py` 31/33: the
  puppet stayed idle while the host ran ("puppet runs while host runs", modes [6]) and fell 2087
  behind. Not investigated.

A room's own enemies (debug command 5 = warp through a made-up door to stage/room/spot):
- Survey at save "Leon A_02" (stage 0 room 0x0F): stage 0 rooms with spawned entities: 0x00
  (6 x type 0x1F), 0x01 (0x48), 0x03 (9 x 0x1F), 0x04 (6 x 0x1F), **0x05 (3 x 0x10 zombies, slots
  2-4)**, 0x07 (0x1F x3, 0x13), 0x09 (5 x 0x21), 0x18, 0x19, 0x1A (0x12), 0x1B. Types other than
  0x10: unknown here.
- `qa/roomtest.py` (stage 0 room 0x05): the host warps in, the client follows; same zombies in the
  same slots on both, client snapshot lists them, gap p90 28-63, same state 149/150. The client
  kills one (Leon's handgun, 14 damage, test-only host Life 10): it dies on both, its kill bit is set
  on both (byte 0 of 0x98EB74: 0x08 -> 0x28), and after leaving and coming back the killed one stays
  gone on both while the others return.

**Kill flags were OR-merged both ways; now host -> client only, copied as they are.** Found when
`roomtest` lost one more zombie per run although no file changed: copy B sometimes loaded the other
save (a menu key press lost in `session.py`), its follow_host then walked it into the host's room,
and the two-way OR merge pushed that save's kill bits (plus earlier test kills kept in B's memory)
into the host's fresh game. Evidence: copy A alone (co-op off) loads 0x98EB74 = the A_02 file's
bytes at file offset 0x3D8 (`08070000071b00e01c7c...`); in co-op it held the A_08 file's bytes OR'ed
with 0x30. In real play each player loads their own save, so the old merge would have corrupted the
host's world. Protocol 7. The client's kills still reach the host through hits (chunk 4) and come
back as the host's own kill bits. `session.py` now prints the room the save loaded (before
follow_host) and `enemylib.start_pair` restarts both when they differ.

Save file format seen: 2048 bytes, raw SAVE_DATA (Stage_no at 0x378, Enemy_flg at 0x3D8).

QA after the kill flag change (ran 2026-10-28 ~05:50, two copies on one PC), steam-mock and udp
each: `qa_suite.py` 33/33, `enemytest.py` 11/11, `mirrortest.py --shoot` 11/11, `hittest.py` 11/11,
`multitest.py` 17/17, `attacktest.py` 8/8, `roomtest.py` 11/11 in 3 of 3 runs (all three room
zombies present on every run).

## Session 2026-10-28 (early morning): other enemy types, a door back into the same room

`qa/roomtest.py` takes `RE2COOP_ROOM` / `RE2COOP_TYPE` and `--no-kill` (presence, slots, tracking,
state only). Stage 0 at save "Leon A_02" (ran 2026-10-28, udp): room 0x03 (9 x type 0x1F, slots
2-10) 7/7, gap p90 31; room 0x07 (type 0x13, slot 5) 7/7, p90 196; room 0x09 (5 x type 0x21) and
room 0x1A (type 0x12) 7/7 after the fix below. What these types are in the game: unknown here.

**Crash: a door back into the same room.** Client crashed at 0x4C0669 (shadow pass,
`mov eax, [edi+0x14]` with edi = [entity+0x16C] = 0xA343D100) on the puppet in room 0x09, every
time the client was warped into the room it was already in. The plugin only noticed room changes by
stage/room id, so the rebuild went unseen and the puppet was spawned while the room was still
loading; its shadow pointer got clobbered after our pre-loop checks (a pre-loop guard on pKage never
fired). Fix: a running door (top byte of Stop_flg 0x989ED4) now counts as a room change and holds
puppet spawns back (a first try that only delayed the spawn to 90 pad frames did not fix it and
made `qa_suite` miss the puppets after a follow; reverted to 20).
Also added: entities we create with em_set are kept inactive while their pKage is outside
0x988904..+0x68*48 (shadow works; enemy entries are not aligned on the player's 0x988904, e.g.
0x988D38, 0x988DA0). In real play this crash would have hit a client going through any door that
leads back to the same room.

QA at the end of the night (two copies on one PC):
- Full battery with the door fix and the 90-frame spawn delay (ran ~06:25-06:52): steam-mock
  `qa_suite.py` 31/33 and udp 32/33 (puppets not yet visible 1.5 s after a follow: the delay),
  everything else passed on both: enemytest/mirrortest/hittest 11/11, multitest 17/17, attacktest
  8/8, roomtest 11/11; udp `roomtest --no-kill` rooms 0x03, 0x07, 0x09, 0x1A 7/7 each; no CRASH in
  either log.
- After reverting the delay to 20 (ran ~06:55-07:04): `qa_suite.py` steam-mock 33/33, udp 33/33;
  `roomtest --no-kill` room 0x09 7/7 twice; `roomtest` room 0x05 11/11; no CRASH. The enemy tests
  were not rerun after this revert (the delay only gates the puppet spawn).

## Session 2026-10-28 (day): shared items and story flags

Owner's decision: items and story flags are **shared** (an item one player picks up is gone for
both; a door one unlocks is open for both). Host-authoritative, like the kill flags.

Observations (static, bio2.exe Sourcenext 1.1.0):
- `0x503120` Flg_on(ULONG* array, n), `0x503140` Flg_off, `0x503170` Flg_ck: bit `0x80000000 >> (n & 31)`
  of dword `n >> 5`. Flg_on has 48 callers, Flg_off 6.
- Item pickup sets the item's flag with Flg_on at `0x4FDFFB` / `0x4FD996`: array SaveData+0x418
  (`0x98EBB4`) in stages 0-3, SaveData+0x438 (`0x98EBD4`) from stage 4. The flag number is a word
  in the item AOT data.
- AOT table: `0x988850` (pointers), count byte `0x98E528`. Entry: Sce, Sat, floor, super, X, Z, W, D
  (int16), then data; for an item (Sce 2): item id, count, flag (words at +0x0C/+0x0E/+0x10).
  AOT type handlers: table `0x53B46C` (Sce 1 door `0x4E9460`, Sce 2 item `0x4E96C0`).
- AOT check `0x4E9150`: the point checked is the player position plus a 620-unit (0x26C) offset
  rotated by Cdir_y; the check-button pass runs when Status flags `0x989ED0` has `0x200000`.
- Key_flg (SaveData+0x590, `0x98ED2C`) is read by the door code (`0x4E94C0`, `0x4E9589`).

Observed live (two copies, save slot 1):
- Stage 0 rooms with item AOTs (flag in brackets): 0x05 (4), 0x06 item 30 x3 (7), 0x09 (13),
  0x0E (105, 106), 0x11 (55), 0x12 (22), 0x13 (18), 0x15 (40, 209), 0x16 (57, 59).
- With Item_flg bit 7 set by the host and copied to the client, the client re-entering room 0x06
  has no item AOT any more (it had flag 7 before).

What re2coop does now (protocol v8, local, not pushed):
- Shared SaveData ranges: +0x390..0x3D0 (Scenario_flg, Common_flg), +0x418..0x484 (Item_flg,
  Item_flg2, Item_c_flg, Map flags), +0x584..0x598 (File_flg, Zapping_flg, Key_flg). Not Room_flg,
  not Enemy_flg (kill flags, kind 2), not Pri_be_flg.
- kind 6, host -> client, 4 per second while in game: the host's copy; the client takes it.
- kind 7, client -> host: Flg_on/Flg_off are replaced by the plugin's own copies (5-byte jmp), which
  on the client record each bit the game sets or clears in a shared range. Sent 4 per second until
  the host's copy shows them. A first version diffed memory against the last host copy and reset
  that base when the game left the entity loop (save load). Dropped: the entity loop also stops in
  pickup messages and doors, so a pickup made there would have been overwritten.
- Debug cmd 6: call Flg_on/Flg_off (array, bit, on).

QA (2026-10-28): `qa/storytest.py` 16/16 on udp, steam-mock, and udp with 30% loss.
`qa_suite.py` 33/33 on udp and steam-mock; udp at 30% loss 32/33: "puppet never further than snap
distance" failed (max gap 1653), the puppet-under-loss issue seen at 50% last night; not
investigated.

Not done / unknown:
- (Solved later the same day, see below.) A real pickup by a bot first failed: right after a warp,
  square sets nothing.
- (Done later the same day, see below.) An item the other player took while you were in the same
  room stayed visible until you re-entered.
- Code that writes the flag arrays inline, without Flg_on/Flg_off (capstone scan of .text for
  memory-operand writes into the shared ranges, 13 hits): the low byte of `0x98EBD0` (SaveData+0x434,
  last dword of Item_flg) is a puzzle state written directly at `0x4CB6F0`, `0x4E3A82`, `0x4E3A9C`
  (which puzzle: unknown); Zapping_flg (+0x588/+0x58C) is copied whole at `0x4B7759`. The other 8
  hits are Item_work entries addressed as `[i*4 + 0x98ED30]` (1-based index). Consequence: these two
  written on the client are overwritten by the host's copy (the host's puzzle state wins).
- Inventories stay separate (each player's Item_work).
- A crash of copy B at `0x4E2AF2` during this session came from a debug write of -1 to
  `0x98EAE4` (an AOT index the game reads), not from the plugin.

Real pickup by a bot (2026-10-28, later):
- Observed: right after a debug warp the player is in Routine `01 00 00 00` and square does
  nothing; after one cross tap (Routine `01 00 01 00`) square sets the check bit `0x200000` and,
  facing the desk of room 0x06 from (-25252, 3801) with Cdir_y 2048, opens the item message
  (Status `0x8000`). Square then confirms (ink ribbons 1 -> 4, Item_flg bit 7 set by the game).
  Why the cross tap matters: unknown. `pickuptest.py` retries cross + square up to 4 times.
- The typewriter (AOT Sce 9) shares the item's rectangle: once the item is taken, square there
  opens the save prompt, and confirming it uses an ink ribbon and writes a new save. One run did
  that on copy A (new file `Leon A_03_...` in `savedata`, moved out by hand). The test now cancels
  with cross and checks that no save file appears.
- `qa/pickuptest.py` (both in room 0x06; one picks it up with the buttons, the other must get the
  flag and no item message): 11/11 with the client picking and with the host picking, on udp with
  30% loss (3 runs), steam-mock and udp without loss (both orders each); `qa_suite.py` 33/33 and `storytest.py`
  16/16 on steam-mock and udp in the same series.

Item taken by the other player, same room (2026-10-28, later):
- Observed (static): after Flg_on the pickup at `0x4FE005` reads MD1 (byte +6 of the item data) and,
  unless 0xFF, zeroes Be_flg of object work `0x98A61C + MD1 * 0x1F8`, which hides the item's model.
- re2coop now does the same when a flag in Item_flg (stage 0-3) or +0x438 (stage 4+) goes 0 -> 1
  through a peer's packet (client: host copy; host: client change) and an item AOT (Sce 2) of the
  current room carries that flag. SharedBlock `items_hidden` counts it.
- `pickuptest.py` 13/13 (udp and steam-mock, both orders; qa_suite 33/33 and storytest 16/16 in the same series): the room 0x06 item's model (MD1 3, Be_flg 0xB on both) goes to Be_flg 0 on
  the other player within 2 s, without leaving the room. Checked in memory only: screenshots of the
  copies came out black at that point, so not checked by eye.
