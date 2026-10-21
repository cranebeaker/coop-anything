# Spider-Man: Friend or Foe research notes

## 2026-10-20: preliminary research, awaiting game files

### Observations from sources

- Activision's 2007-10-03 launch announcement describes drop-in co-op and lists Windows PC among the platforms. It does not give Windows-specific joining controls: https://investor.activision.com/static-files/d8459280-fceb-49b0-9210-86bb7010c3d3
- A firsthand Windows review dated 2007-10-13 describes two players on the same computer, playing missions together: https://www.swiftworld.net/2007/10/13/pc-spider-man-friend-or-foe-review/
- GameSpot's 2007-10-04 review reports playing PC, Xbox 360 and Wii, describes two-player support without online play, and reports PC support for an Xbox 360 controller: https://www.gamespot.com/reviews/spider-man-friend-or-foe-review/1900-6180451/
- An interview with Activision representative Bafus describes drop-in/drop-out co-op. Its controller instructions do not establish the Windows build's exact joining controls: https://www.superherohype.com/features/94943-spider-man-friend-or-foe-exclusive
- Before these edits, the directory and games tree contained entries for different Spider-Man games, but no Friend or Foe entry. The checkout was freshly cloned and pulled.

### Derived conclusions

- Existing two-character local co-op is a starting point for examining the playable sidekick and input assignment.
- Native local co-op does not establish deterministic simulation, usable network code, or a working mod for independently running Windows clients.
- Remote input/video sharing is outside the selected target.

### Hypotheses, not tested

- Reusing the existing local player-two input path may reduce work compared with creating a new playable entity.
- Input synchronization is a candidate only if determinism is demonstrated. Otherwise authoritative state replication needs investigation. No networking design is selected.

### Unknowns

- Executable version, SHA-256, architecture, engine, archive formats, scripting support, and licenses of any reusable code.
- Original co-op functionality in the supplied build and player-two joining controls on current Windows.
- Simulation determinism, random seeds, AI/combat ownership, entity identities, story/world state, level transitions and save behavior.
- A native Windows online project may exist outside the bounded searches below.

### Existing projects and exclusions

- No native Windows online co-op project was identified in these searches. This is not an exhaustive absence claim.
- PCGamingWiki hosts a display compatibility fix credited to Fix Enhancers/Chip, based on work by Elisha Riedlinger and ThirteenAG. Its description concerns resolution, aspect ratio, FOV and FPS; it does not claim multiplayer. No download, hash or license verification was performed: https://community.pcgamingwiki.com/files/file/3759-spider-man-friend-or-foe-fix/
- The author's 2025-12-23 guide identifies PCGamingWiki as the current official source rather than old GitHub mirrors: https://www.youtube.com/watch?v=s9T13Vr-oi8
- A reported online demonstration uses Parsec and PCSX2: PS2 emulation with streaming, not native Windows netcode: https://www.youtube.com/watch?v=9BoGtSh8z9o
- Proton issue 7389 concerns Linux/Steam Deck startup compatibility, not Windows multiplayer: https://github.com/ValveSoftware/Proton/issues/7389
- Remastered/Spider-Man 2 multiplayer, skins in other games, emulator fixes and widescreen patches were excluded as different projects.

### Searched

Public web searches on 2026-10-20, including delegated research:

```text
site:github.com/cranebeaker/coop-anything "Spider"
Spider-Man Friend or Foe PC online co-op mod github multiplayer
"Spider-Man Friend or Foe" "online" "co-op" mod
"Spider-Man" "Friend or Foe" "multiplayer mod"
site:github.com "Spider-Man: Friend or Foe"
site:github.com "Spiderman Friend or Foe"
site:github.com "Friend-or-Foe" "Spider"
site:github.com "Friend or Foe" "controller"
site:moddb.com "Friend or Foe" multiplayer
site:moddb.com/games "spider-man-friend-or-foe"
site:nexusmods.com "friend or foe" "co-op"
site:archive.org "spider-man friend or foe" "mod"
"человек паук друг или враг" "по сети" мод
"человек паук друг или враг" "кооператив" "мод"
"Spider-Man Amigo o Enemigo" "online" "mod"
"Spider-Man Freund oder Feind" "online" "Mod"
"Spider-Man" "Friend or Foe" "controller fix"
"Spider-Man" "Friend or Foe" "xinput"
"Spider-Man" "Friend or Foe" "x360ce"
"Spider-Man Friend or Foe" "PC" "second player"
"Friend or Foe" "netplay" "PC"
"Spider-Man Friend or Foe" "network" "mod"
```

