# Crimson Desert — notes

## Game identity (observed 2026-10-19)
- Store: Steam, appid 3321460, Steam build id 25477059
- CrimsonDesert.exe file/product version 1.0.0.2976, PE timestamp 0x6AB28F00, image base 0x140000000
- SHA-256 CrimsonDesert.exe: 57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7
- Engine: BlackSpace (Pearl Abyss in-house). Install ships steam_api64.dll, sentry, librdkafka, pers.exe.
- OS tested: Linux via Proton (Windows-native game)
- DRM: Denuvo anti-tamper (source: https://www.pcgamesn.com/crimson-desert/denuvo-on-steam). No online anti-cheat files seen in bin64.

## Prior work (observed)
- ByJunter/CrimsonDesertMultiplayer (cdmp) — https://github.com/ByJunter/CrimsonDesertMultiplayer
  GPL-3.0, v0.2.0 released 2026-10-11, author says unmaintained. ASI mod (Ultimate ASI Loader winmm.dll).
  Targets exactly 1.0.0.2976 / PE 0x6AB28F00 / Steam build 25477059 (src/mod/game/offsets.h) — MATCHES this install.
  Two players, each in own save; shared enemies near host; Steam friends only. Nexus: https://www.nexusmods.com/crimsondesert/mods/3677
- blizz3010/CrimsonDesertCoop — https://github.com/blizz3010/CrimsonDesertCoop
  0.2.3, last push 2026-07-08, no license. Companion-hijack design; quest/cutscene sync not applied.
- No-Context2224: unreleased, more advanced mod in development (press only: https://wccftech.com/crimson-desert-2-player-co-op-mod-but-more-advanced-soon/)
- No coop-anything issues/PRs for this game (GitHub search, 2026-10-19).

## Derived
- Triage path: "Abandoned co-op mod exists" -> revive cdmp. It already runs on this build, so play-today is install-only.
- Under Proton, a winmm.dll proxy needs WINEDLLOVERRIDES="winmm=n,b" in Steam launch options (standard Proton behavior; not yet tested here).

## Unknown
- Whether cdmp works under Proton at all (unknown — not run).
- Whether the Oct 15 2026 DLC update changed the build (this install still reports 1.0.0.2976).
- Official Pearl Abyss multiplayer: discussed publicly, no date (unknown).

## Checks actually run
- sha256sum, PE header read, appmanifest read, file listing of bin64. Nothing launched, nothing installed.

## 2026-10-19 — revive cdmp: phase 0-2 survey
Observed (read in cdmp repo at v0.2.0):
- ~23k lines C++20 (src + tests). Build is MSVC/VS2022 + CMake on Windows; uses SEH (`__try`), SafetyHook, Zydis, ImGui DX12.
- Author's own open-work list: docs/PLAN-PENDIENTES.md (22 items). Research notes are Spanish, index in English (docs/research/README.md).
- One-PC test method already exists: `cdmp_peer --mode relay` echoes your own player back as the "other" player.
- Biggest open gaps for playing with friends: guest gets no loot/XP/quest progress (#17), guest also sees own enemies (#16), bosses (#18), quests/NPCs not shared at all.
- Small, well-researched open fix: Step 0.2 — guest's finishing blow uses KillCheatReq, which gives no XP and costs the host 10-15 skill XP per kill (pendiente-recompensas-progreso.md §0). Proposed fix: finish with AttackByCollisionReq.

Derived:
- No Windows here; build needs clang-cl + lld-link + Windows SDK/CRT (e.g. via xwin) in a podman container. mingw cannot compile MSVC SEH.
- Test install: hardlink the 142 GB data dirs, real copy of bin64 only (mod files + ini go there).

Unknown:
- Does a second install path count as a new Denuvo activation (5/day limit)? unknown.
- Does the mod (DX12 inline hooks, Streamline) work under Proton/vkd3d? unknown.

## 2026-10-19 — build + one-PC echo test on Linux/Proton
Observed (checks that actually ran):
- Built cdmp v0.2.0 (b62a572) on Linux with clang-cl 19 + lld-link + xwin 0.6.5 SDK in podman. Clang 18 fails (MS STL STL1000 needs Clang >= 19).
  One source fix needed for clang: enemy_zone.h default argument `Settings s = {}` -> two constructors (clang rejects nested-struct NSDMI in default arg).
- cdmp_tests.exe under Proton Experimental's wine inside SteamLinuxRuntime_4: "All tests passed" (no per-test count printed).
- Ultimate ASI Loader v9.7.4 bundled dinput8.dll SHA-256 031a3e55...2daf == dinput8.dll in official Ultimate-ASI-Loader-NoPDB_x64.zip (zip e5860e7d...45d1).
- Game launched from a hardlinked test install + copied prefix, through `proton waitforexitandrun` with HOME set to snap Steam's home, WINEDLLOVERRIDES="winmm=n,b", wine virtual desktop 1280x720.
  cdmp.log: build check passed, all hooks installed, DX12 overlay drew 15k+ frames (vkd3d-proton), echo puppet spawned via cdmp_peer relay,
  500+ events replayed, 0 rejected, minimap icon shown, normal quit ("Terminate 시작."). First launch attempt exited with code 225 before the exe loaded (cause unknown; second attempt fine).
- Input during the test came from a person at the window, not from a bot. No automated input yet.
Unknown:
- Denuvo activation count effect of the second install path (no error seen).

## 2026-10-19 — shared quests: first static pass
Observed (from cdmp docs/research/data/troc_messages_2976.csv, the mod's table of the 1,114 internal Troc messages):
- Progress hierarchy visible in message names: Quest -> Mission -> Stage (+ Sequencer = scripted scenes/cutscenes, QuestGauge = camp liberation bars).
- Server->client progress notices: UpdateQuestListAck 2808, UpdateMissionListAck 3009, UpdateStageListAck 2724, SubMissionProgressListAck 2102,
  NotifyQuestSaveDataAck 2426, CompleteQuestGaugeAck 2738, UpdateQuestGaugeAck 3088, StageGameEventAck 2222, UpdateStageChartEventAck 2111.
- Client->server dev/cheat requests still registered: CompleteQuestByCheatReq 2627 (Ack 2889), ResetQuestByCheatReq 2251 (Ack 3078),
  ForcePlayStageDevReq 2885, ForcePlayStageSequencerDevReq 2419, StageChartForceBranchDevReq 2476, MovePlayerByCheatReq 2860, ChangeGameLevelStateByCheatReq 2281.
- cdmp already injects C->S cheat Reqs successfully (KillCheatReq, VaryStatCheatReq), so the injection path exists.
Guesses (untested, label: guess):
- "Story follows the host": sniff host's quest/mission/stage list Acks, send deltas to the guest, guest injects CompleteQuestByCheatReq for quests the host finished.
  Risk: cheat completion may skip rewards/flags like KillCheatReq skipped XP (see cdmp pendiente-recompensas-progreso.md). Test only on a copied save; never save the owner's real slot.
- Cutscenes: host's StageGameEventAck / sequencer start could trigger ForcePlayStageSequencerDevReq on guest. Unknown if it plays outside the right world state.
Unknown:
- Payload formats of all of the above (need captures with [sniffer] record=1 and/or disassembly of the serializers).
- Whether quest ids are the same keys on both PCs (expected: static data keys, same game build -> likely same; unverified).
Next:
1. Capture session on the test copy with record=1 while finishing one quest step / one camp gauge.
2. Disassemble CompleteQuestByCheatReq serializer (writer fn) for its payload layout.

## 2026-10-19 — quest/stage Req handlers: stub or live (static, capstone)
Method: each C->S Troc has a Process function (vf2, RVA in cdmp's troc_messages_2976.csv "handler_slot2").
It deserializes fields with Deserializer vfunc [+8] (r8d = byte count) then calls the game logic.
Linear disassembly to the first int3; a handler whose only calls are deserialize/vector/free/error-report = "stub".
Control: KillCheatReq (known to work in cdmp) -> one extra call exe+0x2C40C00 after parsing = live. Scripts: tools/re/ (dis.py; the first-pass calls.py/classify.py were wrong, see the correction below, and are not kept).

Observed (derived from disassembly, build 1.0.0.2976):
- STUB in retail (parses, does nothing): CompleteQuestByCheatReq 2627, ForcePlayStageDevReq 2885, ForcePlayStageSequencerDevReq 2419,
  ChangeGameLevelStateByCheatReq 2281, MovePlayerByCheatReq 2860, LearnKnowledge*ByCheatReq, ResetHistoryByCheatReq, SetQuestDialogReq (7 insns).
  Indirect calls checked for CompleteQuest/ForcePlay*/ChangeGameLevelState: none beyond parse + error path.
- LIVE: StageChartForceBranchDevReq 2476, ResetQuestByCheatReq 2251, StageBranchStateReq 2436, StageBranchStateDelayEndReq 2105,
  UpdateStageChartDelayedEventReq 2704, StageQuitReq 2325, ReloadStageStartReq 2283, UpdateStageListReq 2454, QuestDialogEndReq 2342,
  QuestDialogInterrogationAnswerCompleteReq 2554, UpdateSequencerGamePlayDataEventReq 2173, RegisterGlobalStageSequencerReq 3046.
- Payloads decoded:
  - CompleteQuestByCheatReq: u8, u8, u8, then array to end of payload of 14-byte {u64, u32, u16 (default 0x95)}. (Moot: stub.)
  - StageChartForceBranchDevReq: u64 A, u32 B, exactly 12 bytes. Calls exe+0x2892FE0(stageMgr = session->vf[0x48/8], &res, session, A, B).
Derived:
- Quest progress is stage-chart driven. "Complete quest" shortcut is gone; forcing branches (dev Req) or replaying the host's own
  StageBranchState*/QuestDialogEnd Reqs to the guest's local server are the two candidate levers.
Unknown:
- Meaning of A/B (guess: stage chart key + branch id). Need a capture of normal play with record=1.

## 2026-10-19 — first quest capture (one PC, cdmp sniffer record=1, ~13 min, one quest step done by a person)
Observed (from cdmp_packets_*.log, payload previews; little-endian):
- Counts: UpdateStageStateAck 72k (noise), SetQuestDialogReq/Ack ~900 each, UpdateStageChartEventReq 823, StageBranchStateReq 53,
  UpdateMissionListAck 98, UpdateStageListAck 82, UpdateQuestListAck 2, NotifyQuestSaveDataAck 2, UpdateQuestGaugeListAck 1.
- Quest step finished at t=298.98 (all in the same ms):
  - UpdateQuestListAck len 41: `04 | 01 00 10 A0 (player 0xA0100001) | 00 01 | A5 45 0F 00 (quest 0x0F45A5) | 0x00 x12 | A5 EE 0C 00 (0x0CEEA5) | 0x00 x12 | 02 01`
  - UpdateStageListAck len 12383 (also contains 0x0CEEA5), UpdateMissionListAck len 3802 (mission 0x0F4AB4 first), ~45 EraseNotNoticeChallengeMissionAck,
    then NotifyQuestSaveDataAck len 19.
  - At load (t=260) the same UpdateQuestListAck was a full list (len 22753).
- Stage messages carry small runtime instance ids, not static keys: StageBranchStateReq len 14 = `u64 instance (0x158, 0x17C, ...) | u32 | u16`;
  UpdateStageChartEventReq len 17 = `u64 instance | u32 | u32 | u8`.
  StageBranchStateAck / PrepareSequencerAck pair those ids with static keys (0x0F496C, 0x0F4CB1, 0x0F42DB ...) and 32-bit hashes (0x22B6EC91).
Derived:
- Quest/mission keys (0x0F4xxx range) look like static data keys -> expected to match between two PCs on the same build (unverified with a 2nd PC).
- Stage instance ids are per session/PC -> relaying the host's StageBranchStateReq / StageChartForceBranchDevReq raw would hit the wrong stage.
  A guest-side map static stage key <-> local instance id is needed (from the guest's own StageBranchStateAck / PrepareSequencerAck / UpdateStageListAck).
- Candidate sync unit: host's UpdateQuestListAck/UpdateMissionListAck deltas (static keys + state) -> guest maps to local stage instance -> StageChartForceBranchDevReq(u64 instance, u32 branch).
Guess:
- Trailing `02 01` in the quest delta = new quest state 2 (completed?) + flag. Unverified.
Unknown:
- Full layouts of UpdateQuestListAck / UpdateMissionListAck / UpdateStageListAck elements (need the .bin full packets + client handlers 0xBF86C0 / 0xBF9700 / 0xBF8070).
- Whether StageChartForceBranchDevReq's u64 is the runtime instance id (likely, same width/position as StageBranchStateReq) and what the u32 branch values are.
Next:
1. Decode the three list Acks fully from the .bin + client handlers.
2. Single-PC experiment on the test copy (no saving): inject StageChartForceBranchDevReq for a known instance/branch and watch the Acks.

## 2026-10-19 — list Ack / branch layouts (capture + client handlers 0xBF86C0, 0xBF9700, 0xBF8070)
Observed:
- Common header of the S->C list Acks: `u8 scope (4) | u32 player key | u8 a | u8 b`. Full list at load: a=1 b=0; delta: a=0 b=1.
- UpdateQuestListAck element = 34 bytes `{u32 quest key, u32, u64, u64 f3, u64, u8 state, u8 flag}` (handler divides by 34).
  Full list at load: 669 quests, state histogram {2: 618, 5: 25, 3: 17, 1: 9}. Quest step delta added quest 0x0F45A5 (absent at load) with f3=0x0CEEA5, state 2, flag 1.
- UpdateMissionListAck element = 55 bytes (handler magic 0x29E4129F = /55): `u32 mission key | u8 1 | u8 state ... | u32 parent key at +30`.
- UpdateStageListAck: handler magic 0x4EC4EC4F = /52, but the data rows repeat every 56 bytes (221 elements in 12376 bytes): `u32 stage key (0x1003A3...) | u8 state | ... | u32 chart key 0x0CEEA5 at +26`. Stride mismatch is unexplained (unknown).
- StageBranchStateAck 2981 (len 29 or 37): `hdr5 | u64 instance | u32 state (0 waiting, 1 branched, 2 ?) | u32 branch (FFFFFFFF = none) | u32 chart-type hash | [u32 hash2, u32 counter]`.
- StageBranchStateReq 2436 (len 14): `u64 instance | u32 branch | u16`. Answer within ~10 ms: Ack(state 1, branch b) + StageOnBranchEventAck 2524 `hdr5 | u64 instance | u32 branch | u8`.
- 53 Req / 53 OnBranch pairs in the capture; branches seen: 0 and 5.
Derived:
- StageChartForceBranchDevReq (u64, u32) has the same shape as StageBranchStateReq -> u64 = runtime stage instance id, u32 = branch. Test below.

## 2026-10-19 — StageChartForceBranchDevReq live test (cdmp stage_force_test, test copy, opcode 2476)
Checks that actually ran (cdmp.log + capture cdmp_packets_20261019-121524):
- Negative control PASS: ForceBranch(instance 0x7FFFFFF0, 0) injected (seen C>S in capture), no answer in 3 s.
- Real test FAIL: stage 0x23 (hash 0x22B6EC91) waiting 21 s (Ack state 0, branch FFFFFFFF); ForceBranch(0x23, 0) injected (seen C>S at t=66.456),
  no StageBranchStateAck / StageOnBranchEventAck for 0x23 for the rest of the session, no game error logged.
  The game's own StageBranchStateReq (0x15D/0x15E/0x15F at t=91) was answered in 5 ms, so the server was serving branch requests.
- Not yet run: mutation run with a stub opcode (pointless while the real test fails).
Derived:
- The retail force-branch path either does nothing visible for this stage, needs a different u32 (branch index vs. branch key?), or refuses without an error. Unknown which.
Crash in the same run (12:18:12, 90 s after the injection, right as the player opened the menu):
- Proton log: vkd3d-proton "Device is lost ... Failed to execute pending memory clears" / VK_ERROR_DEVICE_LOST, then access violation in the game at exe+0x3BD10E0
  (not in cdmp.asi). The game's crash reporter ("[Crash] 덤프 보내는 중..." = sending dump) ran.
- Earlier runs with cdmp + overlay: 18 menu openings, no crash. This run also changed the window setup (Wine virtual desktop removed, game Windowed 1280x720).
- Cause: unknown. Candidates: window setup change, the forced branch, cdmp overlay, plain vkd3d/driver issue.
Isolation run (12:21-12:30, same window setup, same cdmp build + overlay + recording, stage_force_test=0):
- World loaded (12:23), 5 menu openings, normal quit ("Terminate 시작."), no [Crash], no DEVICE_LOST, no c0000005 in the Proton log.
- Test-copy save slots were unchanged by the crash run (diff vs. backup: only the engine option xml we edited).
Derived (weak, one run each): the window setup + overlay alone did not reproduce the crash in 5 menu openings; the forced branch
on stage 0x23 remains a suspect, alongside chance. Not proven either way.

## 2026-10-19 — CORRECTION: handler stub/live classification redone with exact function bounds
Mistake: the first-pass classify.py and calls.py scanned linearly to the first int3. Handlers here are not separated by int3
padding, so scans ran into the NEXT function and borrowed its calls. Fixed in classify2.py: bounds from the .pdata
RUNTIME_FUNCTION table (244,444 entries). All "live" verdicts above that came from classify.py are withdrawn.
Observed (classify2.py, exact bounds):
- STUB: StageChartForceBranchDevReq 2476 (0x2A5D330-0x2A5D530), ResetQuestByCheatReq 2251, CompleteQuestByCheatReq 2627,
  ForcePlayStageDevReq 2885, ForcePlayStageSequencerDevReq 2419, ChangeGameLevelStateByCheatReq 2281.
- LIVE: StageBranchStateReq 2436 -> 0x2891FB0, StageBranchStateDelayEndReq 2105 -> 0x2892210, UpdateStageChartDelayedEventReq 2704 -> 0x28928D0,
  StageQuitReq 2325 -> 0x28934F0, ReloadStageStartReq 2283, UpdateStageListReq 2454 -> 0x287D3C0, QuestDialogEndReq 2342 -> 0x2904F10,
  QuestDialogInterrogationAnswerCompleteReq 2554, UpdateMissionUIStateListReq 2477, UpdateSequencerGamePlayDataEventReq 2173,
  ChangeSequencerPivotReq 2888, RegisterGlobalStageSequencerReq 3046. Controls: KillCheatReq, VaryStatCheatReq live (as cdmp uses them).
- The function analysed as "force branch" (0x2892FE0) is SubTimelinePauseReq's (2406): it adds a u32 to a per-stage set
  (+0x158 data, +0x160 count, gated by byte +0x28B, marks +0x16 dirty) and always returns 0. SubTimelineResumeReq (2968) removes it
  via 0x2893260. 0x287CEBF (in ~0x287C200) checks the set while walking per-branch records (u32 at rec-4).
- So the 12:16 live FAIL is explained: StageChartForceBranchDevReq is a retail stub. That run doubles as the mutation/negative run.
Derived:
- Every quest/stage cheat is gone in retail. The remaining lever is the game's own StageBranchStateReq (u64 instance | u32 branch | u16),
  sent by the guest's mod with the guest's local instance id. The stage_force_test driver now defaults to opcode 2436 (14-byte payload).
Not yet run: the StageBranchStateReq live test.

## 2026-10-19 16:18 — first "unattended" run: injected StageBranchStateReq crashes the game (autoload did NOT work)
Setup: cdmp + new test_input.cpp ([dev] test_autoload=1: in-process Raw Input synthesis, WM_INPUT with a marker handle
answered by a GetRawInputData hook, GetAsyncKeyState/GetForegroundWindow/GetFocus faked only during autoload;
[dev] test_autoquit=1). Stage test with opcode 2436 (StageBranchStateReq, 14-byte payload).
Observed:
- Autoload: Enter every 4 s from 15 s after the window appeared; the world started loading after 14 presses, BUT the owner
  entered the world by pressing E (the game's confirm key on the title/menu). So this run had human input, and whether the
  synthesized Raw Input reaches the game at all is unknown (Enter may just do nothing there).
- Negative control PASS (made-up instance 0x7FFFFFF0, no answer, no crash).
- Target: stage 0x58 (hash 0x22B6EC91), reported waiting (Ack state 0, branch FFFFFFFF) for 21 s; injected StageBranchStateReq(0x58, branch 0, 0).
  53 ms later: access violation (null read) on the main thread at exe+0xAA1B4D in function ~0xAA1A10:
  `rax = [[[r15+0x58]+0x38]+8]` = 0, then `mov rdx, [rax + rcx*8]` with rcx = our branch (0). The game's crash reporter ran.
  No GPU device loss this time. Capture ends just before (last records not flushed before the process died).
- Test-copy save slot1 unchanged by this run.
Derived:
- StageBranchStateReq is not validated against the stage's branch table: an index the stage does not have crashes.
  The game only sends branches it knows exist, so "first waiting stage, branch 0" is not a safe probe.
- The 12:18 menu crash (GPU device lost, 90 s after the stub request) is a different signature; still unknown.
Consequence for quest sync: only replay (instance, branch) pairs the host's game actually sent for the same static stage,
mapped to the guest's own instance id. Never synthesize branch numbers.

## 2026-10-19 16:23 / 16:27 — autoload checks without human input: FAIL (game reads the keys but ignores them)
Observed (two runs, nobody touched the window, test_autoquit closed the game each time):
- 15 synthesized E (0x45) taps (120 ms) at 4 s intervals, starting 15 s after the window appeared. The game read our marker
  30 times per run (one GetRawInputData per down/up, command RID_INPUT 0x10000003, 48-byte buffer = sizeof(RAWINPUT)),
  also with a real keyboard hDevice from GetRawInputDeviceList (run 2). World never started loading: RESULT FAIL.
Unknown (next to rule out): whether the title needs a HOLD of E rather than a tap; whether the game drops input while its
window is not active (tracked via WM_ACTIVATE*/focus messages, not only GetForegroundWindow/GetFocus).
- 16:33 run, owner focused the window (title-bar click) and pressed nothing: same FAIL, 30 reads. Focus alone is not the cause.
- 16:37 run, queued WM_INPUT (PostMessage, per-event marker handles), no human: FAIL. Game read 29-30 events; it calls
  GetAsyncKeyState ~100/s but never for E (0x45). RegisterRawInputDevices hook saw no keyboard hwndTarget (registered before
  our hook or with NULL). Guess: the menus (an HTML/CSS-style UI, "selector-*" class strings in the exe) take WM_KEYDOWN/WM_CHAR.
- 16:42 run, raw events PLUS classic WM_KEYDOWN / WM_CHAR('e') / WM_KEYUP posted to the game window, no human input
  (owner asked to keep hands off): PASS. GameLoadingStartReq after press 9 (~35 s of presses), "in the world" 4 s later,
  test_autoquit closed the game. So the title/menu UI reads the classic key messages, not Raw Input.
  Unattended path into the world: [dev] test_autoload=1 (default key E), test_autoquit=1.

## 2026-10-19 16:46-16:55 — record + replay of stage branches by static key: PASS (unattended, one PC, same save)
Setup: cdmp src/mod/net/stage_replay.cpp. Stage identity = sequencer key from PrepareSequencerAck (`hdr5 | u32 player |
u64 instance | u16 | u32 key`), stable across sessions; instance ids are not (key 0xF4CB1 was 0x159/0x2E0, 0x163, 0x164).
Test-copy saves backed up before, restored between the two sessions (record session left them unchanged).
Observed (no human input; autoload 8 presses each time, test_autoquit closed the game):
- Record session ([dev] stage_record=1, 240 s idle in the world): the game itself sent StageBranchStateReq for keys
  0xF4880, 0xF4CB1, 0xF4CB5: branch 0 first, then branch 1 repeatedly (~16 s apart). cdmp_stage_record.txt, RESULT PASS.
- Replay session ([dev] stage_replay=1, same save): key 0xF4CB1 came up as instance 0x164, waiting since t=48.9 s.
  2 s+ later the mod sent StageBranchStateReq(0x164, 0, 0) at t=61.326 — the first branch request for 0x164 in the
  session. Server: StageOnBranchEventAck + StageBranchStateAck(state 1, branch 0) at t=61.335 (9 ms). Then the GAME continued
  by itself with branch 1 for 0x164 at t=78.0, as in the recording. Game still running 30 s later: RESULT PASS x2.
  Verified in the packet capture, not only the driver's log.
Derived:
- Replaying a branch the game really chose, onto the local instance found by static key, is accepted and the stage chart
  continues from it. This is the mechanism for "guest follows host" on stages both worlds have.
Limits (honest): one PC, one player, same save, one stage, one run. Not yet: a second PC/player, stages where host and guest
are at different story points, quest/mission list deltas, rewards. Mutation counterpart already seen: a made-up branch
(16:18) crashes the game.

## 2026-10-19 17:08 — live stage sync over cdmp's network, one-PC echo test: PASS (2 genuine applications)
Code: common/stage_sync.h (guest queue, unit-tested + mutation check: removing the retry wait fails tests.cpp:1067/1069),
proto RelKind::StageBranch = 9 (u32 sequencer key | u32 branch | u16 flags, reliable), kVersion 10, mod/net/stage_tracker
(shared instance<->key bookkeeping), mod/net/stage_sync ([coop] stage_sync=1; [dev] stage_sync_feed=1 = echo test feed).
Observed (cdmp_peer --mode relay --delay 50, unattended, saves restored to the record/replay start point):
- Feed of 3 recorded branches as the host's at in-world+20 s -> relay (86 reliable fragments, 0 dropped) -> guest half:
  key 0xF4CB1 -> our instance 0x164, 0xF4CB5 -> 0x165, both sent at t=85.607 (first branch requests for them),
  server OnBranch + Ack(state 1, branch 0) at t=85.615. Game still running 30 s later. Capture-verified.
- Race found: the game's own branch of 0x163 (key 0xF4893, t=91.453, answered 91.512) came back through the echo and the guest
  sent the same branch again at 91.515, before the tracker (sniffer writer thread, a few ms behind) saw the answer. Harmless
  (identical branch) but the driver counted it as "applied 3". Fixed: a stage whose own branch request is unanswered is "busy"
  and not touched (unit test added). Confirmation run below.
Observed by the owner (watching the window): the echo copy of the player animates choppily and sometimes teleports.
  Log: 4001 events replayed, clock offset 0 ms, nothing dropped on the link; 1584 of 3891 movement positions dropped as
  repeats; test config uses coop delay_ms=1500 and offset 0.01 m. Cause unknown; cdmp's author also noted occasional teleports.
- 17:12 confirmation run with the busy fix (same setup): sent 2, applied 2 (0xF4CB1 -> 0x164, 0xF4CB5 -> 0x165), the game's
  own branches coming back through the echo counted as "reached by our game alone" (4) and not re-sent. Game alive 30 s later.
  Unit tests pass (fresh binary). Limits: one PC, echo relay, same save on both "sides", first branch of a stage only.
- 17:18 replay rework (all recorded branches through StageSyncQueue; quest list deltas recorded as "Q key state flag" and
  used as the oracle): regression run on the ambient-stage recording, unattended: applied 2 (0xF4CB1 -> 0x163, 0xF4CB5 -> 0x164,
  ids differ again), reached alone 21, PASS; alive 30 s later. No quest change in that recording, so the quest oracle is untested.

## 2026-10-19 — where a bad branch is (not) validated (static)
Observed:
- StageBranchStateReq's handler calls exe+0x2891FB0, which has no .pdata entry: it is a single `jmp 0x1502D2B70` into .xtls
  (RVA 0x75C9000, ~250 MB, characteristics 0xE0000020), followed by non-code bytes. The server-side branch logic is virtualized
  (protector VM); not readable with plain disassembly. (tools/re/fn.py: .pdata-bounded single-function disassembly.)
- The 16:18 crash site exe+0xAA1B4D is in a normal client function (0xAA1A10-0xAA1E5C) that indexes a branch table
  ([[[r15+0x58]+0x38]+8] + branch*8) while handling the answer.
Derived:
- The server accepted the made-up branch and answered; the client then crashed applying it. A safety guard for stage sync must
  therefore be client-side: read the stage's branch table (count/pointer) before sending, and refuse out-of-range branches.
  Not built yet; same static key on both PCs should mean the same chart and branch set, so this matters for mismatched builds/data.

## 2026-10-19 17:24 — stage sync echo test under a lossy link: PASS
cdmp_peer --mode relay --delay 60 --loss 5 --jitter 40 (relay dropped 420 of 8518 packets, 4.9 %), unattended: all 3 fed
branches arrived over the reliable channel (71 fragments), applied 2 (0xF4CB1 -> 0x163, 0xF4CB5 -> 0x164) 100 ms after the feed,
own echoed branches reached alone 2, game alive 30 s later.

## 2026-10-19 — the echo copy's stutter/teleports (separate read-only investigation, cdmp-puppet-findings.md, local)
Summary of its findings (code refs checked here: inject_clock.h:27 kAheadMs=450; coop.cpp:121-124 echo offset only when offset_m==0):
- Our test config caused much of it: delay_ms=1500 vs the 450 ms "ahead" clamp re-stamps every Move (timing margin pinned at
  650 ms, ~40 % dropped as repeats, actions ~1 s behind positions); offset_m=0.01 disables the 2.5 m echo offset, so the copy
  stands in the player and shoves them (player drifted ~25 m with no input after the copy spawned).
- Real cdmp issue at default settings too: Moves arrive at ~83 Hz and are never thinned; the client's 30-sample interpolation
  buffer overflows (offline model of the documented client algorithm: ~3700 evictions per run at delay 300), giving stall-then-
  jump. The author's own proposal P2 (drop Moves < 22 ms apart) was never implemented; the model predicts 0 evictions with it.
- Test settings changed back to delay_ms=300, offset_m=0. Fixes not implemented or tested yet; a live check needs a walking bot.

## 2026-10-19 17:32-17:45 — copy stutter: Move thinning (author's P2) + walking bot: PASS
Code: common/move_thinner.h (drop a player Move < 22 ms after the last kept unless first after a 150 ms gap, parent change or
out of order; a dropped Move is held and sent after 60 ms of silence), unit tests + mutation check (removing the parent-change
rule fails tests.cpp:1058); remote_puppet.cpp applies it to the player's Moves only ([coop] move_thin, default 1);
test_input: [dev] test_walk_s (holds W 4 s / S 4 s via WM_INPUT + WM_KEYDOWN auto-repeat; ClipCursor/SetCursorPos are no-ops
while focus is faked); net/puppet_motion: samples the copy's position ~every 20 ms, counts jumps (> 15 m/s) and
stall-then-jump (still >= 150 ms, then > 10 m/s); 3 s warm-up added after run 1 showed the first-Move snap.
Observed (echo relay, delay_ms 300, offset_m 0, 60 s of bot walking, unattended; the bot moves the player: 90-103 m per run;
the game reads movement keys from window input, never GetAsyncKeyState):
- move_thin=0: run 1 jumps 5 (first at +2.5 s) FAIL; run 2 jumps 14 + 1 stall-then-jump, max 83 m/s, FAIL.
- move_thin=1: run 1 jumps 2, both in the first 0.3 s (first Move after rest) and 0 afterwards; 1945 Moves dropped as too
  close, 3 held-and-released at stops. Run 2 (with warm-up): jumps 0, stall-jumps 0, max 9.8 m/s, copy 91.9 m vs walk 91.6 m: PASS.
Limits: one PC, echo relay, flat walking only (no horse, climbing, combat), two runs per side.

## 2026-10-19 — client branch table (for a future guard), static
Observed (client fn 0xAA1A10-0xAA1E5C, normal code):
- Same pattern as the server side: lock on [mgr+8], then stage lookup exe+0x1F6C190(mgr, &out, instance) -> stage object (r15).
- Branch pointer = [[[stage+0x58]+0x38]+8][branch]: the chart object ([stage+0x58]+0x38) holds a {data ptr, u32 count} vector at
  +8, so the count is at chart+0x10 (same {data, count} layout is read at [[r13+0x50]+0x10]/+0x18 a few lines later).
  For stage 0x58 (16:18 crash) the data pointer was null: that stage has no branch table, so any branch crashes.
Derived: a guard is `data != null && branch < count`. Not built: it needs the client stage manager + lock to call the lookup
from the mod, and it only matters if the two games' data differ, which cdmp already rules out (same build, same mod version).

## 2026-10-19 17:51-18:03 — running (Shift held) with and without thinning
Observed (echo relay, delay 300, unattended, 60 s runs; Shift+W/S covers 218-333 m vs ~90 m walking, so Shift does run):
- First pair, old meter (GetTickCount64 timing, speed-only jump rule): 20 vs 17 "jumps". The thinned run's were all
  0.34-0.56 m per ~22 ms: 2-3 game frames at a run timed with a ~16 ms clock, i.e. meter noise; the unthinned run also had
  real ones (1.11, 0.83, 1.81 m).
- Meter fixed (QueryPerformanceCounter timing; a jump must also exceed 0.75 m): move_thin=0 jumps 1 (1.13 m) + 1 stall-then-jump
  (0.23 m after 171 ms); move_thin=1 jumps 1 (0.88 m) + 1 stall-then-jump (0.25 m after 193 ms). Both "FAIL" by the strict rule.
Derived: at a run, thinning makes no measurable difference; about one small event per minute remains either way, not tied to
starting or stopping. Walking results above were measured with the old meter, but their jumps were 0.5-1.8 m (baseline)
vs none (thinned), so they stand. Cause of the running residue: unknown.
