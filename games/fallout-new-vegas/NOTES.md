# Fallout: New Vegas: working notes (TTW)

Goal: story co-op for Tale of Two Wastelands (TTW) plus a mod list, built on fnv-coop.

## Identity of the test setup

- Steam, New Vegas 1.4.0.525 (exe SHA-256 `518C87F5...2A57`, 4GB-patched), xNVSE 6.4.9, Windows 10.
- Mod Organizer 2 with the TTW game plugin; TTW, the TTW NVSE plugin, YUP for TTW and about 20 other mods.
- The 4GB patch (FNVpatch) changes the exe hash. Byte diff against the unpatched backup (observed): 188 bytes
  differ, only in the PE header (Characteristics = large address aware, AddressOfEntryPoint, CheckSum) and in
  the `.bind` section (the Steam stub, which the patch hooks to start xNVSE). `.text`, `.rdata` and `.data`
  are identical, so engine addresses in `.text` that fnv-coop hooks don't move. Unpatched Steam 1.4.0.525
  SHA-256: `3A87F92F...104D`; patched: `518C87F5...2A57`.

## Observed

- TTW plugin order in MO2's plugins.txt: FalloutNV, the 4 story DLCs, GunRunnersArsenal, then Fallout3.esm,
  Anchorage, ThePitt, BrokenSteel, PointLookout, Zeta, then the 4 add-on packs, TaleOfTwoWastelands.esm, YUPTTW.esm.
  Source: `<MO2>\profiles\Default\plugins.txt`.
- fnv-coop `tools/questdata/loadorder.json` hard-codes the 10 vanilla masters. `loadorder.py verify` raises
  `LoadOrderMismatch` on any other order, so every tool that needs runtime FormIDs fails under TTW until a
  TTW load order is supplied. Source: fnv-coop @ 5b74d71.
- fnv-coop's `FormRef` is (plugin file name, local id), so the sync protocol itself does not depend on load order.
  Source: `synccore/include/fnvcoop/types.h`.
