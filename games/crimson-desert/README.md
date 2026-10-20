# Crimson Desert (2026)

| | |
|---|---|
| Engine | BlackSpace Engine (Pearl Abyss, in-house) |
| Versions tested | Steam build 25477059, `CrimsonDesert.exe` 1.0.0.2976 (game version 2.03.02), PE timestamp `0x6AB28F00`, SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`; Linux, Proton Experimental |
| Projects | [cdmp (ByJunter)](https://github.com/ByJunter/CrimsonDesertMultiplayer), [CrimsonDesertCoop (blizz3010)](https://github.com/blizz3010/CrimsonDesertCoop), unreleased mod by No-Context2224; see the three Crimson Desert rows in [DIRECTORY.md](../../DIRECTORY.md) |
| QA level | none (one-PC tests only) |
| Last updated | 2026-10-19 |

## Where things stand

Two players can see each other and fight the host's nearby enemies with ByJunter's cdmp alpha, but the story is not
shared: each player stays in their own save. This folder documents a revival of cdmp aimed at shared quest progress
(claim: [cranebeaker/coop-anything#51](https://github.com/cranebeaker/coop-anything/issues/51)). So far:
- cdmp builds on Linux and runs under Proton;
- the quest/stage messages are mapped;
- an experimental stage sync applies the host's quest stage branches on the guest. It passed one-PC tests through
  cdmp's echo relay, but has not been tried between two PCs, and it is not yet shown to move a real quest forward;
- the other player's copy no longer jumps while walking (Move thinning).

## Existing work

- **cdmp** by ByJunter, [GitHub](https://github.com/ByJunter/CrimsonDesertMultiplayer) and
  [Nexus](https://www.nexusmods.com/crimsondesert/mods/3677). C++20 ASI mod, GPL-3.0, v0.2.0 (2026-10-11), author no
  longer maintains it. Only build 1.0.0.2976. Syncs movement, gear, horse, rope, time of day, map marker, and hostile
  enemies near the host. Steam friends only. Research notes (Spanish, English index) in its `docs/research/`.
  Revival fork: [CaptCapybara-cmd/CrimsonDesertMultiplayer](https://github.com/CaptCapybara-cmd/CrimsonDesertMultiplayer).
- **CrimsonDesertCoop** by blizz3010, [GitHub](https://github.com/blizz3010/CrimsonDesertCoop), 0.2.3, last push
  2026-07-08, no license. Player 2 takes over a companion. Its README says quest and cutscene sync are not applied.
- **No-Context2224**: unreleased, known from press only
  ([Wccftech](https://wccftech.com/crimson-desert-2-player-co-op-mod-but-more-advanced-soon/)).
- Pearl Abyss has talked about an official multiplayer mode; no date found.

## How the game works (for co-op)

- The single-player game keeps the MMO split of its ancestry: the client talks to an in-process server through
  1,114 "Troc" messages (`u16 opcode | u8 slot | u16 len | payload`). cdmp hooks both funnels
  (`ClientNet::PostPacket` exe+0x35C670, `ClientNet::OnRecvPacket` exe+0x35CA00) and can inject packets both ways.
  Message table with handler RVAs: cdmp `docs/research/data/troc_messages_2976.csv`.
- Each C->S message has a Process function (vfunc 2) that deserializes fields one by one (`Deserializer` vfunc +8,
  byte count in `r8d`) and then calls the game logic. Bound functions by the `.pdata` table, not by int3 padding
  (handlers are packed without padding; see the correction in [NOTES.md](NOTES.md)).
- **Quest progress** is stage-chart driven: Quest -> Mission -> Stage, with sequencers for scripted scenes.
  - `UpdateQuestListAck` (2808), `UpdateMissionListAck` (3009), `UpdateStageListAck` (2724): header
    `u8 scope | u32 player key | u8 a | u8 b` (full list a=1 b=0, delta a=0 b=1), then fixed records:
    quest 34 bytes `{u32 key, u32, u64, u64, u64, u8 state, u8 flag}`, mission 55 bytes, stage rows every 56 bytes.
    Quest and mission keys are static data keys (0x0F4xxx range).
  - Stages are referenced by **runtime instance ids** (small numbers like 0x158), different per session:
    `StageBranchStateReq` (2436) `u64 instance | u32 branch | u16`; `StageBranchStateAck` (2981)
    `hdr | u64 instance | u32 state | u32 branch | u32 chart hash`; `StageOnBranchEventAck` (2524).
- **Retail stubs** (parse, then do nothing): `CompleteQuestByCheatReq`, `ResetQuestByCheatReq`,
  `StageChartForceBranchDevReq`, `ForcePlayStageDevReq`, `ForcePlayStageSequencerDevReq`,
  `ChangeGameLevelStateByCheatReq`. The quest cheats are not a shortcut. The game's own `StageBranchStateReq`,
  `StageBranchStateDelayEndReq`, `UpdateStageChartDelayedEventReq`, `QuestDialogEndReq` and others are live.
- Saves: `%LOCALAPPDATA%\Pearl Abyss\CD\save\<steam account id>\slot*`; display settings in
  `user_engine_option_save.xml` next to them (`_displayType` Windowed/Borderless/FullScreen, `_viewportSize`).
- Denuvo anti-tamper; no online anti-cheat seen in `bin64`.

## What we changed

All on branch [`dev/quest-stage-tests`](https://github.com/CaptCapybara-cmd/CrimsonDesertMultiplayer/tree/dev/quest-stage-tests)
of the revival fork (on top of [`fix/clang-cl-build`](https://github.com/CaptCapybara-cmd/CrimsonDesertMultiplayer/tree/fix/clang-cl-build)).

- **clang-cl build fix** (`src/common/enemy_zone.h`): clang rejects a default argument that needs a nested struct's member
  initializers inside the enclosing class; split into two constructors.
- **Quest stage sync** (`[coop] stage_sync=1`, off by default; protocol v10):
  - the host sends each stage branch its game chooses (`StageBranchStateReq`), named by the stage's static sequencer key;
  - the guest applies it to its own instance of that stage when it is waiting there.
  - Only branches a real game chose are sent: a made-up branch number crashes the game.
  - Rules and tests: `src/common/stage_sync.h`.
- **Move thinning** (`[coop] move_thin=1`, on by default): the other player's Moves are cut to about 40 Hz, the cdmp
  author's own proposal. Without it, the client's 30-sample buffer overflowed and the copy stalled and jumped.
- **Dev test tooling** (`[dev]` keys, all off by default):
  - `test_autoload` / `test_autoquit`: the mod presses E inside its own game window until the world loads, and closes the
    game without saving when a test ends. The menus react to `WM_KEYDOWN`/`WM_CHAR`, not to Raw Input.
  - `test_walk_s`: walks the player back and forth. The mouse is left alone (`ClipCursor`/`SetCursorPos` do nothing).
    A motion meter logs `Puppet motion: RESULT` for the copy.
  - `stage_record` / `stage_replay`: records the stage branches and quest changes of one session, and replays the branches
    on a fresh load. The recorded quest changes are the pass/fail check.
  - `stage_sync_feed`: the one-PC test of stage sync through `cdmp_peer`.
  - `stage_force_test`: the mutation case (stub dev request, made-up branch).

## QA

QA level stays **none**: every check so far ran on one PC, and there is no two-player test yet. All runs below were
unattended. Details and dates are in [NOTES.md](NOTES.md).

- Unit tests (`cdmp_tests.exe` under Proton's Wine): all pass, including stage-branch message, sync queue and Move thinner.
  Mutation checks: removing the queue's retry wait or the thinner's parent-change rule makes the suite fail.
- Record/replay by static key: a branch recorded in one session was applied to a different instance id in the next.
  The server answered in 9 ms, and the game carried on from it.
- Stage sync through `cdmp_peer`:
  - 2 of 2 fed branches applied to this world's own stages, no crash;
  - after a race fix, the game's own echoed branches are no longer re-sent;
  - again PASS with 5 % packet loss and 40 ms jitter.
- Walking copy, 60 s of bot walking per run:
  - without thinning: 5 and 14 jumps;
  - with thinning: 0 jumps after warm-up in both runs, max 9.8 m/s, copy distance equal to the walk.
  - running (Shift held): about one small jump or stall per minute with or without thinning, so no measurable effect there.
- Autoload into the world: 3 of 3 runs after the `WM_KEYDOWN` change.

## How to play

Use ByJunter's release from Nexus (both players, same game build 2.03.02, Steam friends). On Linux/Proton add
`WINEDLLOVERRIDES="winmm=n,b" %command%` to the Steam launch options so the `winmm.dll` loader is used (worked in the
smoke test above). Back up saves; do not save while the other player is in your world.

## Known issues

- Story, NPCs, quest progress and loot are per player; only hostile enemies near the host are shared.
- Stage sync applies only a stage's first branch, needs both players to have the same stage in their world, and is
  untested between two PCs.
- Works on one game build only; any patch disables it until offsets are updated.
- One crash on Proton while opening the menu (vkd3d-proton `VK_ERROR_DEVICE_LOST`) right after a stub request was injected;
  not reproduced since. Treated as unknown.

## Next steps

1. Record a real quest step (needs one human-played session), then replay it unattended and compare the quest changes.
2. A two-PC session with stage sync on.
3. A client-side guard that reads a stage's branch table before sending, so a mismatched branch is refused.
4. Players at different story points; later branches of a stage; quest rewards for the guest.
5. Walk bot: climbing and riding; find the cause of the small residue when running.

## Tools in this folder

- `tools/xbuild/`: podman image with clang-cl 19, lld-link and the Windows SDK/CRT from xwin 0.6.5 (download the
  release tarball into `dl/` and check its SHA-256 first; building the image accepts Microsoft's SDK license), plus a
  CMake toolchain file and `build.sh <abs source dir>` that prints BUILD_OK/BUILD_FAILED.
- `tools/re/`: capstone + pefile scripts, run in a hash-pinned image against your own `bin64` mounted read-only:
  `dis.py` (disassemble at an RVA), `classify2.py` (stub or live handler, exact .pdata bounds), `disp2.py`
  (operand offset search), `callers.py` (direct callers), `cap.py` (extract payloads from a cdmp `.bin` capture),
  `questlist.py` / `rows.py` (decode list Acks), `sections.py`.
- `tools/proton/run_game.sh`: launch a hardlinked test copy with its own prefix through Proton.
