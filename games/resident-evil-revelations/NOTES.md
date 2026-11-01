# Resident Evil Revelations - working notes

Session date: 2026-10-22. Phases covered: 0 (triage), 1 (research), 2 (workspace),
3 (understand), 4 (choose), 5 (build, first slice). Goal: a co-op mod built
from zero.

Nothing in a Steam install was modified. All work happens on a copy of the game
in a test folder (`<test copy>`), never on the install itself.

Sections below are in the order they were written. The findings of the second
part of the session (the mode field, the loaders, the save slots, the input
path and the first build of the mod) are in
[Second session: the mode field, the loaders and the first build](#second-session-the-mode-field-the-loaders-and-the-first-build)
at the end of the file. The findings of the third part (the two game-system
instances, the netcode that already ships with the game, and the second slice
of the mod) are in
[Third session: the game already ships a netcode](#third-session-the-game-already-ships-a-netcode)
further down.

## Game identity (recorded before following any write-up)

| | |
|---|---|
| Game | Resident Evil Revelations (PC/Steam, the 3DS original re-made) |
| Steam appid | 222480 |
| build id | 13298487 |
| Executable | `rerev.exe`, 14,260,576 bytes, SHA-256 `1573B79EB921B1E7F571E6DEB71C39961C7F1940FAD6D19283FC3AEA7FCA48AA` |
| Engine | MT Framework (the build tree calls itself `KATARIBE`, project code name `bhr\biorevhd`) |
| Architecture | x86, 32-bit, image base 0x400000 |
| Sections | `.text` 12,301,312 bytes (entropy 6.41, normal code), `.rdata`, `.data`, `.rsrc`, `.bind` 170,712 bytes (entropy 7.97, SteamStub) |
| Entry point | RVA 0xdf0310, inside `.bind` (SteamStub loader) |
| Test copy | `<test copy>` (a copy of the Steam install; the install itself was never touched) |
| Steam tags | Single-player, Multi-player, Co-op, full controller support, Valve Anti-Cheat enabled. No "Online Co-op" and no "Shared/Split Screen Co-op" tag. |

The `.text` section has normal code entropy, so the game code is statically
analysable; `.bind` is only the SteamStub wrapper.

## Phase 3: the shipped PC build already contains a campaign co-op layer

> **Corrected later in this file, in
> [Session 6](#session-6-the-premise-is-dead---this-game-has-no-campaign-co-op).**
> Everything in this section is what the strings, RTTI names and data files
> looked like before the game was run. Measured afterwards: this layer is Raid
> Mode's, the mode field is 0 at the menus and 1 in Raid, and the campaign pause
> menu has no invite path. Read on for how the wrong reading was made, not for
> the current conclusion.

This is the central finding, and it changes the shape of the mod: the game was
built with a two-player campaign co-op implementation compiled in. The shipped
UI does not expose it, and the store page has no co-op tag, but the code and
the data are in the build.

Evidence taken from the executable (58,907 strings, 1,189 RTTI class names, 120
leaked source paths) and from the data files:

**A global co-op flag with an FSM setter**

- `sGameSystems::getInstance().getData().isCoop()`, `isCoopMode() == false`,
  `mIsCoop`, `mIsCoopData`
- `FsmSetCoopFlag` and `cFsmAction::cSetCoopFlagParameter` - the flag is set by
  the scene script, not by a menu
- `mCoopFlag`, `mCoopState`, `mCoopDifficulty`, `mCoopKeyNum`
- `cCoopManager` / `mCoopManager`

**A second player actor**

- `uPcsActor<Player2>`

**A per-object network sync framework, in campaign-level classes**

| Owning class | Sync data |
|---|---|
| `uCharaBase` | `cPositionSyncData`, `cDamageSyncData` |
| `cNetEnemySync` | `cEnemySyncData` |
| `sItem`, `uItem` | `cDropItemSyncData`, `cPickupItemSyncData` |
| `uDoor2pBase` | `cDoor2pSyncData` (a two-player door) |
| `uLoadRoom`, `uElevatorBase` | load/elevator sync data |
| `sArrange` | arrange, montage, cut move, terminal and fall-out sync data |
| `sObjModel` | `cCoopChainLockSyncData`, `cDoor2pOpenCancelSyncData`, `cElevatorSwitchSyncData` |
| `uCoopChainLock` | `cCoopChainHostCheckSyncData`, `cCoopChainHostAnswerSyncData` |
| `cCoopSyncData` | lobby, ready, chara/equip/colour/mission changes, unlock stage, `cReNetworkDeadSyncData` |

**A co-op UI**

- `uGUI_CoopEnemyHP`, `uGUI_PartnerWarning`, `uCoopChainLock`
- `cCoopMenu::cRaidMenuState`, `cPlayerMatch`, `cCreateSession`, `cLobby`,
  `cSearchSession` - this is the *Raid Mode* online menu, not a campaign menu

**Enemy behaviour for two players**

- `cNpcThinkCoop_Move`, `cNpcThinkCoop_Combat`, `cNpcThinkCoop_SwimWS`,
  `cNpcThinkCoop_SwimUW`
- `mLocalCoopTableRes`, `mGlobalCoopTableRes`, `mChainCoopRes`

### The transport

`prog\game\re_net\ReNetOptional.h`, `sReNetAccessManager`, `mReNetManager`,
`SteamNetworking005`, `mAddressP2p.mPort.mData`, `mPeerId`, `mpP2p`,
`cReNetReliefAidTable`. The exe imports `WS2_32.dll` with a full client **and**
server socket stack (20 functions), `steam_api.dll` (12 functions) and
`XINPUT1_3.dll` (4 ordinals). So the session layer is native code in this
binary, on top of Steam's old networking API.

**Unknown:** whether the campaign co-op path can be pointed at a transport we
control, or whether it is hard-wired to Capcom's matchmaking service.

## Co-op data files in the shipped install

| Path | What it holds |
|---|---|
| `nativePC\game\episode\scene_start_list_coop.ssl` | the co-op episode -> scene table |
| `nativePC\game\episode\<ep>\<room>\texturedetail_coop.tde` | co-op texture list for nine story scenes (13_00, 32_06, 41_00, 51_02, a2_10, f1_00, f1_06, f1_0b, f1_10) |
| `nativePC\game\scene_coop\72\72_07\72_07_s108_coop_em_DARK.arg` | a co-op enemy arrangement |
| `nativePC\game\scene_coop\montage_setting.msd` | co-op montage settings |
| `nativePC\game\chara\em\emXXXX\emXXXX_global_coop.tbl` / `_local_coop.tbl` | per-enemy co-op tables |
| `nativePC\game\coop_table\*.lvt` | co-op rule tables (enemy level, player level, damage rate, dead table, …) |
| `nativePC\game\coop_table\emXXXX\*.lvt` | per-enemy co-op tables (hp, behaviour, tolerant, formidable) |
| `nativePC\game\system\coop_dead.sdl` | what happens when the partner dies |
| `nativePC\msg\Coop_*.gmd`, `epCOOP_*.gmd` | co-op menu text (Raid Mode online: `stage_no_01`…`stage_no_22`, `player_match_button`, `systemlink_button`, `xboxlive_party_button`) |
| `nativePC\id_bhr\32_coopchara` | co-op character portraits/icons |

### The co-op campaign has its own scene flow

`scene_start_list.ssl` and `scene_start_list_coop.ssl` are both 540 bytes,
33 records of 16 bytes, and differ in **18 of the 33 records**. The differing
field is the "next scene" link, so the co-op run visits a different scene
graph than the single-player run. The co-op data is not a bolt-on: it is a
second, authored campaign route.

## Formats decoded this session (all read-only, verified against the real files)

| Format | Layout |
|---|---|
| `.ssl` scene start list | `"SSL"`, u32 version (2), u32 count, then records of 8 u16 |
| `.tde` texture detail | `"XFS"`, quality/param-list/auto-delete/path/format/type fields, then model texture paths |
| `.lvt` value table | `"lvt1"`, u32 version (2), u32 flags, u32 columns, u32 rows, then column descriptors (u8 type + NUL-terminated name) and row-major cells (u8 type + payload: 1 = int32, 2 = uint32, 3 = float32, 4 = string) |
| `.arc` archive | `"ARC\0"`, u16 version (7), u16 entry count, then 80-byte entries (`name[64]`, u32 timestamp, u32 compressed size, u32 size, u32 offset), payloads zlib-compressed, data block starts at 0x8000 |

The `.lvt` parser was checked against all 188 `.lvt` files in the install (every
one parses to exactly its length), the `.arc` parser against all 3,088 archives
and 66,203 entries (every entry decompresses to exactly its declared size).

Reading the co-op tables: **the difficulty scaling is keyed by level, not by
player count.** No co-op table has a player-count column, so any two-player
adjustment is either hard-coded in the executable or applied through the chosen
level index. That is what the executable still has to be asked about.

## Open questions (phase 3, being worked on)

1. Where is the co-op flag set, and what starts a co-op campaign run? The
   `FsmSetCoopFlag` action and the `isCoop()` asserts are the handles.
2. How is the second player created and driven (`uPcsActor<Player2>`)?
3. Which transport does a campaign co-op session use, and can it be pointed at a
   relay we run?
4. Is there any entry point at all in the PC build, or was the campaign co-op
   cut and only the data left behind?

Questions 1 and 4 are answered by the second session below: the co-op
campaign is fully present in the build and the gate is the game mode field.
Questions 2 and 3 are still open and are the two things the next agent has to
resolve.

## Guesses (not verified, listed separately on purpose)

- The co-op layer is probably a leftover from the 3DS original, where the game
  had a two-player local mode; the PC port kept the code and the data but not
  the menu.
- The 18 changed records in `scene_start_list_coop.ssl` suggest the co-op route
  was authored for the full campaign rather than for a handful of stages.

## Source files for this session

The scripts were throwaway (`peinfo.py`, `stringscan.py`, `scan_coop_strings.py`,
`rtti_paths.py`, `ssl_dump.py`, `arc_probe.py`, `mt_tables.py`) plus one Ghidra
script (`CoopScan.java`) that resolves string references and decompiles the
functions that touch them. None of them is needed to follow this file.

---

# Second session: the mode field, the loaders and the first build

Same day, later. Phases 3 (understand), 4 (choose) and the first slice of
phase 5 (build). The game was **not** launched in this part, so everything
below is static analysis plus host-side tests. 1,635 functions were decompiled
and indexed for cross-reference lookups.

## The co-op gate is one int32

The whole "where is co-op switched on?" question reduces to a single field:

```c
mode = *(int32 *)(*(int32 *)(DAT_01169108 + 0x24) + 0x20)
```

| piece | value | how it was confirmed |
|---|---|---|
| `DAT_01169108` | the global `sGameSystems` instance pointer | RTTI name `sGameSystems`; VA 0x01169108 → RVA 0x0D69108 (image base 0x00400000, no ASLR, DllCharacteristics 0x8100) |
| `+0x24` | `sGameSystems::getData()` → the game data pointer | matches the decompiled `getData()` and the `isCoop()` assertion sites |
| `+0x20` | the mode field inside the game data | the field every `isCoop()` / `isRaid()` branch reads |

Mode values:

| value | meaning |
|---|---|
| 0 | campaign (single player) |
| 1 | co-op — the dormant two-player campaign |
| 2 | raid — the online Raid Mode the PC release does expose |

`FUN_004373c0` (the episode-start flow state) reads exactly this expression
and then, in the co-op branch, switches the game system over:

```c
if (*(int *)(*(int *)(DAT_01169108 + 0x24) + 0x20) == 1) {
    ...
    *(undefined4 *)(*(int *)(in_ECX + 0x14) + 0x3c8) = 1;
}
...
if (mode != 1) goto LAB_00437b13;
FUN_0041a900(1);          // switch to the co-op game-system instance
```

The `unique_ptr` member at `+0x14` of that flow state points at the co-op
player/game system in co-op mode, so the second player is created by the
episode-start flow, not by a menu.

## The two functions that read the mode while loading

| function | RVA | what it does |
|---|---|---|
| `FUN_0042bfc0` | `0x0002BFC0` | builds `archive\game\{scene\|scene_coop}\XX\XX_event` and `archive\game\episode\XX\XX_episode` and loads them — **the point where the game picks the campaign or the co-op route** |
| `FUN_0041b710` | `0x0001B710` | loads `archive\game\{scene\|scene_coop}\XX\XX_%02x\XX_%02x_event` and swaps every actor that has a `_coop` resource to the co-op variant |

Forcing the mode to 1 in a detour around these two is what makes the game
load the co-op campaign.

## The mode also picks the save slot

This is the sharpest consequence of the mode override and it is easy to miss.

- `FUN_0041ba70` selects the game-system instance by mode: index **1** (the
  co-op instance) when `mode == 1`, otherwise index **0**. The instances live
  at `(&DAT_0116a344)[index]`.
- `FUN_006d0cb0` uses `this + 0x60 + mode * 0x24` as the per-mode save array.

So a forced co-op session reads and writes a **different save slot** than the
campaign. On a fresh PC save, slot 0 has the campaign progress and slot 1 is
empty. Mirroring slot 0 into slot 1 before the flip (or sharing one slot) is
an open item, and it is the first thing to look at when the co-op campaign is
finally launched and the game appears to "forget" the save.

## The co-op campaign data is complete

Re-checked against the shipped archives of the test copy: `game\scene_coop\`
exists for every episode from `11` to `f1`, together with
`scene_start_list_coop.ssl` and the `coop_table\*.lvt` rules. The co-op route
is an authored campaign, not a leftover subset.

## The input path: XInput (corrected)

> **Correction.** An earlier version of this section said the opposite, that
> the game reads pad state through DirectInput 8 and uses XInput for rumble
> only. That was wrong: it was based on the assumption that ordinal 1 of
> `XINPUT1_3.dll` is `XInputGetState`. The real export table of the shipped
> `xinput1_3.dll` was dumped and says otherwise (see below), and the mod and
> this note were fixed.

Import table of `rerev.exe`:

| import | detail |
|---|---|
| `DINPUT8.dll` | exactly **one** symbol: `DirectInput8Create` (import thunk at RVA `0x0B1E202`) |
| `XINPUT1_3.dll` | by **ordinal** 2, 3, 4 and 5 (thunks at RVA `0x0B1E208`, `0x0B1E20E`, `0x0B1E214`, `0x0B1E21A`) |

The export table of the system `xinput1_3.dll` (32-bit, `SysWOW64`), dumped
from the file:

| ordinal | export |
|---|---|
| 1 | `DllMain` |
| 2 | **`XInputGetState`** |
| 3 | `XInputSetState` |
| 4 | `XInputGetCapabilities` |
| 5 | `XInputEnable` |

So the four ordinals the game imports are exactly
`XInputGetState / XInputSetState / XInputGetCapabilities / XInputEnable`, and
**the game reads pad state through XInput**. DirectInput 8 is used too, but
only for the mouse and the keyboard.

The call sites (all in one small input region, RVA `0x5BD000`–`0x5C1000`):

| call | RVA of the call | the user index it asks for |
|---|---|---|
| `XInputGetState` | `0x005BDB4F` | `[esi+0x10]` of the pad object |
| `XInputGetState` | `0x005BEFF6` | `[esi+0x10]`, guarded by `cmp byte [esi+7], 4` |
| `XInputGetState` | `0x005C0423` | `[esi]`, guarded by `cmp byte [esi-9], 4` |
| `XInputGetState` | `0x005C0555` | `edi`, inside a loop over a table with stride `0x2CC` |
| `XInputGetState` | `0x005C0692` | `edi`, guarded by a byte at `[esi+7]` |
| `XInputSetState` | `0x005BDB67`, `0x005BF00E`, `0x005BF20B`, `0x005C0438` | rumble |
| `XInputGetCapabilities` | `0x005BDDC9` | `[esi+0x10]`, flags `1` = `XINPUT_GAMEPAD` |
| `DirectInput8Create` | `0x005C1F09`, `0x005CB0D7`, `0x005CBE75` | — |

The pattern at the call sites is `push &state; push index; call`, so the user
index is a **per-object field** (`[this+0x10]`), not a constant: the game
asks for "the pad that belongs to this player object". The loop at
`0x005C0555` walks a table of per-pad records with a stride of `0x2CC` bytes
and matches a record against a user index and a device type byte.

**What this buys the mod:** the input seam is a single function, and it can
be hooked through the game's import address table without touching the
game's own code. The mod now does exactly that (`src/game/pad_probe.cpp`):
it hooks `XInputGetState` and logs which user index the game polls. When the
game is finally run in co-op mode, that log answers the decisive question:

- if the game polls index **1** as well as index 0, player 2 is driven by a
  second local pad and the online half only has to replicate *state*;
- if it never polls index 1, the mod has to synthesise player 2's input.

The same hook is where remote input has to go (`setPadOverride()`).

**Still unknown:** how many pad records the game keeps and whether a pad
object for player 2 is created at all in co-op mode.

## Signature verification against the real executable

The mod finds the two loaders by byte signature and refuses to patch
anything that is not unique. Checked against the shipped `.text`:

| signature | matches | RVA of the match | expected |
|---|---|---|---|
| `81 EC 04 02 00 00 53 55 56 8B F1 8B 46 70` (episode loader) | **1** | `0x2BFC0` | `0x2BFC0` ✔ |
| `81 EC 28 02 00 00 53 55 56 8B F1 33 DB` (scene loader) | **1** | `0x1B710` | `0x1B710` ✔ |
| `81 EC 04 02 00 00` (episode prefix only) | 11 | — | ambiguous ✘ |
| `81 EC 28 02 00 00` (scene prefix only) | 2 | — | ambiguous ✘ |

The different stack-frame sizes (`0x204` vs `0x228`) are what make the long
signatures unique. The 6-byte prefix that the inline hook actually overwrites
is *not* unique, which is why the uniqueness check exists.

## The mod, first slice

Repository: `<mod repo>` (its own repo, first commit `082b787`).

- **Injection carrier:** a proxy `dinput8.dll`. The game imports exactly one
  symbol from `DINPUT8.dll`, so a proxy that forwards it is enough to get
  code running inside the game process. Everything else is forwarded to the
  system copy.
- **Hooks:** inline detours at the two loaders, overwriting the 6-byte
  `sub esp, imm32` prologue.
- **Behaviour:** force `mode = 1` before the original call and again after
  it (the loader may reset the mode itself), then a watcher thread re-asserts
  the mode every `coop_mode_poll_ms` (250 ms default) while armed.
- **Deliberate:** mode 2 (Raid) is never touched, and the mode is only
  written once the game data pointer is inside the game image.
- **Guarded:** a signature that matches anything other than exactly once, or
  whose address does not equal the expected RVA, aborts the patch and is
  logged instead.

Build and test run (host-side, no game):

```
=== running tests ===
    Start 1: hook_test ........................   Passed    0.01 sec
    Start 2: iat_test .........................   Passed    0.13 sec
    Start 3: coop_mode_test ...................   Passed    0.12 sec

100% tests passed, 0 tests failed out of 3
```

What the tests prove: the hook framework detours and restores correctly, the
IAT patcher works, the mode-field offsets are the ones the mod uses, the
read/write helpers only touch the mode field, and both loader signatures are
unique in a synthetic `.text` and match the documented prologues.

What the tests do **not** prove: that the game loads a co-op scene, that a
second player appears, or that the save slot behaves. That needs the game.

## The mod, second slice: the pad probe

After the first slice, the second session added `src/game/pad_probe.{h,cpp}`
to the mod: it hooks `XInputGetState` through the game's import address
table (`iat::findImportSlotByAddress` + `iat::patchImportSlot`, so the
game's own code is not touched at all) and logs

- which user index the game polls,
- the button mask, the triggers and both sticks,
- only on a *change*, so the log stays readable.

`setPadOverride()` is the seam for the network half: a callback that can
replace the state the game sees for one user index.

Build and test run after the change:

```
=== running tests ===
    Start 1: hook_test ........................   Passed    0.14 sec
    Start 2: iat_test .........................   Passed    0.12 sec
    Start 3: coop_mode_test ...................   Passed    0.12 sec
    Start 4: pad_probe_test ...................   Passed    0.12 sec

100% tests passed, 0 tests failed out of 4
```

## Still unknown (updated after the third session)

1. **Does the game create the second player at all** when the mode is co-op?
   The episode-start flow suggests yes, but nothing has been run.
2. **How is the second player driven** — a second local pad, or replicated pad
   data? The seam is now known (`XInputGetState`, see above), but whether the
   game polls a second user index in co-op mode is still unknown. The
   `cPlayerPadSyncData` class exists, but it is attached to the Raid-mode
   classes (`cCoopSyncData` → lobby/ready/chara), so it is probably
   raid-only. The campaign co-op of the 3DS original used system link, i.e.
   each system drove its own player locally and the network replicated
   *state*.
3. ~~**Is the co-op game-system instance created up front** or lazily by the
   co-op menu?~~ — **answered in the third session: up front.** Both slots are
   filled from start-up; see
   [the two game-system instances](#the-two-game-system-instances-both-slots-exist-from-start-up).
4. **The save slot** (see above).
5. **The transport.** Not "does one exist" any more — it does
   (`nNetwork::Session*`, `SteamNetworking005`, `WS2_32.dll`). What is still
   open: whether the campaign co-op path can be pointed at a transport we
   control, and whether the Steam session is the only way into a lobby.
6. **Where the co-op menu entry has to be re-added.** The PC interface exposes
   only Raid matchmaking; the co-op lobby states exist in the binary but no
   menu path to them was found yet.
7. **What the two non-`None` `network::PlayMode` values mean**, and which one
   is the campaign co-op. The strings name only `network::PlayMode::None`.

## Guesses (still not verified - listed separately on purpose)

- The campaign co-op of the 3DS original drove player 2 from a second
  system over local wireless (the `systemlink_button` string in the co-op
  menu text), which would mean the PC port expects player 2's input on a
  second local pad and replicates state over the network. That is the most
  convenient case for us, because it splits the problem into "route a pad"
  and "replicate state".
- `cGameSystemDouble` most likely means "the game system that can host two
  players", and the second slot is the partner's world state. The mode
  selects which world the local player is in, which is why the co-op world
  has its own save slot. Not proven - the class name is a guess from the
  leaked RTTI strings, and the second slot is created as a *child* of the
  first one rather than as a peer, which does not fit the "two peers"
  reading.
- The two non-`None` `network::PlayMode` values are probably Raid and
  campaign co-op, with the co-op one being the value the menu writes into
  `sNetworkManage + 0xF4`. Not proven.

## Files produced this session

- `<mod repo>` - the mod (own git repo)
- `<ghidra scripts>/{CoopScan,DecompileByString,Callers,DecompileFuncs,RefFuncs}.java`
- `<ghidra out>/fn/` - 1,635 decompiled functions
- `<ghidra out>/*.tsv` - string/xref/caller indexes

## Third session: the game already ships a netcode

Same day, later part. The plan was to map the two game-system instances and
then work out what the mod still has to build. The mapping turned up something
that changes the design of the whole project:

> **The PC build contains a complete, host-authoritative network layer for
> the campaign co-op.** The mod does not have to write a netcode. It has to
> unlock the one that is already in the binary.

> *Corrected in [session 6](#session-6-the-premise-is-dead---this-game-has-no-campaign-co-op):
> the layer is real and host-authoritative, but it is Raid Mode's. Nothing is
> dormant and there is nothing to unlock.*

Everything below is split into what was **observed** in the shipped
executable, what was **derived** from those observations, and what is still
**unknown**.

### An RVA that was read wrong

Ghidra prints a data symbol with its *virtual address*. `PTR_FUN_0106f0b4` is
the address `0x0106F0B4`, so its RVA is `0x0106F0B4 - 0x400000 = 0x00C6F0B4`,
which is in `.rdata`. An earlier note in this file read it as RVA `0x06F0B4`;
that address is inside `.text` and was being disassembled as code, which is
why the vtable looked like garbage.

The section table of the shipped `rerev.exe` (raw offsets, for the record):

| section | RVA | virtual size | raw offset | raw size |
|---|---|---|---|---|
| `.text` | `0x00001000` | `0x00BBB27B` | `0x00000400` | `0x00BBB400` |
| `.rdata` | `0x00BBD000` | `0x00155A50` | `0x00BBB800` | `0x00155C00` |
| `.data` | `0x00D13000` | `0x000D5BE0` | `0x00D11400` | `0x00055E00` |
| `.rsrc` | `0x00DE9000` | `0x00006228` | `0x00D67200` | `0x00006400` |
| `.bind` | `0x00DF0000` | `0x00029AD8` | `0x00D6D600` | `0x00029AD8` |

With the corrected RVAs the two vtables read as pointers:

| vtable | RVA | entries |
|---|---|---|
| class A | `0x00C6F0B4` | `0x00721B40, 0x00F4E480, 0x00A055F0, 0x0042E750, 0x00402990, 0x004A7850, 0x004A7850, 0x0042E750, …` |
| class B | `0x00C6F158` | `0x007227F0, 0x00F4E480, 0x00A055F0, 0x0042E750, 0x00402990, 0x004A7850, 0x004A7850, 0x0042E750, …` |

They are identical from entry 1 on, which is the classic shape of a derived
class. The slot at `vtable - 4` is **not** a `COL` (there is no MSVC RTTI
there), so the class names could not be recovered from the RTTI; the names
below come from the leaked source paths and from the class names in the
assert messages instead.

### The two game-system instances: both slots exist from start-up

**Observed** (disassembly of `FUN_00723790`, `FUN_007236e0`, `FUN_00721ba0`,
`FUN_00721b40`, `FUN_00722790`):

| class | vtable RVA | constructor | destructor | own fields |
|---|---|---|---|---|
| `cGameSystem` (A) | `0x00C6F0B4` | `FUN_00721ba0(slot)` | `FUN_00721b40` | `m_index` at `+0x20` |
| `cGameSystemDouble` (B) | `0x00C6F158` | `FUN_00723790(desiredIndex)` | `FUN_00722790` | `+0x24` own slot, `+0x28` child pointer |
| class C (derived from B) | `0x00C6F17C` | `FUN_007236e0` | — | size `0x54`, scene object at `+0x40` |

`FUN_00721ba0` is the **only** writer of the instance array
`(&DAT_0116a344)[slot]` (one store site, `mov [edi*4+0x0116a344], esi` at RVA
`0x321C0A`), and it asserts that the slot is free and that
`slot < InstanceCount_` with `InstanceCount_ == 2`
(`cGameSystem.h:0x47`, "index < InstanceCount_"). It has exactly **one**
caller.

The constructor of the derived class, `FUN_00723790`, disassembles to:

```
slot = (instances[desiredIndex] == 0) ? desiredIndex : desiredIndex + 1;
cGameSystem::cGameSystem(slot);            // FUN_00721ba0: registers this
vptr = 0x106f158;                           // cGameSystemDouble
this + 0x28 = 0;
if (instances[desiredIndex + 1] != 0) {      // early return, no child
    this + 0x24 = desiredIndex + 1;
    return this;
}
this + 0x24 = desiredIndex;
child = new (0x54) classC;                  // FUN_007236e0
this + 0x28 = child;
```

and `FUN_007236e0` (the constructor of class C) calls `FUN_00723790(0)` with
`ECX` = its own `this`, so a class C *is* a `cGameSystemDouble` with extra
members.

The manager `FUN_00792020` (it sets `DAT_01169108 = this`, the
`sGameSystems` singleton) creates one class C at `+0x6C`. That instance
registers itself in slot 0 and then creates a child, which registers itself in
slot 1 (slot 0 is taken by then) and takes the early-return path, so it has
no child of its own. The manager's reset function `FUN_00792440` destroys the
instance at `+0x6C` and creates a new one, which refills both slots.

**Derived from the above:** the co-op game system — slot 1 — is already there
when the title screen appears. `FUN_0041a900(1)` therefore does **not** trip
its `"false"` assertion when the mode is flipped to 1. This settles the third
open question of the previous session.

**Derived:** slot 1 is created as a *child* of slot 0, and `+0x24` holds the
slot index whose scene object (`(&DAT_0117cc90)[index]`) the object owns.
So the two slots are not two equal peers; the second one belongs to the
first.

### The network layer that ships with the game

**Observed** (string literals in `.rdata`, cross-checked against the
decompilation):

`network::sNetworkManage` — the netcode singleton, global `DAT_01169398`
(RVA `0x0D69398`). Its constructor is `FUN_00880f10`; it asserts `"false"`
when the singleton already exists, exactly like `sGameSystems`, and it is the
only writer of the global (one store site, RVA `0x480F44`). Its source path
is `prog\game\network\sNetworkManage.cpp`. Fields that the constructor
initialises and that the code reads:

| offset | constructor writes | meaning |
|---|---|---|
| `+0x30` | `0` | flag bits |
| `+0xE4`, `+0xE8` | `0x80011010` | unknown |
| `+0xF4` | `-1` | the play mode the co-op menu writes |
| `+0xF8` | `-1` | the player index |

`isHost()` is used as an assertion all over the code:
`network::sNetworkManage::getInstance().isHost() == false` /
`!= false` appear in `FUN_006734a0`, `FUN_006735d0` and `FUN_00674970`.

`network::sCoopManager` (`prog\game\network\sCoopManager.cpp`,
`FUN_0087edc0`) with the nested classes `cStageInfo`, `cCoopSkill` and
`cSaveProc`; the network manager's constructor builds eight of them
(`FUN_0087dad0` in a loop of eight), i.e. one per player slot.

`cCoopSyncData` (`prog\game\network\cCoopSyncData.cpp`) is the **complete
campaign sync vocabulary**, and every message has a host-authoritative reply
(the `cRe…` twin):

| message | reply | what it carries |
|---|---|---|
| `cCreateTimeSyncData` | `cReCreateTimeSyncData` | session start time |
| `cLobbyInSyncData` | `cReLobbyInSyncData` | a player entered the lobby |
| `cLobbyReturnSyncData` | `cReLobbyReturnSyncData` | a player left the lobby |
| `cModifyStageSyncData` | `cReModifyStageSyncData` | **which episode/stage the partner is in** |
| `cModifyCharaSyncData` | `cReModifyCharaSyncData` | the partner's character |
| `cModifyEquipSyncData` | `cReModifyEquipSyncData` | the partner's equipment |
| `cModifyColorSyncData` | `cReModifyColorSyncData` | the partner's colour |
| `cModifyMissionSyncData` | `cReModifyMissionSyncData` | mission state |
| `cUnlockStageSyncData` | `cReUnlockStageSyncData` | stage unlock |
| `cPresenceSyncData` | `cRePresenceSyncData` | presence |
| `cKickSyncData` | `cReKickSyncData` | kick |
| `cReadyOn/OffSyncData` | `cReReadyOn/OffSyncData` | ready state |
| `cReadySyncEndSyncData` | `cReReadySyncEndSyncData` | ready-up finished |
| `cCutStartSyncData` / `cCutStartEndSyncData` / `cCutEndSyncData` | `cRe…` | cutscene start/end |
| `cArrangeEndSyncData` / `cArrangeSyncEndSyncData` | `cRe…` | enemy arrangement |
| `cContinueSyncData` | `cReContinueSyncData` | continue |
| `cRushStartSyncData`, `cRushReadyKillSyncData`, `cRushLoadEndSyncData`, `cRushArrangeEndSyncData`, `cRushSyncEndSyncData` | `cRe…` | the "rush" (timed) mode |
| `cReadyKillSyncData`, `cResultSyncData`, `cResultEndSyncData` | `cRe…` | results |
| `cDeathEnemySyncData`, `cNetworkDeadSyncData` | `cRe…` | deaths |
| `cWantedSyncData`, `cZZZSyncData` | `cRe…` | misc state |

Per-object sync payloads (`cNetSyncData` subclasses):

| object | payload |
|---|---|
| `uCharaBase` | `cPositionSyncData`, `cDamageSyncData` |
| `sItem` | `cDropItemSyncData`, `cEnemyDropSyncData`, `cPickupItemSyncData` |
| `uItem` | `cGetItemSyncData` |
| `uWeapon` | `cExplodeSyncData`, `cShootDownSyncData` |
| `uDoor2pBase` | `cDoor2pSyncData` |
| `uElevatorBase` | `cElevatorSwitchOnSyncData`, `cElevatorOutOfBlockSyncData`, `cElevatorDoorOpenSyncData` |
| `uCoopChainLock` | `cCoopChainHostCheckSyncData`, `cCoopChainHostAnswerSyncData` — the chain lock is a **host question / host answer** pair |
| `sActionCommand` | `cMediateData`, `cMediateResultData`, `cUnlockData` |
| `cCutRushState` | `cLoadArrangeSyncData`, `cSetArrangeSyncData`, `cActivateEnemySyncData` |

Transport and session layer: `nNetwork::Session`, `nNetwork::SessionDriver`,
`nNetwork::SessionDatabase`, `nNetwork::Route`, `nNetwork::Transport`,
`SteamNetworking005`, and `WS2_32.dll` with a full client and server socket
stack.

### The co-op menu

`prog\game\gui\revel\coop\cCoopMenu.cpp` and `cCoopMenuState.cpp`. The menu
states are: `cStateBase`, `cRaidMenuState`, `cQuickConfig`, `cPlayerMatch`,
`cCreateConfig`, `cCustomMatch`, `cSessionList`, `cLobbyBase`, `cLobby`,
`cWaitOtherReady`, `cCreateSession`, `cSearchSession`, `cShortCutMoving`,
`cEndErrorJump`.

`FUN_007cb7b0` (`cCoopMenuState.cpp`) reads a play mode out of the menu
object at `+0x408` and branches on it:

| value | behaviour |
|---|---|
| `0` | `network::PlayMode::None` → leave the menu (transition to state 6) |
| `1` | a co-op specific path; asks `DAT_01169110` about something (argument 10) and sets `menu + 0x3F2 = 1` |
| `2` | the same, and additionally sets `sNetworkManage + 0xF8 = 0` (the local player is player 0) |
| other | assertion `"mode != network::PlayMode::None"` (`cCoopMenuState.cpp:0x5a3`) |

and finally writes `sNetworkManage + 0xF4 = uVar5` (`0` or `2`).

**Unknown:** the two non-`None` values are not named anywhere in the strings;
Raid and campaign co-op are the obvious candidates but nothing confirms it.

### What this changes for the mod

The previous plan was "write a host-authoritative netcode for the mod". That
is now the wrong plan. The remaining work is:

1. **the mode gate** — done (the mod flips the mode field);
2. **a way into the co-op menu** — the PC interface exposes only Raid
   matchmaking, so the co-op lobby states have to be reachable somehow;
3. **reuse the game's own session and transport** instead of shipping one.

### The mod, second slice of the third session: the co-op state probe

The mod gained `src/game/coop_state.{h,cpp}` (commit `348ece6` in the mod
repo). It **only reads**:

```
mode           = *(int32  *)((uint8 *)gameSystems->getData() + 0x20)
instances      = (uint32_t *)(imageBase + 0x0D6A344)   // two slots
netManager     = *(uint32_t **)(imageBase + 0x0D69398) // network::sNetworkManage
netPlayMode    = *(int32  *)(netManager + 0xF4)
netPlayerIndex = *(int32  *)(netManager + 0xF8)
netFlags       = *(uint32 *)(netManager + 0x30)
```

and logs a line whenever one of them changes:

```
coop state: mode=1 instances=2/2 coop_instance=1 net=playmode=-1 player=-2 flags=00000000
```

`playmode` stays `-1` while the network manager has never started a session,
`player` is `-2` before the manager exists. If the mode flip alone moves
`playmode` away from `-1`, the game starts a session by itself and the missing
piece is only the menu entry; if it does not, the netcode needs to be woken up
by the mod.

The same commit **guards the mode flip**: the game reads instance slot 1 when
the mode is co-op and asserts `"false"` when that slot is empty, so the mod
now checks that the co-op instance is there before it writes the mode, and
says so in the log instead of writing. The check is redundant for this build
(both slots exist), but the mod must not depend on that.

Build and test run after the change:

```
=== running tests ===
    Start 1: hook_test ........................   Passed    0.14 sec
    Start 2: iat_test .........................   Passed    0.13 sec
    Start 3: coop_mode_test ...................   Passed    0.13 sec
    Start 4: pad_probe_test ...................   Passed    0.12 sec
    Start 5: coop_state_test ..................   Passed    0.13 sec

100% tests passed, 0 tests failed out of 5
```

What the new test proves: the instance slots are read without running off
the end of the array, the co-op instance is detected as slot 1, the network
manager fields are read at the documented offsets, a missing manager is not
read at all, and the log line is formatted safely. What it does **not**
prove: that the game reacts to the mode flip. That needs the game.

### Still open after this session

- the game has to be run at least once with `coop_state = 1` and
  `pad_probe = 1` to answer: does the co-op path react to the mode flip, does
  a second pad index get polled, and does a session ever start;
- the missing menu entry has to be found (which function builds the "Online"
  entry and why only Raid is offered);
- the save-slot question (slot 0 vs slot 1) is still open and only shows up
  in a real run.

## Fourth session: the proxy DLL did not export the name the game imports

The first launch attempt of the test copy failed before the game window
appeared, with a loader error box:

```
rerev.exe - Entry Point Not Found
The procedure entry point DirectInput8Create could not be located in the
dynamic link library D:\...\rev1\rerev.exe.
```

### What it was

**Observed** (parsed the export directory of the built proxy with a small
PE reader, `_exports.py` in the mod repo during the session):

```
exports: [('_DirectInput8Create@20', 1), ('_DllCanUnloadNow@0', 2),
          ('_DllGetClassObject@12', 3), ('_DllRegisterServer@0', 4),
          ('_DllUnregisterServer@5', 5)]
```

The proxy exported `_DirectInput8Create@20` — the MSVC **decorated**
`__stdcall` name — while the game's import table asks for the undecorated
`DirectInput8Create` (confirmed in the first session: the game imports exactly
one symbol from `DINPUT8.dll`, at RVA `0x0B1E202`). The loader could not bind
the import, so it reported the entry point as missing *in the executable*
(the loader names the module that has the unresolved import, not the DLL that
failed to provide it — which is why the message points at `rerev.exe`).

Note that the *string* `DirectInput8Create` was present in the built DLL the
whole time; a plain `bytes.find()` check said "found" and told us nothing. Only
reading the export **name table** shows the decoration. That is why the fix is
checked by resolving the export, not by searching the file.

### The fix

A module-definition file pins the five exports to their undecorated names:

```
LIBRARY dinput8
EXPORTS
    DirectInput8Create
```

wired into CMake (MSVC only) with

```cmake
target_link_options(dinput8 PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/dinput8.def")
```

The export table now reads `DirectInput8Create` (ordinal 1), and the
`dinput8.def` file is tracked in the mod repo.

### The regression test

`tests/export_check.cpp` loads the freshly built proxy and calls
`GetProcAddress(module, "DirectInput8Create")` — exactly what the game's import
binder does. It is a ctest target that depends on the DLL target, so a
recurrence fails the build:

```
    Start 6: export_check
6/6 Test #6: export_check .....................   Passed    0.12 sec

100% tests passed, 0 tests failed out of 6
```

What it proves: the built DLL exports `DirectInput8Create` by name and the
symbol resolves through the normal loader. What it does **not** prove: that the
game starts with the proxy loaded — that still needs the game run.

### Note for anyone else proxying a system DLL for this game

The same trap applies to any `__stdcall` export (`DllGetClassObject`,
`DllCanUnloadNow`, …). If a proxy for this game builds and the game refuses to
start with an entry-point error, check the **export name table**, not the
presence of the string. `dumpbin /exports <dll>` (or the PE reader above) shows
the decoration directly.

## Fifth session: the proxy crashed the game (two independent defects)

With the export name fixed, the loader bound the import and the game reached
`DirectInput8Create` — and then died with a fast-fail, every single time:

```
Application Error
  Faulting application: rerev.exe          (PE timestamp 0x65a78e7e)
  Faulting module:    DINPUT8.dll          (PE timestamp 0x6ad9aa84)
  Exception code:     0xc0000409
  Fault offset:       0x0000f5ce
```

`0xc0000409` is `STATUS_STACK_BUFFER_OVERRUN`, which on modern Windows is what
`__fastfail` raises. The disassembly of the failing routine (Ghidra, project
`re1coop`) shows the classic fast-fail stub:

```
push 0x17            ; IsProcessorFeaturePresent
call [IsProcessorFeaturePresent]
push 5               ; FAST_FAIL_GUARD_ICALL_CHECK_FAILURE
pop  ecx
int 0x29
```

`ecx = 5` is `FAST_FAIL_GUARD_ICALL_CHECK_FAILURE`, i.e. **a Control Flow Guard
check rejected an indirect call**. Two separate defects were behind it, and
both had to be fixed.

### Defect 1: the proxy was a broken CFG image

**Observed** (parse of the load config of the built DLL, x86 layout:
`SEHandlerTable` +0x40, `SEHandlerCount` +0x44, `GuardCFCheckFunctionPointer`
+0x48, `GuardCFDispatchFunctionPointer` +0x4C, `GuardCFFunctionTable` +0x50,
`GuardCFFunctionCount` +0x54, `GuardFlags` +0x58):

```
load config: rva=0x360b8 size=0x40
  GuardCFCheckFunctionPointer    +0x48 = 0x1002b184
  GuardCFFunctionTable           +0x50 = 0x1002b1f4
  GuardCFFunctionCount           +0x54 = 0x000000b7
  GuardFlags                     +0x58 = 0x10017500
```

The interesting part is what happened *before* the fix: the linker still set
`IMAGE_GUARD_CF_INSTRUMENTED` in `GuardFlags` (`0x100`), but dropped the
`.gfids` data, so the image ended up with

```
  GuardCFFunctionTable           +0x50 = 0x00000000
  GuardCFFunctionCount           +0x54 = 0x00000000
```

— an **empty valid-target table** on an image that claims to be instrumented.
Every guard check that resolved a target inside the DLL then failed, and the
process died before the first window appeared.

**Why the linker did that:** the static MSVC CRT (`libcmt.lib`) ships
pre-compiled with `/guard:cf`. It carries 50 `.gfids` entries and 26
references to `__guard_check_icall_fptr`. Linking those objects *without*
`/guard:cf` on the link line produces exactly the broken image above.

**The fix** is one link option:

```cmake
target_link_options(dinput8 PRIVATE "/guard:cf")
```

After it, the load config of the proxy reads `GuardCFFunctionTable = 0x1002b1f4`,
`GuardCFFunctionCount = 0xb7`, `GuardFlags = 0x10017500` — a consistent image.

Note for the reader: MSVC's x86 linker always writes `0x40` into the load
config **data-directory** size while the structure's own `Size` field says
`0xc0`. The directory size is therefore useless for validating a CFG image;
the structure's internal `Size` is the field to check.

### Defect 2: the logger used the CRT stream layer

With the CFG table populated the crash moved, but it did not go away: the fault
offset changed (`0x0000f5ce` → `0x0000f66e`) and the exception code stayed
`0xc0000409`.

**Observed:** the log file the proxy wrote was exactly **3 bytes** long — the
UTF-8 BOM that `_wfopen_s` writes when it opens a stream with `ccs=UTF-8`.
Nothing else ever reached the file, so the crash happened between the open and
the first `fprintf`.

**Root cause** (standalone repro in `iop_test.c`, six variants, built with
`/guard:cf` and a custom invalid-parameter handler so the CRT reports instead
of aborting):

```
variant 1: _wfopen_s + fflush + fclose        -> fine
variant 2: + fprintf("... %ls ...")           -> INVALID PARAMETER
variant 3: + fprintf("... %s ...")            -> INVALID PARAMETER
variant 4: + setvbuf(_IOLBF)                  -> fine
variant 5: + setvbuf(_IOFBF)                  -> fine
variant 6: + _snwprintf_s                     -> fine
```

So it is not the format string and **not** `setvbuf` (the `_IOLBF` value is
accepted by this CRT — that hypothesis was wrong and was dropped). It is
`fprintf` itself: `_wfopen_s` returns a **wide-oriented** stream, and a narrow
`fprintf` on it makes the CRT raise an invalid parameter.

And *that* is fatal here, because the CRT reports an invalid parameter by
calling through a function pointer — and that pointer is not a legal CFG
target in this legacy, non-CFG host process. The report therefore ends in
`__fastfail(FAST_FAIL_GUARD_ICALL_CHECK_FAILURE)` → `int 0x29` →
`0xc0000409`. The Ghidra decompile of the failing CRT helper shows it
literally:

```
esi = *(code **)(obj + 0x35c);
mov  ecx, esi;
call dword ptr [PTR_guard_check_icall_1002c184];
call esi;
```

**The fix:** the logger no longer uses the CRT stream layer at all. It opens
the file with `CreateFileW` and writes with `WriteFile`; only the *formatting*
still uses the CRT (`_vsnprintf_s`), which is pure string building and has no
invalid-parameter behaviour of its own.

```cpp
g_file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
```

This also removed the awkward wide/narrow mix (`%ls` in a narrow `fprintf`)
that the old code had.

### The regression tests

Two ctest targets were added, so neither defect can come back unnoticed:

- `tests/log_test.cpp` — writes a real log line with the production logger and
  reads the file back, checking the banner and the formatted line.
- `tests/load_config_check.cpp` — parses the built proxy's load config and
  fails when the image claims to be CFG instrumented but carries an empty
  valid-target table (the defect-1 shape).

```
    Start 1: hook_test
1/8 Test #1: hook_test ........................   Passed    0.01 sec
    Start 2: iat_test
2/8 Test #2: iat_test .........................   Passed    0.01 sec
    Start 3: coop_mode_test
3/8 Test #3: coop_mode_test ...................   Passed    0.01 sec
    Start 4: pad_probe_test
4/8 Test #4: pad_probe_test ...................   Passed    0.01 sec
    Start 5: coop_state_test
5/8 Test #5: coop_state_test ..................   Passed    0.01 sec
    Start 6: export_check
6/8 Test #6: export_check .....................   Passed    0.11 sec
    Start 7: log_test
7/8 Test #7: log_test .........................   Passed    0.14 sec
    Start 8: load_config_check
8/8 Test #8: load_config_check ................   Passed    0.12 sec

100% tests passed, 0 tests failed out of 8
```

The `export_check` test used to be flaky (~3 crashes in 30 runs, always
`0xc0000409`): the proxy's init thread called `fprintf` while the test binary
was already tearing down. After the logger rewrite, 40 consecutive runs with
zero crashes.

### The game still would not start: the Steam app id

With the crash gone, the game started, initialised the mod completely and then
**exited after half a second, without creating a window**.

**Observed** (control experiment, same folder, same command, twice):

```
RUN A: baseline, no mod DLL  -> PROCESS EXITED (0.5 s, no window)
RUN B: with the fixed mod DLL -> PROCESS EXITED (0.5 s, no window)
```

The exit is the game's own behaviour when it is launched directly, **with or
without the mod** — it is not caused by the mod. The game is a Steam title and
its Steam API needs the app id; when the process is started outside the Steam
client the API init fails and the game quits.

**The fix for a direct launch:** a `steam_appid.txt` file containing the app id
next to the executable:

```
222480
```

With that file present, the same command that produced a 0.5-second exit now
produces a running game:

```
window: visible=True  class=B title=R
window: visible=False class=I title=D
window: visible=False class=M title=M
window: visible=True  class=D title=D
STILL RUNNING pid=9844 ws=172.5MB responding=True
```

Launching through Steam needs no such file — the Steam client exports
`SteamAppId` into the game process itself.

### The game with the mod, running

The full soak run (90 s) on the test copy, mod loaded, no crash:

```
launched pid=9072 at 15:32:20
STILL RUNNING after 90 s: pid=9072 ws=314.7MB responding=True
```

and the mod's log:

```
---- re1coop start ----
re1coop loaded in pid 9072 (enabled=1 role=auto host=127.0.0.1 port=21500)
coop mode: game image at 00400000, size 00e1a000, entry 00df0310
coop state: instances at 0116a344, scenes at 0117cc90, network manager at 01169398
coop mode: hooked the episode loader at 0042bfc0
coop mode: hooked the scene loader at 0041b710
coop mode: watcher started (always=0 hold=1 poll=250ms)
pad probe: XInputGetState hooked at 019D2850 (ordinal 2 of xinput1_3.dll)
coop state: sampler started (poll=500ms)
coop state: mode=-1 instances=0/2 coop_instance=0 net=playmode=-2 player=-2 flags=00000000 session=0 state=0 status=-2
coop state: mode=-1 instances=2/2 coop_instance=1 net=playmode=-2 player=-2 flags=00000000 session=0 state=0 status=-2
```

So the game boots, both game-system instances come up, and the state probe
sees the co-op instance become active. The netcode is still idle
(`playmode=-2`, `session=0`), which is correct for a game sitting on its
title screen.

### Still open after this session

- the co-op path itself is still unverified: no session has been established
  yet, so the second player is never created, XInput index 1 is never polled
  and the menu entry is still missing;
- `steam_appid.txt` is a **direct-launch** convenience; when the game is started
  from Steam instead, the two mod files have to be copied next to `rerev.exe`
  first;
- an early, crashing build of the proxy DLL is dangerous to leave in an install
  (`DINPUT8.dll`, PE timestamp `0x6ad9aa84`): launching that copy crashes with
  `0xc0000409` at fault offset `0x0000f5ce`. It does not ship with the game and
  has to be removed.

### Note for anyone else proxying a system DLL on a legacy x86 game

Two traps, both of them silent and both fatal, and neither visible in the
build output:

1. **link the proxy with `/guard:cf`** if any statically linked CRT is
   CFG-instrumented. Without it the image is marked instrumented with an empty
   valid-target table and every indirect call inside the DLL is rejected.
2. **do not mix the CRT stream orientations.** A narrow `fprintf` on a stream
   from `_wfopen_s` raises an invalid parameter, and reporting an invalid
   parameter is itself an indirect call that a non-CFG host cannot validate.
   Use the Win32 file API inside the injected DLL and keep the CRT for string
   building only.

Both traps are covered by ctest targets now, so a repeat is a red test rather
than a dead game.


---

## Session 6: the premise is dead — this game has no campaign co-op

**Correction. Everything above that describes a "dormant two-player campaign
behind a mode switch" is wrong.** It was an inference from decompiled
branches and asset names, never checked against the running game. This
session checked it, by playing the game and reading its memory.

### Evidence, in order of decisiveness

1. **The main menu has exactly three entries.** Driven with injected keys and
   read back with OCR: `КАМПАНИЯ`, `РЕЖИМ «РЕЙД»`, `ПАРАМЕТРЫ`, and the
   selection wraps after three `DOWN` presses. There is no co-op entry. The
   description shown for `РЕЖИМ «РЕЙД»` is literally "Режим совместного
   прохождения, в котором вы боретесь с врагами разной сложности." — the
   shipped co-op mode is Raid Mode.
2. **The campaign pause menu has no invite path:** `ЗАГРУЗКА`,
   `ПАРАМЕТРЫ`, `ВЫЙТИ`.
3. **The mode field reads `1` while Raid Mode runs.** Measured with the
   `re1coop` probe (after fixing the bug below) at the menus and in Raid Mode:

   ```
   coop state: mode=0 instances=2/2 coop_instance=1 net=playmode=-1 player=-1 ...
   coop state: mode=1 instances=2/2 coop_instance=1 net=playmode=-1 ...   <- Raid lobby
   coop state: mode=1 instances=2/2 coop_instance=1 net=playmode=0  ...   <- Raid lobby
   ```

   So `0` = campaign/neutral, `1` = Raid Mode. The old enum
   (`1 = co-op`, `2 = raid`) is wrong. No other value was observed.

### The bug that hid all of this

`coop_mode.cpp::currentGameData()` and `coop_state.cpp::currentMode()`
walked the chain correctly and then rejected the result with an
image-range test. The globals are in the image; the objects are on the heap:

```
[0x01169108] = 0x220A6460        (sGameSystems, heap)
0x220A6460 + 0x24 -> 0x220A6570  (game data, heap)
0x220A6570 + 0x20 -> 1           (the mode, while Raid Mode is running)
```

Consequence: `mode=-1` in every log was the mod failing to look, not the game
"not having picked a mode yet", and the mode write **never executed** — the
"override" was a no-op for the whole project. Pointer validity is now
`readableMemory()` (`VirtualQuery` → committed, not `PAGE_NOACCESS`/`PAGE_GUARD`).

### What changed in the mod

* pointer checks use readability, not image range (mode is now readable:
  `0` at menus, `1` in Raid Mode; confirmed in a live game);
* `forceCoopMode()` is a deliberate no-op with a log line: writing `1` into a
  campaign would send the engine to Raid scene data for a campaign chapter;
* `layout.h` enum corrected to `kModeCampaign = 0`, `kModeRaid = 1`, and all
  comments that asserted a dormant campaign co-op rewritten;
* `coop_mode` in the `.ini` now only enables the (read-only) probe;
* README rewritten to state the result instead of the hypothesis;
* `ctest`: 8/8 passing; game boots, reaches Raid Mode, plays, and the save
  file is byte-identical (SHA-256) before and after.

### Root cause of the wrong turn (for the playbook)

`game\scene_coop\` mirrors campaign episode ids (`01`…`f1`), and the binary
really does branch on the mode in dozens of places, including functions that
swap `_coop` actors. From static analysis alone that looks exactly like a
disabled campaign mode. It is not: those ids are the stage ids Raid Mode uses,
and the mode branch is the one the game takes *today* when you play Raid Mode.
An asset route existing proves the route is used, not that it is dormant.

**Rule to carry forward: before a "there is a hidden mode behind one field"
hypothesis gets a mod, drive the game's own UI and read the field while the
game is running. Both checks cost about an hour; this hypothesis cost five
sessions.**

### Still open

* whether to publish this at all, now that the premise is dead;
* direction: stop, or pivot to Raid Mode co-op QoL (the game's online Raid
  Mode already works; the open question is direct-IP/relay instead of Steam
  matchmaking).

## Seventh session: the type registry - the game's own replication extension point

Session date: 2026-10-22. Goal for this session: a co-op mod for the
**campaign**, built **from scratch**, on the **network code that already ships
in the game**. The previous session had found the netcode but
not the place where a mod can plug into it. This session found it, and proved
it works in the running game.

### The short version

The game has a **type registry**: 4071 registered types in one tree, filled
at start-up from static initialiser blocks, with inheritance, a hashed type
id, and a class id that says how instances of the type are created and
serialised. The game's own sync payloads (`cZZZSyncData`, `cMediateData`,
`cPositionSyncData`, ...) are entries in that tree.

**The mod can add its own entry to that tree, and the game treats it like one
of its own.** This was verified in the running game, not inferred: the mod
registered `re1coop::cCampaignSyncData` (32 bytes) under the game's own sync
payload parent, and the game accepted it, gave it a type id, and the mod
found it again by name.

That is the extension point the whole mod is built on.

### The registration function

`FUN_008E1330`, `thiscall`, `ecx` = the descriptor:

```
FUN_008E1330(descriptor, name, parent, size, explicitId, flag29, flag23)
```

The game calls it from a static initialiser block of this shape (read out of
`.text`, e.g. at `0x00F996BD`):

```asm
push 0                 ; flag23   (bit 23)
push 0                 ; flag29   (bit 29)
push 0                 ; explicit id -> 0 means "use the hash of the name"
push 8                 ; size in bytes
push 011E05C0          ; parent descriptor
push 01090CDC          ; "cCoopSyncData::cZZZSyncData"
mov  ecx, 011838F0     ; the descriptor of this type
call 008E1330
push 00FB7CF0          ; a destructor for the type
mov  dword [011838F0], 01090104   ; the class vtable of this type
call 00F29F91          ; _atexit(dtor)
```

### The descriptor layout

Every registered type has a static object in `.bss`. The layout was read out
of the shipped executable and cross-checked against the decompilation:

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

Rules, all confirmed by the decompilation:

* `FUN_008E1220` keeps the **children of a type sorted by name** (insertion
  sort on the name at `+0x04`, sibling at `+0x08`);
* `FUN_008E1300` **propagates the flag bits** from a type down to its
  children (mask `0x1f800000`, i.e. bits 23..28);
* `FUN_008E1660` is the **class-record lookup**:
  `(&DAT_011859D0)[(desc[0x18] >> 23) & 0x3f]`. The 6-bit class id selects
  one of 64 class records. **The class id is the memory domain the instances
  live in** - the game uses it that way: `MtMemoryAllocator::GLOBAL` is class
  1, `UI` is class 7, `UNIT` is class 12, `SOUND` is class 14, and the
  network domain is **class 16**, which is where every sync payload lives
  (corrected in the eighth session; the first reading said 0).

### The tree, measured in the running game

The root global at `0x0114D5CC` stays empty, so the mod finds the top of the
tree by walking up from a descriptor it knows. The tree then reads:

```
net registry: 4071 types reachable from 'MtObject' (54 nNetwork::, 137 *SyncData, 14 cCoopMenu::)
net registry:   root 'MtObject' size=4 class=0 id=2ea10ceb
net registry:   child 'CoprocessorObject' size=2180 class=0 id=6d5941c5
net registry:   child 'MtAllocator' size=92 class=0 id=5f813cb8
net registry:   child 'MtMap' size=17424 class=3 id=29429698
net registry:   child 'MtObject_Game<MtMemoryAllocator::NETWORK>' size=4 class=16 id=6fab1f0b
net registry:   child 'MtObject_Game<MtMemoryAllocator::GLOBAL>' size=4 class=1 id=27b868ab
net registry:   child 'MtObject_Game<MtMemoryAllocator::UI>' size=4 class=7 id=128bd5b3
net registry:   child 'MtObject_Game<MtMemoryAllocator::UNIT>' size=4 class=12 id=11b1e319
net registry:   child 'MtObject_Game<MtMemoryAllocator::SOUND>' size=4 class=14 id=100c91d8
...
```

`MtObject` is the root of the MT Framework's own object hierarchy, and the
network, AI, GUI and collision types all hang off it. This is the whole
object model of the game, registered in one place.

The types the mod cares about:

```
net registry: 'nNetwork::Session'          size=5072  class=16 parent='MtObject'
net registry: 'nNetwork::SessionDatabase'  size=44424 class=16 parent='MtObject'
net registry: 'nNetwork::SessionDriver'     size=1288  class=16 parent='MtObject'
net registry: 'nNetwork::SessionListener'   size=4     class=16 parent='MtObject'
net registry: 'cCoopSyncData::cZZZSyncData' size=8    class=16 parent='cRemoteCall'
net registry: 'sActionCommand::cNetSyncData::cMediateData' size=12 class=16 parent='cRemoteCall'
net registry: 'cCoopMenu::cLobby'           size=36    class=3 parent='cCoopMenu::cLobbyBase'
```

**Correction to the third session:** the parent of the per-object sync
payloads is called **`cRemoteCall`**, not `cNetSyncData`. Its descriptor sits
at `0x011E05C0` (RVA `0x00DE05C0`), size 8, class 16, type id `0x4cd24d24`.

### The session API, decoded

The type descriptors of the session classes, with their vtables:

| type | descriptor | size | vtable |
|---|---|---|---|
| `nNetwork::Session` | `0x011E23DC` | 0x13D0 (5072) | `0x010D20B8` |
| `nNetwork::SessionDatabase` | `0x011E0A14` | 0xAD88 (44424) | `0x010C6580` |
| `nNetwork::SessionDriver` | `0x011E239C` | 0x508 (1288) | `0x010D1FA0` |
| `nNetwork::SessionListener` | `0x011E23BC` | 4 | `0x010D2094` |

The `nNetwork::Session` vtable is followed, in the same 16-byte-aligned
region, by the strings

```
KSP3=PROTOCOLSYSTEM
KPP3=PROTOCOL_COMM_ON
```

Those are the **Valve Source Engine** protocol identifiers
(`PROTOCOL_SYSTEM` / `PROTOCOL_COMMON`). The game ships a Source-style
netchannel stack on top of `ISteamNetworking` (SteamNetworking005), together
with a full `WS2_32` client and server socket stack. Its constructor
(`FUN_00BE2C30`) is a normal object constructor: it allocates through the
class record's allocator, sets the instance vtable, and initialises the
session's own state.

The send path:

```
FUN_00882FD0(session, descriptor, size, flags)   // the game's own send
    -> FUN_009D59C0()                            // lock
    -> FUN_009CDE40(*(descriptor + 0x28), size, flags)
         if (*(char *)(target + 0x14) && size)  // "connected" byte
             FUN_00A11030(flags, 0xC0, flags2, size)
    -> unlock

FUN_00A11030(...)   // builds the packet: buffer of 0x800 bytes, serialises
                    // the payload, then FUN_00BE2900 hands it to the channel
```

### The mod: milestone 1

New module `src/game/net_registry.{h,cpp}` in the mod repo. It reads the
game's registry, finds a type by name, and registers one type of the mod's
own. New module `src/game/net_probe.{h,cpp}` drives it and samples the
network manager. Nine ctest cases cover the helpers on a synthetic tree that
has the same shape as the game's.

The measured result in the running game:

```
net registry: register=008e1330 class-record=008e1660 root-slot=0114d5cc
net registry: 4071 types reachable from 'MtObject' (54 nNetwork::, 137 *SyncData, 14 cCoopMenu::)
net registry: the sync payload parent is 'cRemoteCall' (size=8 class=16 id=4cd24d24)
net registry: registering 're1coop::cCampaignSyncData' with class=16
net registry: registered 're1coop::cCampaignSyncData' size=32 class=16 id=57fe3c6a
net registry: 're1coop::cCampaignSyncData' found at 68c0c1bc (id=57fe3c6a, parent='cRemoteCall')
net registry: 're1coop::cCampaignSyncData' shares the class record of its parent at 058ceac0
net registry: allocated an instance of 're1coop::cCampaignSyncData' at 215f8370
net session: manager=21648f30 session=00000000 playmode=-1 player=-1 flags=00000000 status=0
```

The game booted, reached the main menu, kept running, and the save file is
byte-identical (SHA-256 `037EC948...EDC`) before and after.

The last line is the second important measurement: **in the campaign the
network manager exists but has no session** (`session=0`, `playmode=-1`).
That is the gap the mod has to close.

### The bug that crashed the game this session

The proxy crashed with `0xc0000005` in `DINPUT8.dll`, fault offset
`0x00017d90`, two seconds after launch. Cause: **a virtual address was
converted to an RVA by hand and a hex digit was lost.**

The game is loaded at `0x00400000` with no ASLR, so
`rva = va - 0x00400000`. Written out:

```
0x011E05C0 -> 0x00DE05C0     not 0x000E05C0
0x0114D5CC -> 0x00D4D5CC     not 0x0004D5CC
0x011859D0 -> 0x00D859D0     not 0x000459D0
```

With the wrong RVA the mod read a *code* address as a type descriptor and
then walked the pointers in it. The game died.

Two fixes, both of them general and not about this one constant:

1. **The registry walk validates every descriptor before it follows it.**
   `validDescriptor()` checks that the descriptor is readable, that its name
   pointer is readable byte by byte, and that the name is printable ASCII.
   The walk also has a depth limit (32) and a node budget (4096), so a
   half-initialised or corrupted registry cannot take the process down.
   `safeTypeName()` is used wherever a name is printed.
2. **The RVAs are written out in one place** (`net_registry.h`, namespace
   `rva`), each with the virtual address in a comment, so they can be checked
   against the binary by eye.

**Rule to carry forward: an address in a mod must be derived, not typed.**
Every constant is `va - 0x00400000`, written next to the virtual address it
came from.

### What this changes for the plan

The plan is now concrete, in the mod repo's README:

1. **a session in the campaign** - drive `nNetwork::SessionDriver` /
   `nNetwork::Session` the way the Raid lobby does, but from the campaign.
   Success criterion: `sNetworkManage` reports a live session while a
   campaign chapter is running.
2. **the partner player** - the game already replicates characters
   (`uCharaBase::cNetSyncData::cPositionSyncData`, `cDamageSyncData`); in Raid
   the second player is a second `uCharaBase` spawned by the co-op menu.
3. **the campaign glue** - our own entry point, our own payload types
   (registration is proven to work), and save ownership.

Fallback if the campaign's character sync turns out to be gated on
`mode == 1`: run the campaign chapters inside the shipped co-op engine, with
the campaign's chapter selection feeding the co-op stage list. The netcode,
the sync classes and the spawning are then entirely the shipped ones and only
the flow is ours.

### Still open after this session

* the session creation path (`cCoopMenu::cCreateSession` / `cSearchSession`)
  still has to be found and called from the campaign - this is the next
  milestone;
* the partner player's spawn path has to be found;
* two-client verification needs a second Steam peer and a second Steam
  account; the DRM stub kills a second local instance, so this was not
  attempted;
* the branch `game/resident-evil-revelations` carries this write-up and
  PR #126 is open for review.

## Eighth session: the right class, and the allocator that crashed on a wrong calling convention

The seventh session ended with the mod's type registered, but with `class=0`.
That was wrong, and it was wrong in a way that only matters later: the class
id chooses the memory domain, and a type in the wrong class would have its
instances allocated from the wrong heap. This session fixed it, decoded the
allocation path, and got the game to hand out an instance of a type the mod
registered itself.

### The class id is 16, not 0

`typeClass()` masked bits 23..29 instead of 23..28, and the earlier log was
read as `class=0` for the network types. With the six-bit mask the real
values are:

```
net registry:   child 'MtObject_Game<MtMemoryAllocator::NETWORK>' size=4 class=16
net registry:   child 'MtObject_Game<MtMemoryAllocator::GLOBAL>'  size=4 class=1
net registry:   child 'MtObject_Game<MtMemoryAllocator::UI>'      size=4 class=7
net registry:   child 'MtObject_Game<MtMemoryAllocator::UNIT>'    size=4 class=12
net registry:   child 'MtObject_Game<MtMemoryAllocator::SOUND>'   size=4 class=14
net registry: 'nNetwork::Session'          size=5072  class=16 parent='MtObject'
net registry: 'nNetwork::SessionDatabase'  size=44424 class=16 parent='MtObject'
net registry: 'nNetwork::SessionDriver'    size=1288  class=16 parent='MtObject'
net registry: 'nNetwork::SessionListener'  size=4     class=16 parent='MtObject'
net registry: 'cRemoteCall'                size=8     class=16
net registry: 'cCoopSyncData::cZZZSyncData' size=8    class=16 parent='cRemoteCall'
```

So **NETWORK = 16**, and every sync payload, `cRemoteCall` and the session
classes live in it. The class ids are the `MtMemoryAllocator` domains, not
arbitrary numbers.

### The class is assigned at run time, not at registration

Every registration block in the game pushes three zeroes before the size,
the parent and the name:

```
6a 00 6a 00 6a 00   push 0   ; classId
                    push 0   ; flag29
                    push 0   ; explicitId
push <size>; push <parent>; push <name>; mov ecx,<desc>; call 008E1330
```

`FUN_008E1330` writes bits 23..28 from the seventh argument, which is 0 at
that point. The real class arrives later: `FUN_008E18D0` / `FUN_008E1990`
walk the parent chain, call `ancestor->vtable[3](ancestor->name)` until one
of them returns something other than `0x7fffffff`, and write that value into
the child's bits 23..28. `FUN_008E1300` then propagates a type's class down
to its children (mask `0x1f800000`).

Consequence for the mod: a type registered from the proxy **after** that
pass never gets a class unless it is passed explicitly. `registerType()` now
takes the class as an argument and the probe passes `typeClass(parent)` -
the parent is `cRemoteCall`, so the mod's type is class 16 as well. The
verification checks both the class and that the type resolves to the very
same class record as its parent.

### The allocator path

The class record is a heap object; its vtable is `0x0109D1A8`. The slots
that matter:

```
record->vtable[6]   the real alloc(size, flags, typeId, 0)
record->vtable[7]   alloc(this = record, size, 0x10, typeId)   __thiscall
record->vtable[8]   alloc(size, flags)                          raw
record->vtable[12]  free(ptr)
```

`FUN_00878000` is the game's own creator of a sync payload:

```c
piVar1 = FUN_008e1660(&DAT_01183b30);                    // class record
puVar2 = (*piVar1->vtable[7])(8, 0x10, DAT_01183b4c);    // alloc, this = record
if (puVar2) {
    FUN_009ccd80();        // *instance = &PTR_LAB_010c4b9c; instance[1] = 0
    puVar2[1] = 2;
    *puVar2 = &PTR_LAB_010904b8;   // the instance vtable of that type
}
```

### The crash: a `__thiscall` called as `__cdecl`

The first version of `createTypeInstance()` called `vtable[7]` as a plain
function. The slot is a `__thiscall` method whose `this` is the class
record; with `ecx` left holding whatever the compiler had there, the method
dereferenced garbage and the game died about six seconds in:

```
Id 1000, rerev.exe, 0xc0000005
faulting module: unknown, offset 0x2A28
faulting address: 0x1DD621B8A37C411
```

The faulting address is a stack-shaped value used as a call target - the
signature of a call through a `this` pointer that was never set.

The fix is one typedef: the allocator is
`void* (__thiscall*)(void* self, uint32_t size, uint32_t flags, uint32_t typeId)`
and is called with the class record as `self`. A second, independent guard
was added at the same time: the slot is only called when it points into the
game image, so a class record that is not fully built yet cannot be called
by accident.

### Verified in the running game

```
net registry: the sync payload parent is 'cRemoteCall' (size=8 class=16 id=4cd24d24)
net registry: registering 're1coop::cCampaignSyncData' with class=16
net registry: registered 're1coop::cCampaignSyncData' size=32 class=16 id=57fe3c6a
net registry: 're1coop::cCampaignSyncData' found at 68c0c1bc (id=57fe3c6a, parent='cRemoteCall')
net registry: 're1coop::cCampaignSyncData' shares the class record of its parent at 058ceac0
net registry: class record 058ceac0, vtable 0109d1a8
net registry: allocated an instance of 're1coop::cCampaignSyncData' at 215f8370
net session: manager=21648f30 session=00000000 playmode=-1 player=-1 flags=00000000 status=0
```

The game boots, reaches the main menu and keeps running with an instance of
the mod's own type allocated from the game's NETWORK heap. Nine ctest cases
pass.

### What this proves and what it does not

Proven: the mod can

* find the game's type registry and read it safely (validated descriptors,
  depth limit, node budget);
* insert a type of its own into it, in the same class and under the same
  parent as the game's own sync payloads;
* turn that type into an instance through the game's own allocator, i.e.
  memory the game's serialiser and transport accept.

Not proven yet: that the game will **serialise and send** that instance,
and that the campaign will accept a session. The send path is decoded
(`FUN_00882FD0 -> FUN_009CDE40 -> FUN_00A11030 -> FUN_00BE2900`) but has not
been driven; the session is still `0` in the campaign.

### The open question for milestone 2

The class record's instance-creator also sets a per-type **instance vtable**
(`0x010904B8` for the type in `FUN_00878000`). The mod's type does not have
one yet. Whether the game needs it for serialisation, or whether the
serialiser only reads the payload bytes the type declares, is the next thing
to establish - it decides whether the mod's payload can be a plain data
struct or needs a vtable of the same shape as the game's.

### Repo state

* mod repo: `85a1fbf` "Milestone 1: register a type of our own in the game's
  network registry" - committed, LF-only blobs, 13/13 ctest.
* hub: this file and the game README updated with the corrected class id and
  the allocator findings; pushed to the PR branch, PR #126 open for review.
