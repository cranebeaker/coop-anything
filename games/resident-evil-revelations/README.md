# Resident Evil Revelations (2012, PC port 2017)

| | |
|---|---|
| Engine | MT Framework (`KATARIBE`, project tree `bhr\biorevhd`) |
| Versions tested | PC/Steam appid **222480**, build 13298487; `rerev.exe` 14,260,576 bytes, SHA-256 `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA`; x86, image base 0x400000, no ASLR |
| Projects | `re1coop` (in progress, unpublished) — a campaign co-op mod built on the game's own netcode |
| QA level | none |
| Last updated | 2026-10-23 |

## Where things stand

**The shipped game cannot be played co-op in its campaign, because it ships no
campaign co-op.** The PC release has exactly one two-player mode, and it is
not hidden: **Raid Mode**, the second entry of the main menu
(`КАМПАНИЯ` / `РЕЖИМ «РЕЙД»` / `ПАРАМЕТРЫ`). The game's own description of that
entry is *"Режим совместного прохождения, в котором вы боретесь с врагами
разной сложности."* The campaign is single player and its pause menu has no
invite path (`ЗАГРУЗКА` / `ПАРАМЕТРЫ` / `ВЫЙТИ`).

**The project is now to build that co-op anyway, from scratch, on the network
code the game already ships.** The reverse engineering says that is the right
route: the network stack, the session classes and the replication vocabulary
are present in the binary and drive Raid Mode today. Whether they can carry a
campaign session is **unproven** - they are present in the binary, their
reachability from the campaign is not.

What the reverse engineering has established so far:

* the game registers **4071 types** in one tree (the MT Framework's own object
  model), including **137 sync payload classes** and **54 `nNetwork::*`
  classes** - the game's replication vocabulary is fully present;
* the session layer is `nNetwork::Session` / `SessionDatabase` / `SessionDriver`
  / `SessionListener`, on top of a **Valve Source-style netchannel stack**
  (`KSP3=PROTOCOLSYSTEM`, `KPP3=PROTOCOL_COMM_ON`) over `ISteamNetworking`
  and `WS2_32`;
* the registry is an **extension point**: the mod can add its own payload type
  to it - in the same class and under the same parent as the game's own sync
  payloads - and the game's allocator hands out instances of it from the
  NETWORK heap. This is verified in the running game, not inferred;
* the campaign really is disconnected: with the game at the main menu the
  network manager exists but has **no session** (`session=0`, `playmode=-1`).

The measured details are in
[NOTES.md](NOTES.md) - the seventh and eighth sessions have the full
write-up.

## Existing work

| project | state |
|---|---|
| `re1coop` (in progress, unpublished) | proxy `dinput8.dll` that loads into the game. It reads the game's mode, the game-system instances and the network manager, walks the game's type registry, registers one payload type of its own in it, and allocates an instance of it from the game's NETWORK heap. No traffic yet. |
| No other project found | — |

Searched on 2026-10-22: Steam community, GitHub, general web search turned up
nothing for the PC release. Nexus Mods and ModDB refused the fetcher with
HTTP 403 and the Steam community answered 429, so those are only partly
covered; Reddit and YouTube were not checked.

## The mod

The mod lives in its own repository (`re1coop`). It loads as a proxy
`dinput8.dll` next to `rerev.exe`, patches what it needs through the IAT, and
drives the game's own session and synchronisation stack.

Current slices:

| module | what it does |
|---|---|
| `coop_mode` | reads the game's mode field. Read-only, and it stays that way: writing `1` into a campaign would point the engine at Raid scene data. |
| `coop_state` | samples the network manager: session pointer, status, play mode, player index, flag bits. |
| `net_registry` | reads the game's type registry: finds a type by name, walks the tree, registers a new type. |
| `net_probe` | drives `net_registry`, logs what the registry holds, registers `re1coop::cCampaignSyncData`, samples the session. |
| `pad_probe` | reports which XInput user index the game polls. |

### The type registry - the extension point

Every registered type is a static object in `.bss`:

```
+0x00  vtable           the concrete class vtable
+0x04  name             "cCoopSyncData::cZZZSyncData"
+0x08  next sibling
+0x0C  first child
+0x10  parent type
+0x14  next in the hash bucket
+0x18  size (low 23 bits) | class id (bits 23..28) | flag29 (bit 29)
+0x1C  type id = hash(name) & 0x7fffffff
```

Registration is one function, `FUN_008E1330`, called from a static
initialiser block. The mod calls it with its own name and its own parent
descriptor, and the game accepts the new type:

```
net registry: 4071 types reachable from 'MtObject' (54 nNetwork::, 137 *SyncData, 14 cCoopMenu::)
net registry: the sync payload parent is 'cRemoteCall' (size=8 class=16 id=4cd24d24)
net registry: registering 're1coop::cCampaignSyncData' with class=16
net registry: registered 're1coop::cCampaignSyncData' size=32 class=16 id=57fe3c6a
net registry: 're1coop::cCampaignSyncData' found at 68c0c1bc (id=57fe3c6a, parent='cRemoteCall')
net registry: 're1coop::cCampaignSyncData' shares the class record of its parent at 058ceac0
net registry: allocated an instance of 're1coop::cCampaignSyncData' at 215f8370
```

The class id in bits 23..28 selects one of 64 class records that say how
instances of a type are created and serialised; the game uses it as the memory
domain the instances live in. The classes measured so far are `GLOBAL = 1`,
`UI = 7`, `UNIT = 12`, `SOUND = 14`, **`NETWORK = 16`**, `AI = 17`,
`GUI = 19` - the network classes (`nNetwork::Session`, `cRemoteCall`, every
sync payload) are class 16.

**The class id is not passed by the game's own registration blocks** - they
all push 0. It is assigned at run time: `FUN_008E18D0` / `FUN_008E1990` walk
the parent chain and ask each ancestor for a class through its vtable, then
write the answer into the child's bits 23..28 (`FUN_008E1300` propagates it
down). A type registered *after* that pass therefore has to be given the
class explicitly, or it stays in class 0 and its instances are allocated from
the wrong heap. The mod passes the class of the parent (`cRemoteCall` → 16).

The class record is what the game allocates through:

```
FUN_008E1660(desc) = classRecordTable[(desc[0x18] >> 23) & 0x3f]
record->vtable[7]  alloc(this = record, size, 0x10, typeId)   __thiscall
record->vtable[8]  alloc(size, flags)                          raw
record->vtable[12] free(ptr)
```

The allocator in slot 7 is a `__thiscall` method: the game calls it with the
class record in `ecx` (see `FUN_00878000`, the creator of a sync payload).
Calling it as a plain `__cdecl` function leaves `ecx` pointing at nothing and
takes the game down - that was the milestone-1 crash, and it is fixed by
following the convention.

With that, the mod registers a type of its own, gets the same class record as
the game's sync payloads, and allocates an instance of it from the NETWORK
heap. What is still missing is not the replication machinery: it is a session
in the campaign.

### The plan

Milestone 1 is done: the mod's own type is registered in the game's registry
with the right class and instances of it are allocated from the game's own
heap. The next milestones:

1. **a session in the campaign** - drive `nNetwork::SessionDriver` /
   `nNetwork::Session` the way the Raid lobby does, but from a campaign
   chapter. Success criterion: `sNetworkManage` reports a live session while a
   campaign chapter runs.
2. **the partner player** - the game already replicates characters
   (`uCharaBase::cNetSyncData::cPositionSyncData`, `cDamageSyncData`); in Raid
   the second player is a second `uCharaBase` spawned by the co-op menu.
3. **the campaign glue** - the mod's own entry point, its own payload types
   (registration and allocation are proven), and save ownership.

Fallback if the campaign's character sync turns out to be gated on
`mode == 1`: run the campaign chapters inside the shipped co-op engine, with
the campaign's chapter selection feeding the co-op stage list. The netcode,
the sync classes and the spawning are then entirely the shipped ones and only
the flow is the mod's.

## How the game works (for co-op)

### The mode field — what it actually says

One `int32`:

```c
mode = *(int32 *)(*(int32 *)(sGameSystems + 0x24) + 0x20)
```

`sGameSystems` is a global pointer at RVA `0x0D69108`, `+0x24` is `getData()`
and `+0x20` inside the game data is the mode. Measured **in a running game**
with the probe in `re1coop`:

| where | `mode` |
|---|---|
| title screen / main menu | `0` |
| Raid Mode lobby | `1` |
| Raid Mode gameplay | `1` |

So `0` = campaign/neutral and **`1` = Raid Mode**. The same probe shows the
network manager's `playmode` going `-1` (menus) → `0` (Raid lobby) → `1`
(Raid gameplay). No value other than `0` or `1` was observed for the mode.

What this kills: the claim that `1` is a dormant campaign co-op and `2` is
Raid Mode. `1` is the mode the game already uses for its shipped two-player
mode.

### Why the assets looked like campaign co-op

All of these exist in the shipped build, and all of them belong to Raid Mode /
the network layer that Raid Mode uses:

* `game\scene_coop\...` — a parallel scene tree for every episode id
  (`01`…`f1`), 81 `*_event.arc` files;
* `scene_start_list_coop.ssl`, `coop_table\*.lvt`;
* `_coop` variants of scene actors, swapped in by the scene loader
  (`FUN_0041b710` at RVA `0x0001B710`);
* `cCoopSyncData` and the per-object sync payloads (position, damage, item
  pickup, doors, elevators, `uCoopChainLock`), `network::sNetworkManage`
  (`DAT_01169398`), `nNetwork::Session`, `SteamNetworking005`;