- fnv-coop docs mention TTW only in `docs/05-nvmp-intel.md` (NV:MP has a "Hosting TTW" wiki page:
  https://nuwiki.nv-mp.com/wiki/Hosting_TTW) and one TTW script function in `tools/questdata/ctda_funcs.py`.
- The test for `plugin/hooks` already uses `FormRef("fallout3.esm", ...)` as a fixture.

- fnv-coop's quest-data pipeline (`tools/questdata/run_all.py --data --plugins`) parses all 18 TTW masters
  unchanged: 917 quests, 4382 stages, 258 quest override records, 917 scenarios. It only failed at the
  scenario step on `loadorder.json` (KeyError `fallout3.esm`); fixed by a `FNVCOOP_LOADORDER` env var plus
  `loadorder.ttw.json`.
- EditorIDs do not collide between Fallout3.esm/DLCs and FalloutNV.esm/DLCs (0 collisions), so
  `rules.json` (keyed by EditorID) can hold both games.
- Story quests by plugin under TTW: Fallout3 68, BrokenSteel 16, PointLookout 18, Zeta 13, Anchorage 6,
  ThePitt 6, TaleOfTwoWastelands 6 = 133 with no rules in fnv-coop. After running
  `tools/coverage/build.py --qdir <ttw questdata>` (with `FNVCOOP_QUESTDATA` support added to
  `tools/coverage/model.py`), 92 of them get derived (uncurated) rules, 0 validation errors; 41 stay without
  rules (tiers T0-T2, which the generator skips).
- TTW and YUP override quests that fnv-coop has curated rules for: TaleOfTwoWastelands.esm 41, YUPTTW.esm 74,
  Fallout3.esm 6. Includes VCG00/VCG01 (character creation; TTW changes the start) and the main quest.
- Comparing the vanilla and TTW risk profile (tier, stage count, risk categories, cutscene, move) for the
  483 curated quests: 415 identical, 68 differ. Examples: NVDLC01MQ00 loses its player MoveTo (curated
  PartyWarp rule no longer matches); the companion fire quests (NVDLC01NewVegasFollowerFireQuest,
  P04CompanionFireQuest) lose almost every category; CG02 goes T0 -> T3.
- Build: 20 `.cmd`/`.bat`/`.py` files hard-code `C:\Program Files\Microsoft Visual Studio\2022\Community`,
  so nothing builds with VS Build Tools or another edition, and `test_all.py` marks every C++ suite as missing
  its toolchain. Fixed with `tools/build/vsroot.py` + `vsroot.cmd` (env VSROOT, then vswhere, then the old path).
- Build: `tools/build/build.py` and `tools/runner/build_l1.cmd` ("canonical nvcoop.dll build") need a root
  superbuild `CMakeLists.txt` that is not in the repo and never was (`git log -- CMakeLists.txt` is empty).
  `plugin/hooks/build_nvcoop.bat` builds from `plugin/CMakeLists.txt` and works.
- With VS 2022 Build Tools (MSVC x86) + xNVSE 6.4.9 sources: `nvcoop.dll` builds, 242/242 steps, PE machine
  0x014C (x86).
- Baseline `python -I tools/test_all.py --quick` on fnv-coop @ 5b74d71: 20 PASS, 0 FAIL, 3 SKIP so far
  (skips: canonical build, and two suites needing `out/questdata`, which is now generated).

### In game (2026-10-26, flattened TTW folder, fnv-coop runner instances)

- With `bUseMyGamesDirectory=0` (which the runner sets), FNV 1.4.0.525 reads `.\Plugins.txt` from the instance folder,
  not `%LOCALAPPDATA%\FalloutNV\plugins.txt`: the instance loaded all 18 TTW masters while the AppData file lists 10.
  The AppData file's SHA-256 was the same before and after. So instances can run TTW without touching owner files.
- The bridge `plugins` order matches `loadorder.ttw.json` (`loadorder.py verify`: load order OK).
- The runner's instances drop every third-party xNVSE plugin (`mirror_tree` skips Data/NVSE/Plugins), so TTW runs
  without `ttw_nvse.dll`/JIP. Added `NVCOOP_GAME_PLUGINS=1|<stems>` to `tools/runner/instances.py`. With it, all 17
  plugins load next to nvcoop.dll.
- The console reads Fallout 3 quests by hex FormID: `GetStage 06014E90` (MQ10) works; EditorIDs do not resolve.
- Engine pins/hooks that fail with the full TTW plugin set: `BGSQuestObjective::SetState` PrologueMismatch,
  world-time menu/travel pin, intents `MarkScriptEvent` (005AC750). With only ttw_nvse + JIP + JohnnyGuitar, only
  `MarkScriptEvent` still mismatches: the other two are patched by one of ShowOff, Stewie's Tweaks, Engine
  Optimizations, NVTF, kNVSE, CrashLogger, mlf, UIO, Improved Console, ActorCause fix, VATS/weapon lag fix (which one: not bisected yet).
- Crash `FalloutNV.exe+0x25FDA` (null read, call chain through projectile creation, `HandGunMuzzleFlash01` on the
  stack) after loading a save made by `coc GSDocMitchellHouse` from the main menu (that save was made after force-closing
  TTW's INI warning box, see below). Seen with co-op on and once with `coop.enable=0` (one of two copies).
- Same crash with ONE copy, co-op off, after `coc qasmoke` from the main menu (no save involved). CrashLogger's stack
  shows the shooter: Character "Wastelander" 06031E2C (`FFEU02NPC2Ref`, a Fallout 3 freeform-encounter NPC) in
  "Holding pen for freeform encounters" (FFEncounterWorld), weapon WeapNV9mmPistol; the engine dies building the muzzle
  flash for an actor with no loaded 3D.
- Control: vanilla FNV (no TTW, no third-party plugins) + the same nvcoop.dll + `coc qasmoke`: no crash in 150 s.
- TTW without nvcoop.dll could not be checked: JIP's `Data\NVSE\plugins\scripts\gr_*.txt` ran (`con_SCOF` made its
  log) but `coc` from it did not enter the world.
- CORRECTION: `qasmoke` is not a cell in this game ("Item 'qasmoke' not found"), so every `coc qasmoke` run stayed at
  the main menu. The vanilla "control" and the 13:41 TTW crash were main-menu runs; in that TTW run my loop called
  `CloseActiveMenu` on the main menu itself, so that crash is my own doing. Use `coc GSDocMitchellHouse` and check
  the player left the menu placeholder position (2048, 2048, 128).
- The runner's instance INIs miss the Best of Times `FalloutCustom.ini` settings (MO2 applies it on top of the
  profile INI). TTW's `TTWGenericHandlerScript` (TaleOfTwoWastelands.esm:001880) checks
  `bBackgroundLoadLipFiles:BackgroundLoad` and shows "Misconfigured INI settings detected!" when it is 0. After merging
  FalloutCustom.ini into the base Fallout.ini: no message box, 180 s in the world, no crash, clean save.