- Direct GitHub repository/code searches and filtered hub issue/PR/API queries failed. Indexed GitHub results were used instead. No candidate native online repository surfaced for fork/branch/release inspection.
- The public hub issue first page and open PR listing were read; no target item was visible. Issue page two failed to fetch. Coordination is not considered cleared by these incomplete listings.
- No private community or authenticated account was accessed. No claim issue or PR was published.

### Checks actually run

- Local title search across `DIRECTORY.md`, `data/`, and `games/` before adding this write-up.
- Public source retrieval and the searches above.
- Directory regenerated; `python tools/build_directory.py --check` reported `OK: 262 entries`. `git diff --check` passed. These are documentation checks, not game tests.
- No game executable was supplied or run. No runtime, network, controller, automated gameplay or campaign test ran.

### Next investigation

1. Obtain an installed/extracted game folder, preserve its layout/localization, and identify the executable before applying patches or write-ups.
2. Keep supplied files and private details under ignored `local/`. Use an isolated test copy with separate saves/configuration. Understand write locations before launching.
3. Inspect source/script/archive access first, then native hooks if needed. Match addresses/signatures to the exact build.
4. Verify local co-op and investigate input routing/determinism before choosing a synchronization model.
5. Recheck claims/PRs before implementation. Prepare a claim and contribution diff for owner approval before account use or publication.

## 2026-10-20: uploaded-build static investigation

### Observations

- Two language variants identify their main executable as Windows x86 build 1.0.0.111. Their main executable bytes are identical. Exact hashes, complete inventories and supplied-copy details remain in ignored `local/`.
- Case-normalized manifests contain 1305 matching paths per variant. 1270 files are identical; 35 differ: 23 dialogue wavebanks, 7 Bink videos, 2 localization tables, 2 fonts, and uninstall metadata. Gameplay data/dictionaries, scripts, hash lookup and INI files match byte for byte.
- Text localization tables retain the same 815 numeric IDs and record order. Seven corresponding localized Bink videos report identical container duration and video frame rate. These are static metadata checks, not playback tests.
- Dialogue wavebanks parse as WBND version 42 with 24-byte entries. Some counterpart audio sample rates/payload sizes differ. Header-derived payload-capacity estimates are not measured playback durations; the metadata alone cannot rule out audio-driven event timing differences.
- A read-only archive reader validated 518 dictionary/data pairs and found 76 structurally valid embedded scripts in addition to five standalone bytecode files. A shipped script named `automatedtests.byte_code` contains area/sidekick/front-end/test functions and test setting names. This is not proof those tests execute in a retail build.
- Native imports and disassembly show save/configuration initialization before window/graphics initialization. Copying the install alone is insufficient to isolate saves and registry settings. A pre-entry isolation probe is being prepared before any game launch.
- Some network-related tuning names remain in the executable. No Winsock imports were found. These names alone do not establish active multiplayer code.
- An unauthenticated public API check read 47 open hub issues/PRs. No Friend or Foe item matched; the Spider-Man match was Remastered, a different title. Source: https://api.github.com/repos/cranebeaker/coop-anything/issues?state=open&per_page=100

### Inferences and unknowns

- One native hook implementation may support both language variants because their executable is identical. Mixed-language gameplay synchronization remains unverified.
- Retained script automation may be useful for reproducible game QA, pending runtime checks.
- Engine identity, safe runtime entry, player-state access, determinism and synchronization design remain open.

### Checks actually run

- Full SHA-256 inventories and case-normalized comparison of both supplied folders.
- PE/import/disassembly inspection using existing tools; localization/video/wavebank metadata analysis; archive and script structural validation.
- Separate full test copies created without editing supplied files.
- No game execution, network gameplay or automated game QA has run at this checkpoint.