* a second game-system instance (`cGameSystemDouble`, `(&DAT_0116a344)[1]`),
  read by `FUN_0041ba70` when the mode is `1`.

`FUN_0042bfc0` (RVA `0x0002BFC0`) does pick between
`archive\game\{scene|scene_coop}\XX\XX_event` from the mode — that part of the
old write-up is accurate. It is the Raid/co-op route of a mode that the game
already ships and enters, not a hidden campaign route.

### How this was measured

No memory pokes: the game was driven through its own menus with injected keys
(arrows need `KEYEVENTF_EXTENDEDKEY`), the screen was read back with OCR, and
the probe read the mode from process memory. The menu contents above are what
the game renders, not what the executable seems to imply.

### Two traps worth knowing

**A pointer trap.** The globals at `0x0D69108`, `0x0D69398`, `0x0D6A344` live
in the image, but the objects they point to live on the heap (`0x220A…`). A
guard that accepts only image addresses silently rejects every game object.
That bug made `re1coop` report `mode=-1` for the life of the project, and
made its mode write a no-op.

**An address trap.** The game is loaded at `0x00400000` with no ASLR, so
`rva = va - 0x00400000`. A hex digit lost in that subtraction points the mod
at a *code* address instead of a descriptor, and the game dies with
`0xc0000005` two seconds after launch. It happened once this session
(`0x011E05C0 -> 0x00DE05C0`, not `0x000E05C0`). The fix is general: the
registry walk now validates every descriptor before it follows it - the
descriptor has to be readable, its name pointer has to be readable byte by
byte, and the name has to be printable ASCII - and the walk has a depth and
node budget.

## What we changed

* **`re1coop`: the mode probe never worked.** `currentGameData()` /
  `currentMode()` rejected heap pointers with an image-range test. Replaced with
  `readableMemory()` (`VirtualQuery` → committed, not `PAGE_NOACCESS` /
  `PAGE_GUARD`). The probe now reports `0` at the menus and `1` in Raid Mode.
  Unit tests cover the helpers; the value was confirmed in a running game.
* **`re1coop`: the mode override is disabled.** Writing `1` into a campaign
  would point the engine at Raid scene data for a campaign chapter. The
  function logs once and returns `false`. The enum/comments that claimed
  `1 = co-op`, `2 = raid` are corrected.
* **`re1coop`: a new type registry module** (`net_registry`, `net_probe`) that
  reads the game's registry, finds types by name and registers one payload
  type of the mod's own. Verified in the running game.
* **Two start-up crashes fixed earlier** (kept, tested): linking the proxy
  without `/guard:cf` leaves an empty CFG valid-target table (`0xc0000409`
  at start-up); using the CRT stream layer inside the DLL makes a narrow
  `fprintf` on a wide stream raise an invalid parameter, which the same CFG
  gap turns into the same fast-fail. The logger uses `CreateFileW`/`WriteFile`.
  `tests/load_config_check.cpp` and `tests/log_test.cpp` catch both.

## QA

- `build.bat` in the `re1coop` repo: configure, build, `ctest`.
- Last suite result: **13/13 passed**, 2026-10-23.
- In-game: the game boots, reaches the main menu and keeps running with the
  registry module active; `re1coop.log` shows the type tree (4071 types), the
  registration of `re1coop::cCampaignSyncData` and its rediscovery by name;
  the game's save file was byte-identical (SHA-256) before and after.
- **QA level stays `none`**: there is no co-op feature to test yet. Two-client
  verification needs a second Steam peer and a second Steam account; neither
  was available, and the game's DRM stub kills a second local instance.

## How to play

Nothing to install yet - the mod does not change gameplay. It is a
diagnostic: drop `dinput8.dll` and `re1coop.ini` next to `rerev.exe` and read
`re1coop.log`.

Raid Mode co-op is the game's own, over Steam, unchanged.

## Known issues

- The mod's readme and earlier hub notes described a "dormant campaign co-op";
  those claims are withdrawn. The measurement replaced them: the co-op layer
  exists and Raid Mode uses it, and nothing connects the campaign to it.
- The old theory's origin is not fully traced: `scene_coop` asset names match
  campaign episode ids, which is probably what made the inference look safe.
  The runtime measurement overrides the inference.

## Next steps

1. **Find the session creation path.** `cCoopMenu::cCreateSession` and
   `cSearchSession` are the game's own "host" and "join" flows; the mod has to
   reach them from a campaign chapter instead of from the Raid lobby.
2. **Find the partner spawn path.** In Raid the second player is a second
   `uCharaBase`; the mod has to make that happen in a campaign scene.
3. **Send.** With a session and a partner, the mod sends its own payload
   through the game's transport (`FUN_00882FD0`) and verifies it arrives.
4. **Two-client test.** Needs a second Steam peer and account; the game's DRM
   stub kills a second local instance.

Do not re-run the "flip the mode" experiment expecting a different result;
the measurement above is reproducible and the mode field is read, not guessed.