- TTW's hard requirements (same script): xNVSE >= 6.3.5, JIP LN >= 57.2, JohnnyGuitar >= 501, Stewie's Tweaks >= 941,
  ShowOff >= 180, ttw_nvse.dll, 4GB patch (GetIsLAA), bBackgroundLoadLipFiles=1.
- TTW shows `TTWNVMPIncompatibleMessage` when `IsDLLLoaded "client"` (NV:MP's client.dll) is true: TTW declares
  NV:MP incompatible. So NV:MP is not a route for TTW co-op.
- Main-menu automation: JIP `ClickMenuButton` on `StartMenu/NOGLOW_BRANCH/main_container/lb_item_hotrect:4` ("New")
  crashes inside the New option (FalloutNV+0xA0CBA2), from the console and from a JIP `gr_` script, minimized or
  visible. `TapKey` and OS mouse input (SetCursorPos, relative mouse_event) do not reach the menu of a test copy.
  `ClickMenuButton` on Settings works, so the path syntax is right.
- Without JIP, `GetActiveMenuMode` does not exist; nvcoop's `bot.menu` ([dev] bots=1) reports the top menu in both.

### First TTW co-op result (two copies, all 17 plugins, clean save, Fallout 3 quest MQ10 Infiltration 06014E90)

    PASS session formed; PASS starts at 0; PASS host sets 10 -> both 10 in 0.1 s; FAIL guest sets 20 (both stay 10);
    PASS 10 stays done; PASS host sets 100 -> both 100 in 0.1 s; FAIL 20 stays done      7 of 9

Vanilla control (two copies, no third-party plugins, VCG02 0010A214, same steps): the identical 7 of 9, the guest set
fails the same way. So guest-to-host console SetStage does not work in fnv-coop today (an upstream gap: the shipped
`scenarios/quest_stage_sync.json` expects it), and TTW matches vanilla on this proof.

Repeat rounds on the clean TTW save: 7 of 8 rounds ran with both copies alive and the same 7 of 9; 1 round (the first)
crashed the host. All earlier crash-heavy rounds used the broken save.

Crash after a guest joins (host, FalloutNV+0x25FDA, Fallout3.esm:031E2C FFEU02NPC2Ref firing in the encounter holding
pen): seen in some TTW two-copy rounds, not in a single host with co-op on (240 s) or co-op off (240 s) on the same
save.

### Engine-hook conflicts with TTW's required plugins (static, then in game)

Method: count, in each plugin DLL's bytes, the little-endian 32-bit values address .. address+0x1F (plugins keep the
engine addresses they patch as constants). A static hint only; the in-game hook status confirms it:

| nvcoop pin | plugins that hard-code that spot |
|---|---|
| BGSQuestObjective::SetState 0x5EC5D0 | ShowOff (+0xC, +0xD, +0x11), Stewie's Tweaks (+0xC) |
| bots Tick 0x825C00, VatsQueueCount 0x5AE380, RunPadGate 0x941A2B | Stewie's Tweaks |
| bots ItemInfo 0x9611E0 | Stewie's Tweaks (+0), ShowOff (+8) |
| intents MarkScriptEvent 0x5AC750 | ShowOff, Improved Console, Console Clipboard (+0), JIP (+0x13) |

ShowOff and Stewie's Tweaks are hard TTW requirements, so nvcoop has to live with them.

Objective hook fix (fnv-coop `plugin/hooks/hook_patch.cpp`): the hook overwrites 6 bytes but compared 16. ShowOff and
Stewie re-point the `call` at +0xC. The new rule keeps the stolen bytes strict and accepts a difference past them only
inside the rel32 of an E8/E9 that is still there and now points at executable code outside FalloutNV.exe's image
(`MemoryOps::foreignCode`, default false; only `engine_hooks.cpp` implements it). A target inside the image (another
exe build) is still refused. Unit tests: 5 new cases; 1866 checks, 0 failed; the prologue-check mutant fails them.