## 2026-10-20: isolated startup checkpoint

### Observations

- The owner authorized a public claim, which was posted and verified as [issue #71](https://github.com/cranebeaker/coop-anything/issues/71). No code or pull request has been published.
- A pre-entry probe redirects the executable's known save/configuration folder calls, virtualizes its registry calls, restricts imported file writes to test roots, and keeps each owned process in a bounded kill-on-close job. It is not a complete operating-system sandbox. Its own fixture passed the redirected-file, virtual-registry and denied-write checks. Known owner save/configuration path metadata remained unchanged after the startup runs.
- English startup rendered legal and movie screens. Captures came directly from the game's D3D backbuffer. A virtual in-process controller bot reached the frontend flow state in a 60-second run; termination at the configured timeout is not a gameplay pass.
- Retail startup did not use the plain INI automation flag. Exact-build guarded, in-memory call overrides successfully selected shipped `InGame`, `GoThroughFrontEnd` and `ExecutionPath` functions.
- The `InGame` shortcut reached an uninitialized allocator before normal frontend setup and crashed. The probe does not initialize this subsystem manually; subsequent tests use `ExecutionPath`, which preserves normal initialization order.
- Script analysis now identifies 170 embedded scripts and five standalone scripts. The earlier count of 76 was an intermediate extraction checkpoint.
- Normalized input exposes 28 action values per logical slot; physical keyboard/controller mapping precedes those values. Individual physical devices should therefore remain local choices. No networking hook for those actions has been tested.

### Unknowns

- Gameplay initialization, local second-player joining, actor-state access, deterministic simulation, mixed-language runtime behavior and networking remain unverified at this checkpoint. QA level remains `none`.

### Evidence

- Exact-build probe source, fixture outputs, bounded runtime logs, private game-buffer captures, owner-path snapshots and disassembly are retained in ignored `local/` rather than publishing game assets or machine paths.

## 2026-10-20: interactive bot and paired-client prototype

### Observations

- The shipped automation's default mode forces player-capable characters to AI. Its interactive test writes a separate flag. Enabling that flag before the normal level setup restored world rendering and separate input controllers in the harness; the earlier black-world/join failure was a test configuration problem.
- Bounded English and Russian runs recorded two characters on distinct logical input slots, movement, and attacks. Direct game-buffer captures verified the world and localized HUD. Existing owner save/configuration path metadata remained unchanged in the recorded checks.
- An own-code TCP input channel passed a 180-frame loopback fixture, three-frame input delay checks, invalid-value rejection and incompatible build/scenario rejection.
- Two paired runs used independent English and Russian Windows processes, exchanged semantic actions over loopback, and completed 600 frames each. Primary and sidekick position bits matched on all 600 frames in each run. Input hashes matched on every frame. Random-state observations differed on five frames in each run; the sampled sequence later rejoined.
- Guarded random-call tracing identified the same random draw occurring one frame apart outside the main world update. A broader frame-clock experiment is in progress; it is not yet a verified fix.

### Limits

- The prototype is private own code, uses bots, and has no human host/join package or WAN test. No public mod download is being claimed.
- The comparison hash covered two tracked character records, including positions, headings and input slots. It did not establish full enemy, health, mission, inventory or dialogue synchronization. Matching player poses is insufficient for complete co-op QA.
- All run details and assets remain private. No new public account action was taken after the approved claim.

## 2026-10-20: longer combat and native-input gates

### Observations

- The early ambient random-state offset disappeared when all gameplay scheduler tasks used the same simulation step. Longer combat runs exposed a second divergence: an NPC moved one step earlier. Two English copies reproduced it, contradicting a localization-only explanation.
- The native navigation queue limits work by measured CPU time. Scoping its existing complete-work flag to the simulation update removed that observed drift. A 3,600-frame mixed-language combat run then matched input, common random state and sampled character/health state throughout.
- An in-process controller fixture now feeds the game's regular physical polling and binding layer. The relay captures the resulting 28 action floats and replays them on canonical player slots. A 7,200-frame English/Russian run matched all sampled states, observed party damage and hostile defeats, and both games closed normally with exit code 0.
- A code review found replay omitted the native per-direction analog deadzone. After correction, a native-axis oracle matched input values below and above the threshold. Another check now neutralizes a disconnected physical provider rather than capturing retained mapped values.
- A subsequent 3,600-frame mixed-language gate passed with those native-axis checks, all 31 simulated controller-disconnect samples neutralized, injected delays, live sampled-state comparisons and both normal exits. A strict scorer also passed its positive fixture and rejected 12 deliberately invalid result fixtures.
- A content preflight verifies 1,257 shared runtime files and a coherent set of 34 exact English or Russian localization files. Its 14 own-code fixtures passed. It rejected an intentionally modified test INI; restoring backed-up original test configuration allowed the three isolated game copies to pass. Supplied files were untouched.
- Known owner save/configuration path metadata remained unchanged after the recorded input tests.

### Limits and next checks

- Captured state covers character records and common RNG, not every shared gameplay subsystem. CPU-time and ambient fixes are tested in the recorded scenario, not certified across the campaign.
- Controller polling fixtures do not establish complete keyboard, focus, accessibility, menu or real-controller usability. A physical-provider unplug is distinct from a network disconnect; transport reconnect remains unfinished.
- All paired game tests used independent processes on one PC and loopback. No real remote-PC or WAN result is claimed. No playable package or new public account action was produced.
- Next: mutation-test the live divergence detector, exercise ordinary transitions, expand shared-state checks and prepare a usable host/join package.

## 2026-10-20: keyboard, area transition and Steam lobby checks

### Observations

- A one-bit mutation of the sampled state at tick240 was detected by both peers at exactly that observed tick; both games closed normally. The mutation gate passed.
- A native keyboard fixture drives the game's own DirectInput state/binding path while the other client uses its native controller path. All3,600 sampled input/character/health/RNG frames matched with damage and enemy defeats; both normal game exits were0. The fixture sends no operating-system input.
- Two earlier transition gates failed because no area swap occurred, despite matching sampled state and normal exits. The native exit-enter routine rejects a busy transition manager. Waiting for that existing manager to become idle allowed the fixture to prove both party characters were in the real enabled exit volume and invoke its normal enter callback. The subsequent gate observed the ordinary preload/exit sequence and an area swap while matching all3,600 samples. Both game/launcher/relay exits were0.
- The stream relay's loopback contract service models lobby membership, metadata, session acceptance and ordered messaging. Its corrected shutdown path tolerates a game TCP reset while counting any discarded late data. A paired game gate passed with461,008 bytes sent and received by each relay, zero discarded bytes and normal exits. This is not actual Steam transport.
- The owner explicitly authorized a bounded Steam account check. The first attempt failed in async callback retrieval. Restricting result retrieval to the helper's own lobby operation fixed the observed failure; the second attempt created one friends-only Spacewar lobby, left and shut down with exit0. No invitations or game traffic were sent.
- Eight own-code relay fixtures passed, including ignoring unrelated asynchronous completions. A separate compiled x64 flat-API mock fixture passed host/join callback decoding, identity/message handling, received-message release, lobby leave and shutdown without loading Steam or using an account. It validates the chosen adapter ABI against the fixture, not against a second live Steam client.
- Known owner save/configuration path metadata remained unchanged after these recorded checks.

### Limits

- One area swap is not campaign coverage. Mission, inventory, dialogue, full-world state, actual hardware usability and transport reconnect remain incomplete or unverified.
- The live account permission covered one bounded create/leave smoke check, including fixing and retrying its adapter error; it did not authorize invitations or gameplay sessions. Two-account delivery and WAN behavior have not been tested. No playable package or new publication is claimed.
- Sources: [Steam networking](https://partner.steamgames.com/doc/features/multiplayer/networking), [ISteamNetworkingMessages](https://partner.steamgames.com/doc/api/ISteamNetworkingMessages), [Spacewar example](https://partner.steamgames.com/doc/sdk/api/example). Runtime evidence and account identifiers remain private in ignored `local/`.

## 2026-10-20: successive areas and portable helper prototypes

### Observations

- The first attempt at three area swaps failed: the fixture re-entered the same registered exit volume after reaching the next area, and both clients crashed. Retaining an object in the global trigger pool does not establish that it belongs to the current area. This was an invalid fixture selection, not evidence that normal forward traversal crashes.
- Refusing to reuse the fixture's consumed exit volumes allowed a subsequent 7,200-frame gate to cross three successive native area boundaries into Tokyo's second, third and fourth areas. All sampled character/input/health/RNG states matched and both game/launcher/relay exits were0. Each relay sent and received921,808 bytes, with zero discarded bytes.
- Exact-build inspection found a 508-byte serialized progress state used by native load/copy and purchase/unlock routines. It also contains padding, local settings and save date/time, so hashing the entire structure would be invalid. The prototype samples selected initialized spans plus the current area name and mixes them into the live divergence checksum. Its own fixture detected13 mutations to sampled fields and ignored11 explicitly omitted bytes; scorer fixtures passed3 positive and18 invalid cases.
- With that broader checksum, a further 7,200-frame English/Russian keyboard/controller gate passed all three area swaps, injected delays, matching sampled progress and ordinary exits. This remains selected-field coverage, not a complete mission, inventory, dialogue or world-state oracle.
- An own C++ x64 relay built with a static runtime and passed a pinned compiled API mock, bidirectional32,768-byte TCP/close tests, reset accounting, malformed/order/membership/buffer/timeout rejection and stdin cancellation. Its only dynamic dependencies are standard Windows libraries. The previously approved bounded create/leave Steam lobby check also passed through this executable with no invites or game traffic. Actual two-account delivery remains untested.
- A private C#5 Windows launcher built using the existing .NET Framework compiler. Both actual language trees passed its1,291-file verification. Synthetic helper tests passed the same wait-for-peer, owned-child launch and normal-close/cancellation methods used by its Host flow. It has not yet established actual game usability, campaign saving or remote play.
- Private own code is checkpointed in a separate local source repository with no remote. Assets, logs, saves, account records and binaries are excluded. Nothing new has been published.


## 2026-10-21: real-input startup and lifecycle

### Observations

- A finite test of native unlimited-frame play mode initially remained at the frontend and produced no network states. Its two isolated games and launchers still closed normally; it was a failed gameplay gate. Earlier input fixtures had supplied startup A/Start presses.
- Adding startup-only actions through the native forced-action field while the flow is below8 allowed the normal frontend sequence to complete. The English/Russian test then matched306 consecutive idle input/RNG/pose/character/selected-progress/area samples, with the gameplay input fixture disabled. Both retained game handles and both native launchers exited0 after owned-window close requests; no forced cleanup occurred. This is a bounded one-PC idle lifecycle check, not physical-input or remote gameplay QA.
- An intro-skip variant failed the selected-progress checksum at tick0, although input, RNG, positions and area matched. Subsequent investigation reproduced the two hashes exactly by changing only an uninitialized alignment byte in the sampled structure. Intro skipping remains removed; the normal movie completion path remains.
- An explicit allowlist packaging tool passed17 synthetic checks and a read-only audit of the intended runtime inputs. It excludes game assets, Steam DLLs, saves, logs and account information. A portable package is still pending final quiet-mode verification.

### Derived findings

- Follow-up exact-build inspection identified byte0xBF as padding after the0xA5-byte upgrade array starting at0x1A and before currency at0xC0. Native initialization leaves it untouched. Changing only that byte in the prior real tick0 dump exactly reproduces both024 hashes; isolated saved-state copies also differ there. This was an invalid oracle byte, not evidence of different shared progress caused by localization or intro skipping. The oracle now omits0xBF and includes initialized0x128..0x12D fields. Scenario version5 rejects the older checksum contract. Native fixtures passed14 shared-field mutations and12 omitted-byte checks. Intro skipping remains removed while further startup behavior is assessed.

### Unknowns

- Two-account Steam delivery, real-device usability and full campaign/save progression remain unverified.

### Further verification

- Quiet mode with the original checksum happened to pass361 ticks with four logged samples and no image captures; this did not validate its padding classification. After correction, another run matched361 ticks but failed on shutdown when one game exited before the peer processed its own close message. That failure was retained.
- The native wire protocol now distinguishes ordered user STOP from finite-test FIN. A guarded hook of the owned game window sends STOP before forwarding close; validated peer STOP requests ordinary peer closure. Unexpected EOF remains an error. Twelve own-code loopback cases passed, including immediate-close and simultaneous-stop stress runs.
- A new7,200-frame English/Russian gate with the corrected progress checksum and new protocol passed three area changes, keyboard/controller bindings and injected delays. Both games, launchers and mock relay helpers exited0; each relay sent and received921,808 bytes with zero discarded.
- The private Windows launcher now offers an unchecked-by-default automated connection-test option. It runs3,600 scripted frames after peers connect, requires native FIN, contiguous state records, both players moving, actual retained process exits0, a normal stream and owned cleanup. Own-code evidence checks passed one positive,28 negative and three configuration cases; the complete UI branch passed synthetic helper runs. The GUI build uses the Windows subsystem, avoiding an extra console window. No real Steam session ran through this UI.

- The final quiet idle lifecycle check reached361 ticks with four120-tick observations, then closed only the host. The peer received the explicit normal-stop marker and exited normally; both game handles and launchers exited0, with no forced cleanup, shutdown error or diagnostic image captures. A private nine-file own-runtime package was built and archive contents/hashes verified. This is still a scripted Tokyo research package, with the matching Steam SDK library external; it does not establish campaign or remote gameplay readiness.

## Source publication and pause/state checks (2026-10-21)

### Observed

- Original code is now in [Friend or Foe Online](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online), with build/test instructions, a roadmap and explicit research-preview status. No game assets, runtime libraries, saves, private evidence or compiled release is published. Development continues; this is not an adoption handoff.
- Current scenario10/wire3 completed3,600 consecutive paired English/Russian frames with native keyboard/controller fixtures, combat, injected delays and mock transport. Native pause began243, the actor sample stayed frozen178 ticks, native resume completed421 and the fixture completed430. Both games, launchers and relays exited0. Each relay sent and received461,008 bytes and discarded0. This is a bounded same-machine result, with no Steam game traffic.
- Failed precursor029 replaced actions after native input processing and invalidated its own axis comparison. The physical-provider fixture now supplies those actions before native binding/filtering; the axis oracle remains enabled.
- Failed030 compared inactive previous-mode history during current mode4. Reconstructing the selected hash exactly reproduced both failures from previous values5 and1 with identical actors/progress/area. Native4E25FA..263B waits for the area script, then directly sets current mode0 without consuming that history. The canonical sample retains restoration history only in active pause/NIS modes1/5. Range checks and two mutation twins passed.
- Run031 completed the requested pause/resume but later failed its overbroad fixture assertion at a legitimate tutorial/system pause. The fixture assertion now ends at430; continuous selected mode/world/progress checks continue on every tick. Run032 then passed the complete3,600-tick gate.
- The profile-storage foundation passed39 synthetic checks. It does not supply native save success evidence, runtime launcher integration or campaign resume. QA lease fixtures passed13 checks, including three mutation twins; game gates hold an exclusive lease through owned cleanup.

### Subsequent startup failure

Run033 (scenario10, planned7,200 ticks and three area changes) failed on both
clients with native access violations before network readiness or gameplay.
No multi-area coverage is credited to this run. Static analysis found that the
native keyboard-name search discards its count and loops without a bound.
Internal names also depend on the active keyboard layout, while display names
are stored separately. The actual missing name/count/layout was not recorded,
so layout causation remains an inference. Guarded lookup diagnostics and canonical
internal naming are under investigation; this is an unresolved startup blocker.

### Unknown / unfinished

Retail campaign frontend/hub/mission selection, native shared save/load, full mission/boss/NIS state, real device usability and two-account Steam game delivery remain unfinished or unverified. The project estimates40/100 toward its first playable release using an explicit weighted judgement, not a time forecast. Whole-project co-op QA remains none. See the public [status](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/STATUS.md) and [roadmap](https://github.com/IlkinQafarov/spider-man-friend-or-foe-online/blob/main/docs/ROADMAP.md).
