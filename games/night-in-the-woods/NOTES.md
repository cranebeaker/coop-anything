# Night in the Woods

## Identity (observed)

- Original Steam PC release, app 481510, build 5940043.
- Main executable SHA-256: `71C33A2723205469AF2B252F313B8AD051FE4F98ED456C2DB33B969853FE0594`.
- Windows x64; Unity 5.6.2p4; Mono scripting backend.
- The game's `Assembly-CSharp` references Rewired and Yarn dialogue code. The assembly has no reference to Unity networking. The presence of `UnityEngine.Networking.dll` does not establish game networking.

## Existing work (checked 2026-10-19)

- No directory entry or game folder was present in this checkout.
- GitHub issue/PR searches scoped to `cranebeaker/coop-anything` found no other Night in the Woods work before claim #69 was opened.
- Claim: [#69](https://github.com/cranebeaker/coop-anything/issues/69).
- The GitHub issue/PR and repository searches above were completed. ModDB, Discord and archival sources have not been searched; their coverage is unknown.

## Observed game structure

- `Player.GetAxisRaw` reads a static Rewired player. Movement reads `Horizontal` and `Vertical`; jump reads `Jump`; interaction is handled through dialogue/game input helpers.
- `Dialogue` uses Yarn's `DialogueRunner`; `DialogueImplementation` owns confirmation and option navigation; `DialogueVariableStorage` delegates story variables to `Continuity`.
- `Continuity` owns story variables, persistent integers, journal progress and scene data. It writes player and persistent data as `.dat` files and `persist.dat` through `GetSaveDataPath`, which on Windows uses Unity's `Application.persistentDataPath`.
- The game was launched from the isolated copy with the preloader patcher active. Its log confirms the save-path rewrite was installed before Assembly-CSharp loaded. The co-op plugin then redirected Continuity save paths to a per-test folder inside the isolated copy. The test folder remained empty during startup; existing user-save file timestamps were unchanged (metadata only checked; contents were not read).

## Derived direction

- Start with host-authoritative shared-story co-play: one Mae, one canonical story and save, with participants contributing movement and interaction input.
- A second visible character is a later, substantially larger scope because it needs separate movement, collision, animation, camera and scripted interaction support.
- The game's story timing and event execution may not be deterministic. Input mirroring alone is not proof of shared progress; scene, dialogue and `Continuity` state need explicit checks and possibly synchronization.

## Unknown / not established

- Whether the host scene and position snapshots keep clients aligned through normal, unscripted story progression.
- How to keep choices, timed scenes, scripted events, journal/persistent counters and minigames consistent without duplicate story effects. Numeric `Continuity.vars` now has host-authoritative snapshots, but its complete story-flow behavior has not been tested.
- How the mod behaves on another machine, over a real network, with packet delay/loss, or after a disconnect.
- The Rewired action map does not contain actions named Confirm or Submit. Confirmation uses the existing `Interact` action. One isolated host startup passed; a remaining startup warning is HarmonyX checking a Unity property absent in this build.
- The two-client in-game bot smoke gate ran on 2026-10-19 and 2026-10-20. It verified title confirmation, host scene/position following and numeric story-variable correction described below. The mutation-twin test of full game progress and full automated suite have not run; no playability claim is made.

## Prototype observation (observed 2026-10-20)

- The local prototype now uses protocol v3: the host sends merged input, active scene, Mae's world position and host-authoritative snapshots of the game's numeric `Continuity.vars` dictionary. Story dictionaries are split across bounded UDP fragments and clients replace their local copy only when a full snapshot arrives. The client corrects its player transform after the game's player update and requests the host scene when its local scene differs.
- Two instances from the isolated copy connected over loopback. With the QA-only `scene:MaeRoom` setup command, both loaded `MaeRoom` and reported an enabled player.
- A client bot `move_right:45` and a host bot `move_right:45` each moved Mae. After each pulse ended, the client and host positions were within 0.02 world units of each other.
- A host-only `scene:BusStation` command moved the client into `BusStation`; after loading, their reported player positions differed by about 0.001 world units.
- A QA-only client `mutate_first_var` command changed the first sorted story variable, `AngusConstellations`. The next host snapshot restored the client's host-authoritative dictionary; both statuses reported 316 variables and hash `0F68BB8C`, and the client reported `host snapshot applied`.
- `scene:<name>` is a QA setup shortcut that stops local dialogue and invokes the game's own `Global.LoadLevel` routine. The scene tests bypassed normal story progression; the variable test is an isolated dictionary mutation, not a normal dialogue-choice test. These checks do not show that choices, scripted events, or saves stay synchronized.
- This variable mutation validates dictionary replication only. It does not validate normal choice flow, all save/persistent state, scripted events or end-to-end co-op play.

## Dialogue option sync work (observed 2026-10-20)

- Static inspection of the game's `DialogueImplementation+<RunOptions>c__Iterator5.MoveNext` showed that visible option navigation uses `Rewired.Player.input.GetAxisRaw("Horizontal")` directly and updates `DialogueImplementation.currentOption`; it is outside the existing `Player.GetAxisRaw` co-op patch.
- The game uses a `runningOptions` flag to distinguish active option selection. The host option state can be read from `Global.dialogue.implementation` through reflection, avoiding a compile-time dependency on dialogue implementation internals.
- The local prototype sends `runningOptions`, `currentOption` and `Dialogue.runningNode` in host state packets (protocol v6). Each host option menu has a session id so a delayed commit from a previous menu in the same Yarn node cannot be mistaken for the current choice. On the client, a Harmony prefix on the game's option iterator adopts the host cursor only when the Yarn node matches, clamps it to the local option count, and clears local arrow-step flags before each iterator step.
- Static inspection showed `Yarn.Unity.Example.DialogueUI.SetOption(int)` invokes Yarn's `OptionChooser` callback directly. A Harmony prefix now records the host's committed option index, Yarn node, option-session id and monotonically increasing commit id. The host repeats this commit in state packets; the client substitutes that index at `SetOption` only once and only when its running Yarn node and option-session id match.
- The client defers a rising host-confirm event for up to 0.5 seconds when the host snapshot still shows an active option menu without a new commit for that menu session. A matching-session commit releases the confirm; if none arrives, the guest continues with its local cursor and sets a status warning rather than waiting forever.
- The prototype built successfully against the isolated game assemblies. A local protocol v6 check round-tripped client and host packets; the host packet preserved cursor index `3`/node `poem_1`/session `42`, and commit id `17`/index `2`/node `poem_2`/session `42`. All truncated prefixes of the host packet were rejected; five offline node/session predicate cases passed.
- This change has not been exercised in a live dialogue with two game clients. It does not prove identical selected-option visuals, matching branch effects or synchronization of scripted effects. A normal-flow, bot-driven choice check remains required before claiming choice sync.
- Dialogue cursor and committed-choice synchronization have only passed compilation, protocol checks and offline node/session predicate cases. They have not passed a live choice test with two game clients; matching story branches and side effects remain unverified.

## Latest live loopback check (observed 2026-10-20)

- Two isolated game folders with separate BepInEx configurations connected over loopback using protocol v6. The client game data was copied from the isolated host install.
- The host's QA scene setup moved both clients to `MaeRoom`; a host change to `BusStation` moved the client there with matching reported positions. In `MaeRoom`, 45-frame host and guest movement pulses each moved Mae; host/client positions differed by less than 0.01 world units after each pulse.
- Synchronization is still incomplete. After a guest `mutate_first_var` command in `MaeRoom`, the client reported `host snapshot applied` but its whole-dictionary hash (`B7EDB636`) differed from the host (`5DA3C071`) for at least six seconds. After forcing host `Title`, hashes agreed (`9662F8C5`) but the client remained in `TransitionScene` while the host reported `Title`. A host confirm was observed by the client but did not advance the host's title menu.
- The v0.6.0 dialogue-choice path has not been exercised in a live option menu. Normal choice flow, matching branches, scripted effects and save progression remain unverified; no playability claim is made.

## Follow-up loopback check (observed 2026-10-20)

- A v0.6.1 build adds a 1.5-second retry when a client remains in a scene different from the host's requested scene, and QA status output now includes the first sorted story variable and live dialogue option/session/commit fields.
- After copying game data into a second isolated folder and using separate BepInEx configurations, two v0.6.1 clients connected over loopback. A forced host transition from `MaeRoom` to `Title` brought both clients to `Title`; a guest confirm continued the isolated save and moved both to `MaeRoom`, then host movement reached `MaeHouse` with both clients following.
- QA guest movement pulses in `MaeRoom` moved Mae on both clients; reported positions differed by less than 0.01 world units. A host scene change to `BusStation` produced matching reported positions.
- QA story-variable mutation in `Title`, `MaeRoom` and `MaeHouse` was corrected in repeat checks: `AngusConstellations` returned to `0`, and the complete host/client variable hashes matched after snapshot application. An earlier v0.6.0 run showed a temporary whole-dictionary hash mismatch in `MaeRoom`; v0.6.1 status now records the first sorted variable and hash for follow-up.
- The final v0.6.1 DLL was rebuilt, copied to both isolated clients and confirmed loaded from both BepInEx logs. Repeating `MaeRoom` → `Title` with that build left both clients in `Title` with matching variable hashes. The QA option-menu/session fields were present but remained inactive; no live choice menu was reached.

## Sources

- [Steam store listing](https://store.steampowered.com/app/481510/Night_in_the_Woods/)
- [Game's repo directory](https://github.com/cranebeaker/coop-anything/blob/main/DIRECTORY.md)
- [BepInEx 5 release](https://github.com/BepInEx/BepInEx/releases/tag/v5.4.23.5)
- Local static inspection of the isolated game build; machine-specific dumps and paths are excluded from this repository.


## MaeRoom to MaeHouse interactive follow-up (observed 2026-10-20)

- Repeated the loopback run with the v0.6.1 DLL loaded in both isolated folders. A 45-frame host movement pulse in `MaeRoom` moved both clients to x=2.791 with matching positions. A 90-frame rightward pulse moved both into `MaeHouse` at x=9.855, y=-1.373.
- In `MaeHouse`, a 90-frame host leftward pulse did not change either reported position. Host and guest `interact` bot commands were received, but neither exposed a Yarn option menu: `optionsActive` stayed false and the option node/session and choice commit fields stayed empty/zero. No live dialogue-choice synchronization result is claimed.
- Both clients remained connected, in the same scene, and reported matching `Continuity.vars` hash `2F4B5739` during this check. The failed movement/interaction is limited to this scripted scene state; the cause is unknown.

## Fresh-save opening choice check (observed 2026-10-20)

- Rebuilt the local prototype as v0.6.3 and launched a host and guest from separate isolated folders on loopback. A guest confirmation from `Title` advanced both to `IntroPoem`; additional guest confirms advanced the poem on both peers.
- A host right-arrow input followed by guest confirm reached the patched `DialogueUI.SetOption` path. Both status files recorded commit id `1`, option session `1316`, choice index `1`, an empty Yarn node, and story-variable hash `236FB091`; the guest logged no missing-commit warning. The current matching rule permits the session id when both node names are empty.
- This confirms the commit was carried to both peers in the opening sequence. The client’s actual substituted callback index and subsequent branch effects are not directly exposed by status, so identical story-choice outcomes are still unverified. No playable-co-op claim is made.


## v0.6.4 choice-application diagnostic run (observed 2026-10-20)

- Rebuilt the co-op prototype as v0.6.4 and added guest status fields for the last applied choice commit id, active client option session and deferred host confirm.
- Launched host and guest from separate isolated game folders; both reported connected over loopback on UDP 39752 and reached `IntroPoem` after a guest confirmation. Both reported the same `Continuity.vars` hash after the transition.
- During additional guest-confirm bot inputs, the host recorded choice commits 1 and 2 at option session 245, index 0 and blank Yarn node. At the final status sample the guest had received commit 2 but `lastAppliedChoiceCommitId` was still 0, `clientActiveOptionSession` was 0, and `optionsActive` was false on both peers.
- The sample does not establish that the callback ran while an option menu was active. It does show that receipt of the commit packet alone is insufficient evidence of client-side choice application. The callback index, selected branch and side effects remain unverified; no playable-co-op claim is made.
- A first launch attempt used a temporary status-file override that was not inherited when the clients were relaunched through the UI, leaving the custom status files stale. The successful sample used the plugin's normal `CoopStatus-Host.txt` and `CoopStatus-Client.txt` files. The guest log recorded one UDP receive warning before both peers connected.


## v0.6.5 choice-application follow-up (observed 2026-10-20)

- Changed the guest's `DialogueUI.SetOption` fallback to accept a committed host choice when the host option-session id and Yarn node match, even if the guest's `runningOptions` flag has already gone false. Added a guest log entry when the callback substitutes the host index.
- Rebuilt v0.6.5 and confirmed both isolated clients loaded it. The host and guest connected over loopback UDP 39752 and a guest confirmation moved both from `Title` to `IntroPoem` with matching story-variable hash `236FB091`.
- In the opening-scene bot run, the host recorded choice commit 1 for option session 251 at index 0 with an empty Yarn node. The guest status advanced `lastAppliedChoiceCommitId` to 1, and its log said it applied commit 1 at index 0 for session 251. Both peers reported `IntroPoem` and hash `236FB091` afterward.
- This is direct evidence that the guest's `SetOption` callback substituted the host's committed index for this index-0 choice. The selected dialogue branch and scripted side effects were not independently observed; nonzero choice selection, normal player input, broader story progression, saves and end-to-end co-op remain unverified. No playable-co-op claim is made.
- Eight subsequent guest-confirm bot pulses did not create another `SetOption` commit or advance the scene; both clients remained in `IntroPoem` with commit id 1 and matching hash `236FB091`. The reason is unknown.

## v0.6.5 progression and bot-confirm diagnostics (observed 2026-10-20)

- In the still-running v0.6.5 pair, the host's QA-only scene command moved both clients from `IntroPoem` to `MaeRoom`; both then reported an active player at the same position. Two sequential host `move_right:90` bot commands moved both to `MaeHouse` with the same reported position (x=9.855, y=-1.373). After an earlier scene sample the host and guest story-variable hashes differed (43AD2F28 and 23FC1D70); a later sample reported matching hash C80A7CF8. Convergence timing and cause were not established.
- Static inspection of the isolated game assembly shows `SupplementalIntroBrain` fades in, waits for `Global.IsConfirmOrInteractDown()`, fades out, then calls `LoadLevel(sceneToLoad)`. In a live bot sample, a host `confirm` command was sent to the guest (its host-confirm packet count and confirm-observed count increased), while the host confirm-observed count did not increase and both remained in `IntroPoem`. This exposed a host-side QA bot input gap; it does not establish the behavior of physical host input or guest-to-host confirm in this scene.
- A local v0.6.6 source change adds a one-shot host bot-confirm path and a consumed-confirm counter; it compiled against the isolated game assemblies. The running pair still had v0.6.5 loaded, so the change has not yet been exercised in-game.
- One later `interact` bot sample in `MaeHouse` left the option menu inactive; the reported player position was below the starting floor. No choice or branch result is claimed. Normal story progression and reliable interaction remain unverified.

## v0.6.6 relaunch check (observed 2026-10-21)

- After the prior client pair was closed, the v0.6.6 DLL was staged in both isolated folders. Startup logs for both instances report that v0.6.6 loaded.
- The first relaunch reported mismatched ports (host 39753, guest 39752), so those clients did not connect. This was a configuration mismatch, not a test of the matched-port case.
- A host `confirm` bot command at the title screen left `botConfirmConsumed=0` and `confirmObserved=0`. The title screen did not exercise the patched `Global.IsConfirmOrInteractDown` hook; this is not a pass or fail for that change.
- The QA bot fix compiled, but its in-game behavior remains unverified. Normal story choice branches, side effects and end-to-end co-op remain unverified; no playability claim is made.
- On a later retry with Steam open and both configurations set to port 39753, the host status refreshed and reported `waiting on UDP 39753`; the guest reported `connecting to 127.0.0.1:39753`. They remained disconnected. The guest log reported `UDP receive error: An invalid argument was supplied.` No live choice or gameplay test ran during this retry.