In game, two copies, all 17 TTW plugins, objective 10 of Fallout3.esm MQ01 (06014E87), shown then completed on the host:

| build | SetState hook | objective sync |
|---|---|---|
| fnv-coop @ 5b74d71 (unfixed) | PrologueMismatch on both | FAIL: nothing reaches the guest in 10 s (1 of 3) |
| with the fix | Ok (69 / 71 hooks, 0 failed) | PASS: guest follows in 0.3 s (3 of 3) |

Intents call-pin fix (fnv-coop `plugin/intents/engine/game_engine.cpp`): under TTW, `MarkScriptEvent` (0x5AC750) reads
`E9 <rel32> 08 C6 45 FF 00 83 7D 0C 00 74 2A`: another plugin's 5-byte JMP over the entry (target differs per process:
a runtime trampoline), rest untouched. nvcoop only calls that function, so a JMP over the entry whose target is
executable memory outside the exe, with every later byte matching, is accepted; anything else still fails. One
mismatching call pin turned the whole intents path off. In game: both copies log "detoured by another plugin; calling
through its jump", `intents.status` pinsOk true (was false); stage proof and objective sync unchanged. Intents C++ suite:
51/51 tests, 615 checks, all 30 twins caught. (The intents feature itself is off by default upstream: `[intents] enable=0`;
an end-to-end guest activation test is still to do.)

Vanilla with the fix: all hooks Ok, stage proof unchanged (7 of 9), objective sync 3 of 3 on running quest VCG01 (00104C1C).
(A first vanilla objective try on VCG02 was invalid: the stage proof had completed and stopped that quest, so the host
itself could not show the objective.) When the guest joins it applies the host's baseline, which includes Fallout 3 actor writes
(`SetActorValue Aggression 0` / `Confidence 0`, `ModActorValue Health 500` on 06037224/06037225, `SetDestroyed 1` on
0602C11B/0602D934).

### Bots pins under TTW (fnv-coop `plugin/bots`)

With `[dev] bots=1`, four call pins failed under TTW. The bytes found are not jumps but shorter rewrites of the same function
that another plugin writes in place (Stewie's Tweaks hard-codes ItemInfo, Tick, VatsQueueCount):
HeapAlloc 0x401000 `FF 74 24 04 B9 38 62 1F 01 E8 32 2E 6A 00 C3 ..` (push [esp+4]; mov ecx,game heap; call allocate; ret),
ItemInfo 0x9611E0 `8B 41 18 C3 ..`, Tick 0x825C00 `8B 41 14 C3 ..` (frameless getters), VatsQueueCount 0x5AE380 (register-only loop).
The game itself runs these. Fix: `kKnownRewrites` in bots_addrs.h, exact 16-byte matches per address only. In game: all
four "known rewrite ... accepted", `bot.menu` answers (was "engine glue not wired"). RunPadGate (always-run) still refuses:
Stewie changes a branch there, which changes behaviour; bots fall back as designed. Bots suite: 433 checks, 0 failed, 67/67 twins.

### Crash rate on the clean save

Six more two-copy rounds with all fixes: all 7 of 9 (as vanilla), objective sync 3 of 3, both copies alive. Total on the
clean save: 13 of 14 rounds without a crash.

### Rules data at runtime

The rules engine runs only when `nvcoop_rules.json`, `nvcoop_edids.json` and `nvcoop_infoquest.json` sit next to
nvcoop.dll (`plugin/coop/tools/gen_coop_data.py`). The runner instances had none, so every round above ran with NO quest
rules (vanilla too). `gen_coop_data.py` now takes `--qdir` / `--rules`, and adds every quest EDID (a stage-less quest such
as TTWTrainTripQuest was missing). With the TTW rules loaded (575 quests): stage proof 7 of 9, objectives 3 of 3, unchanged.

Runner bug fixed: `nvcoop_*.json` files from an earlier provision stayed in the instance and were still loaded (stale rules).

### TTW train between the wastelands (TTWTrainTripQuest)

`questlines/ttw/rules.patch.json` adds a curated entry (PartyWarp to the two engine-car markers and the holding cell,
SetPlayerControls; a PartyTether on TTWTrainEngine01a/01b is dropped for now: those CELLs are not in forms.json).
Loaded only for TTW data; the vanilla rules build is byte-identical with and without it.

End-to-end test (host coc TTWTrainEngine01b, tickets added, `Activate player 1` on switch 10002130, prompt "Are you sure
you wish to travel to New Vegas?", button 1). Save had VCG00 not started, so the "3 years later" branch ran (holding cell,
RemoveAllItems into TTWStolenWeaponsRef, StartQuest VCG00).

| run | guest (never touched anything) | guest controls during the ride |
|---|---|---|
| TTW rules loaded | ends at the host's exact position in the holding cell (10007B77) | never locked |
| no rules (control) | the same: ends at the host's exact position | never locked |

So the guest follows the host's `Player.MoveTo` WITHOUT the rule (the "player" ref is the same id on every machine); the
rule's spacing / hold-absent behaviour did not show (distance 0 in both). Control locks from the quest script do not reach
the guest (gap for cutscenes). `bInNVWasteland` reached both. PlayBink videos stall while the copy's window is minimized
(idle main thread, 0.08 s CPU in 5 s); showing the window let it continue: run train tests with NVCOOP_VISIBLE=1.

### Sync traffic: what the guest applies (flight traces)

Effect tags counted from the guest's TxnApplied records (the .nvfr dumps nvcoop writes on desync):

| | SetScriptVar | SetGlobal | StopQuest | SetStage | MoveRef |
|---|---|---|---|---|---|
| TTW session (~5 min, 3 dumps) | 8759 | 5942 | 997 | 922 | 22 |
| vanilla session (3 dumps) | 144 | 2246 | 819 | 515 | 0 |

Host-to-guest story datagrams: TTW 15.5/s, vanilla 12.2/s. The StopQuest / SetStage repetition and the desync dumps
(every ~30-40 s) happen on vanilla too (upstream). TTW-specific: about 60x more quest-variable writes (TTW and Fallout 3
quest scripts with per-frame timers and position copies). In the train trip the guest applied the host's MoveRef (the
guest's own player moved) and no SetPlayerControls at all: control locks are captured with Recipient::dflt and never
reach the guest.

`plugin/snapshot/tools/volatile_scan.py check` (the gate for the shipped volatile list): vanilla 154 candidates, 0 missing;
TTW 357 candidates, 139 occurrences / 84 unique vars missing (Fallout 3 and DLC timers, position copies).
`plugin/snapshot/data/volatile_keys_ttw.txt` = the shipped list + those 84; deploy it as nvcoop_volatile_keys.txt next to
nvcoop.dll on every machine of a TTW session (the join handshake compares its hash). volatile_scan.py no longer dies without
the uncommitted questlines/systems/inventory.json (only the encounter "cursor" kind is skipped); that also fixes the L7
"volatile probe tests".

Questlines: `tools/coverage/model.py` files TTW quests under "TTW: ..." (Fallout 3 main quest MQ01-MQ12, prologue CG00-CG04,
side quests MS##, misc, one per Fallout 3 DLC, TTW itself) instead of "Systems & misc". Vanilla rules.json unchanged.

### TTW volatile list: no measurable effect

Two otherwise identical TTW sessions with the TTW rules loaded (round + 3 min idle), host `coop.status`:

| volatile list | replicator.txns | volatileDropped | digest repairs / effects repaired | desync dumps host / guest |
|---|---|---|---|---|
| shipped (182 vars) | 3156 | 0 | 22 / 4209 | 8 / 6 |
| TTW (266 vars) | 3208 | 0 | 22 / 4214 | 8 / 6 |

So the 84 statically found TTW vars are not what drives the traffic (or the filter does not touch them). Kept
`volatile_keys_ttw.txt` only as a candidate list; finding the hot variables needs a wire decoder for the trace payloads.

### Quest-script causes never matched their quest's rules (upstream, fixed)

The capture hub's weak cause for a quest's own script (GameMode / MenuMode blocks) names the SCRIPT form, but
`RulesEngine::rulesFor(cause.source)` is keyed by QUEST (byForm_ holds only quest refs; synccore's own tests build such
causes with the quest as source). So no rule (PartyWarp, SetPlayerControls, GuestLocalRun, ...) ever applied to effects
of a quest's per-frame script in the real game. Flight traces: all 22 MoveRef the guest applied in the train trip had
cause Script. Fix: `plugin/hooks/engine_hooks.cpp` `questOfScript()` maps a quest script to the one quest that runs it
(a script shared by several quests stays the cause). Train trip with the TTW rules loaded:

| build | guest controls during the ride |
|---|---|
| before the fix | never locked |
| with the fix | locked at the same moment as the host's (t=3 s) |

Both stay locked at the end of the Fallout 3 start branch (correct: VCG00 takes over). The guest lands on the host's exact
position (distance 0, no 64-unit slot offset): the replicated move targets the marker; open, cosmetic.

### Quest rules for TTW (curation, `questlines/ttw/rules.patch.json`)

Method: a local worksheet per quest (stage and quest-script lines that matter for co-op next to the derived rules;
it quotes game scripts, so it is not committed). New `replace-quest-entry` action in the actionlist adapter (merge mode
`replace`), so a curated entry drops derived rules it does not repeat. Vanilla rules.json byte-identical after it.

Curated: CG01 Baby Steps and CG03 Future Imperfect (character creation over a childhood: PerPlayerScene + Barrier +
SetPlayerControls + SaveSuppress, the VCG00 / VCG01 pattern), CG04 Escape! (shared: warp to cg04playerstartmarker,
tether Vault101 / Vault101b, rewards to all), MQ04 Tranquility Lane (tether Vault112a, warp MQ04StartBench /
MQ04PlayerPod), MQ09 The American Dream (capture: warp, control lock, no saving), TTWTrainTripQuest. All markers and cells
resolve; 0 validation errors. Reviewed and left derived: MQ01-03, MQ05-08 (no player moves or scene locks needing
parameters). Open (owner decision): MQ11 / EndingBinks (purifier kills the activator; ending videos chosen by karma;
PlayBink not in the effect vocabulary).

Note: `PartyWarp.markers` is not used at runtime (`RulesEngine` expands every player MoveRef of the quest); the list is
documentation and a validation check. Behaviour comes from which rule kinds are present.

Curated New Vegas rules on quests TTW changes (sample of 10 from the 68): none becomes harmful. TTW removes the Dead Money
bunker teleport (that warp just never fires), adds an intro PlayBink to VCG00, guts the companion fire quests to
`StopQuest` (their rules have nothing to act on) and adds Dogmeat / TTW follower-perk lines to Followers.

### Inventory stash (RemoveAllItems into a container) does not replicate (upstream gap, measured)

Two-copy TTW session, host runs The Pitt's DLC01Quest02 stage 0 (`Player.RemoveAllItems DLC01PlayerItemContainerREF`,
`Player.AddItem DLC01SlaveOutfit`, `Player.MoveTo DLC01Quest02TestStartMARKER`); host 1111 caps, guest 2222:

| | host | guest |
|---|---|---|
| moved to the Pitt | yes | yes (same spot) |
| slave outfit added | yes | yes |
| inventory stripped into the container | yes (container 1111) | **no** (keeps 2222) |

Same with `[hooks] story_capture=1` (it installs under TTW: host 144 hooks, 0 failed). Cause: `h_RemoveAllItems` skips the
player on purpose (`!isPlayer(thisObj)`) and the `RemoveAllItems` effect has no destination (it means "clear this
container"). A return such as `DLC01PlayerItemContainerREF.RemoveAllItems <return container / player>` replicates as a
clear, which would destroy a guest's stash if there were one. Affects The Pitt, Operation Anchorage, Mothership Zeta,
MQ04, MQ09, TTW's train (Fallout 3 start) and Dead Money's bunker (the pending "inventory-stash" proposal).
Proposal: give `RemoveAllItems` a destination ref; capture the player case as "player -> container"; each machine applies
it to its own player and its own copy of the container (container contents are not synced while the loot module is off),
and the return as "container -> player / container". Touches protocol (wire v6), baseline / late-join replay, shadow
folding and digests, so it needs upstream agreement first.

**Built (owner asked to do everything needed, 2026-10-26).** New effect `ContainerTransfer{src, dst, srcPlayer, dstPlayer, to}`
appended at the end of the Effect variant (existing wire tags unchanged): `h_RemoveAllItems` records it for a player strip,
a hand-back and container-to-container moves (a target-less container clear stays `RemoveAllItems`); each recipient runs
`RemoveAllItems <dst|player>` on its own player (0x14) / its own copy of the container. Host policy: shared by default
(every player handles their own stash); `PerPlayerScene` narrows it to the acting player. An event: not in the baseline,
not folded into the shadow; the plugin sink skips it when delivered as join history (catch-up / snapshot / baseline).
Tests: wire roundtrip of every effect type; `rules.quest_inventory_stash_reaches_every_player_once` (story strip and
hand-back reach everyone, a PerPlayerScene strip stays with the actor, a late joiner gets it only as history) and twin
`stash_origin_only` (caught). Full synccore suite 308/308; plugin hooks 1866 checks, 0 failed.

In game (The Pitt, DLC01Quest02 stage 0 then 170, host 1111 caps / guest 2222):

| | host | guest before | guest with the fix |
|---|---|---|---|
| stripped into the stash | 1111 | not stripped (kept 2222) | 2222 into its own stash |
| hand-back at stage 170 (into the return container) | 1111 | - | 2222, its own caps |

Vanilla round with rules + the fix: stage proof 7 of 9, objective sync 3 of 3 (unchanged). The TTW train's Fallout 3 start
branch (`player.RemoveAllItems TTWStolenWeaponsRef`) now also stashes the guest's gear (`live ContainerTransfer:Ok 1`).

### Videos (PlayBink) for everyone: the Fallout 3 ending and TTW's train

`PlayBink` (handler 0x5D15D0, opcode 0x1114; found in the exe's unencrypted command table, prologue read in game with
`world.peek`) is a new story-capture event op `kOpPlayBink` (op 79, recipients all). Its file name can hold spaces
("2 weeks later.bik"), so a new quoted-text signature `q` allows spaces and `_-.,()!` but never a quote, semicolon or line
break, and puts the text in quotes (the plain-word guard for `s` stays). `story_addrs.h` regenerated from
`tests/story_prologues_dump.txt` (+ the PlayBink line); story checks ALL OK. Needs `[hooks] story_capture=1` (off by default).
In game (train trip, story capture on): the guest received `"3 years later.bik"` and `"FNVIntro.bik"` and applied
`live RefCommand:Ok 110` with no failures.

Fallout 3 ending, provisional default (owner delegated the decision): the player who enters the purifier code dies (the
original object script, host-run), and every player sees the ending videos (EndingBinksScript's PlayBink, replicated).

### Upstream fnv-coop issues found (not TTW-specific)

- 20 build/test scripts hard-code `C:\Program Files\Microsoft Visual Studio\2022\Community` (fixed: tools/build/vsroot.*).
- The root superbuild `CMakeLists.txt` used by `tools/build/build.py`, `build_l1.cmd` and most `run_tests` scripts is not
  in the repo; `--source plugin --build build/L5` works for the plugin tree.
- `plugin/snapshot` tests (volatile scan, sweep configs) skip without `out/questdata`, and once it exists they fail on
  files that are generated elsewhere and not committed (`questlines/systems/inventory.json`); same for `census.json`.
- A guest's console `SetStage` never reaches the host, in vanilla too, while `scenarios/quest_stage_sync.json` expects it.
- Main-menu "New" cannot be driven: JIP ClickMenuButton crashes in the New option; no bot path presses StartMenu.

## Derived

- The per-quest rules (`docs/quest-policies/rules.json`, 481 quests) cover FNV only; TTW's Fallout 3 quests
  have no rules. They have to be generated by running `tools/questdata` over TTW's ESMs.
- Note from the loadorder.json doc: FNV sorts ESMs by file timestamp, not plugins.txt order, so the real
  TTW order must be read in game (bridge `plugins` method), not from plugins.txt.

## Unknown

- Whether NV:MP 9.4 + TTW + fnv-coop load together.
- Whether the 92 derived TTW rules are right; none are curated or tested in game.
- How many TTW quest stages are co-op-unsafe.

## Log

- 2026-10-26: triage done, test copy made (game + MO2 instance, no saves). Blocked on build tools
  (Python, CMake, MSVC Build Tools) pending owner approval.
- 2026-10-26: owner approved installs (winget, hashes verified). TTW quest data, derived rules, rule-review
  list and nvcoop.dll build done. Next: load nvcoop.dll in the TTW test copy, read the real load order in game.
