# Death Stranding research notes

## 2026-10-25 - initial public-source reconnaissance

### Observations

- Target selected: Steam **Director's Cut**, app [1850570](https://store.steampowered.com/app/1850570/DEATH_STRANDING_DIRECTORS_CUT/). No installed build was inspected or tested.
- Hub issue and PR searches for `"Death Stranding"` returned no matches, including closed entries.
- [Decima Workshop](https://github.com/ShadelessFox/decima-workshop) explicitly supports both original PC Death Stranding and Director's Cut; its README describes typed core-object editing and archive repacking. GPL-3.0. Asset tooling is distinct from a runtime gameplay API.
- [decima-native](https://github.com/ShadelessFox/decima-native) has a non-default [`dsdc-injector`](https://github.com/ShadelessFox/decima-native/tree/dsdc-injector) branch as well as `ds2-injector`, `hfw-injector`, `hrzr-injector` and `master`. Default-branch inspection alone misses the relevant target.
- [DeathStrandingModLoader README](https://github.com/NRTnarathip/DeathStrandingModLoader) describes Director's Cut emulator compatibility and lists custom asset loading as upcoming. Its current source tree additionally contains Lua, reflection, entity tracking and API export code. [`ObjectScanner.cpp`](https://github.com/NRTnarathip/DeathStrandingModLoader/blob/master/ModLoader/ObjectScanner.cpp) installs entity creation/free hooks at fixed RVAs and disables container hooking with `fixme: crash some case`. [`MoverComponent.cpp`](https://github.com/NRTnarathip/DeathStrandingModLoader/blob/master/ModLoader/MoverComponent.cpp) implements a velocity getter. These are source observations, not proof of working gameplay control.
- [Odradek RGB Sync research](https://github.com/advanced-bencoding/odradek-rgb-sync/blob/main/RESEARCH.md) documents named animation discovery by vtable/resource path on **Epic Director's Cut under Proton**. Its [README](https://github.com/advanced-bencoding/odradek-rgb-sync) marks native Windows untested and other builds as requiring rediscovery. It reads memory; it does not demonstrate actor spawning or replication.

### Derived conclusions

- No usable real-time co-op implementation has been identified in the searches completed so far. This is a bounded negative finding; it does not prove none exists.
- A native bridge with reflection and a small game adapter is a research candidate, consistent with existing MDK2/Witness approaches. Whether to extend the loader or use a smaller injector remains undecided.

### Contradiction and response

- The initial README-based assessment understated the loader's current source scope. Stopped architecture selection and reported the contradiction before proceeding with source inspection. The hub's wording `only ... a mod loader exists` also omits asset and RTTI tooling. Correct the directory entry and propose a research-playbook clarification to compare README, source and non-default branches before describing a project's limits.

### Guesses / proposals (unverified)

- Start with read-only player/camera/entity discovery and a visible remote pose; investigate native actor spawning separately. Cargo and delivery authority likely need a dedicated design beyond pose sync.

### Unknowns

- Exact installed Steam build ID, executable SHA-256 and runtime OS; current hook compatibility; actor spawn/control API; stable actor/item identifiers; save redirection; two-instance startup; story/cargo/BT/vehicle/streaming semantics; reusable real-time Decima netcode.

### Checks actually run

- Public-source reads, GitHub metadata/tree searches and hub issue/PR searches only. No binary downloads, builds, game launches or co-op tests.

## 2026-10-25 - source audit and search ledger

### Observations

- Loader source pinned to [`8e5396d222d72e9f231d439def3ddd81f66f6eac`](https://github.com/NRTnarathip/DeathStrandingModLoader/commit/8e5396d222d72e9f231d439def3ddd81f66f6eac), commit date 2025-09-24. [Latest release metadata](https://github.com/NRTnarathip/DeathStrandingModLoader/releases/tag/v0.0.4): `v0.0.4`, 2025-08-07. These differ; source capabilities must not be attributed to that release without inspecting it.
- [`LuaGameTypeBinding.cpp`](https://github.com/NRTnarathip/DeathStrandingModLoader/blob/8e5396d222d72e9f231d439def3ddd81f66f6eac/ModLoader/LuaGameTypeBinding.cpp) binds getters for position, velocity, speed, health and maximum health. [`LuaModManager.cpp`](https://github.com/NRTnarathip/DeathStrandingModLoader/blob/8e5396d222d72e9f231d439def3ddd81f66f6eac/ModLoader/LuaModManager.cpp) registers `UpdateTick` with the render callback. One API-named file (`LuaModFunctionAPI.cpp`) contains only includes; filenames alone overstate completeness.
- The loader has a root MIT license but its [vendored decima-native license](https://github.com/NRTnarathip/DeathStrandingModLoader/blob/8e5396d222d72e9f231d439def3ddd81f66f6eac/ModLoader/extern/decima-native/LICENSE) is GPL-3.0. No distribution/license resolution was attempted.
- decima-native relevant branch tip: [`3a9211abd48a46a348361f9fbbc3453f6773d601`](https://github.com/ShadelessFox/decima-native/commit/3a9211abd48a46a348361f9fbbc3453f6773d601), 2026-04-11. [`Dumper.cpp`](https://github.com/ShadelessFox/decima-native/blob/3a9211abd48a46a348361f9fbbc3453f6773d601/source/Dumper.cpp) contains signature discovery, RTTI/exported-symbol export, core listeners and menu/splash resource edits. These are examples in source, not tests performed here.
- Workshop release metadata: [`v0.1.27`](https://github.com/ShadelessFox/decima-workshop/releases/tag/v0.1.27), 2025-04-26. Default-branch tip [`59c6ea825021d0d150fe6ddd8f3181ddb8097c92`](https://github.com/ShadelessFox/decima-workshop/commit/59c6ea825021d0d150fe6ddd8f3181ddb8097c92), 2026-05-10. Decima Explorer `master` tip [`f22ef4cc0696872c412a10ee7c6e36fba5ad1bdc`](https://github.com/Jayveer/Decima-Explorer/commit/f22ef4cc0696872c412a10ee7c6e36fba5ad1bdc), 2020-09-10. Metadata obtained directly from GitHub API, not inferred from search-engine freshness.
- RGB Sync tip [`8a88b3f56cc3fbd0f5a5552076d04b6b5ec2724d`](https://github.com/advanced-bencoding/odradek-rgb-sync/commit/8a88b3f56cc3fbd0f5a5552076d04b6b5ec2724d), 2026-10-23. Its author reports named animation objects and a discovery workflow. Do not transplant Epic/Proton addresses into Steam.
- The loader's currently open [issue #4](https://github.com/NRTnarathip/DeathStrandingModLoader/issues/4) is titled `Antivirus trigger`, opened 2026-05-05. Title/metadata inspected only; no reproduction or diagnosis. No co-op bug tracker found because no co-op implementation was identified.

### Searched

All below on 2026-10-25; negative results are bounded by these methods.

| Surface | Queries / inspection | Result and limits |
|---|---|---|
| Hub | `gh issue list` and `gh pr list`, upstream, `"Death Stranding"`, `--state all`; connector issue/PR search | Both empty; no open Death Stranding PR to read |
| GitHub repositories | `"death stranding" coop`, `"death stranding" multiplayer`, `death-stranding coop` | No matches in connector repository search |
| General web | `Death Stranding coop mod multiplayer github`, `Death Stranding multiplayer mod cooperative mod nexusmods`, quoted coop/multiplayer variants | Tooling and requests/proposals; no verified co-op artifact |
| Japanese / Russian web | `"デスストランディング" "協力" "MOD"`, `"Death Stranding" "кооператив" мод` | No concrete implementation identified; noisy results, not an exhaustive community search |
| Nexus / ModDB / Steam | Site-restricted multiplayer/co-op queries and broad combined queries | No usable project found; some unrelated Garry's Mod workshop results. Full mod lists, files and comments not audited |
| Reddit | Public indexed results | [A Multiplayer mod proposal (PC)](https://www.reddit.com/r/DeathStranding/comments/q9ghda/) surfaced; proposal text is not evidence of a build. Full comments/history not audited |
| Archive.org | `"Death Stranding" co-op mod site:archive.org` | No concrete lead; no CDX/snapshot or dead build-server investigation completed |
| Loader branches/forks | GitHub tree, branches, forks API | `master` only upstream; one fork [Hengle/DeathStrandingModLoader](https://github.com/Hengle/DeathStrandingModLoader), `master` tip `668f967`, 2025-08-24. Fork code not audited |
| decima-native branches/forks | GitHub API, relevant branch source | Relevant `dsdc-injector` found and inspected. Two forks listed: Tevtongermany and Hengle, default `hfw-injector`; their other branches/content not audited |
| Sibling engine | `horizon coop` repository search; `"Horizon Zero Dawn" "multiplayer mod" github` | No reusable co-op framework identified. `2533757653/Horizon-Coop` was checked and is a cryptocurrency trading engine, an unrelated false positive |
| Code search | Attempted `gh search code` | Installed gh lacks `search`; no successful authenticated code search. Still pending |
| Private communities | None | Discord not accessed or exported; nobody contacted |

### Derived conclusions

- Current decision: **build-from-zero research candidate**, with possible reuse of runtime discovery tooling. There is no existing co-op mod selected for revival. Implementation architecture remains provisional until local identity and compatibility gates pass.
- The earlier `only a ... loader exists` directory wording should be corrected. A focused playbook addition now explains this game's README/source discrepancy and the distinction between source capabilities and tested releases.
- Existing MDK2/Witness projects provide a suitable engineering pattern: native game adapter, separate protocol core, lifecycle handling, background bots and failure mutations. Their engine-specific addresses, Lua assumptions and renderer code do not transfer automatically. Source READMEs are ahead of some hub summaries; this research does not change those games' entries.

### Guesses / proposed design

- Host-owned cargo/world/order progression, per-player movement input, explicit ownership transfer and reconnect snapshots are starting hypotheses. First prove a single drop/pickup/delivery transaction before expanding the world scope.
- Player positions may use a different representation from camera or physics positions; derive from observations instead of copying an unverified struct.
- Native porters may be useful spawn references, but no controlled porter/Sam clone has been demonstrated by this research.

### Unknowns / next research gates

- Steam executable identity; DLL entry point and loader-lock/thread behavior; signature compatibility; safe update-thread API calls; native puppet spawning, animation and collision; asset lifetime; stable GUID semantics; inventory transactions; order credit; independent save paths and concurrent game startup.
- Region streaming and origin handling; enemies and BT state; vehicles and structures; menu pause; cutscenes; private rooms; death/voidouts; late join; shared progression persistence; normal offline configuration and service separation.
- No effort estimate is justified by the current evidence.

### Documentation validation

- `python3 tools/build_directory.py`: generated directory, 303 entries.
- `python3 tools/build_directory.py --check`: PASS, 303 entries.
- `git diff --check`: PASS.
- Local Markdown targets in this write-up, notes and modified research playbook: PASS. New write-up/notes scanned for local usernamed paths: PASS.
- The initial `python tools/build_directory.py` command could not run because `python` is unavailable; reran successfully with existing `python3`. No software installed.
- No game QA ran at this point; QA was `none`.

## Findings from the first engineering pass (2026-10-28)

All of this is local engineering evidence for Steam build 13300582 (version 1.004, the latest patch, January 2024). Nothing in this section had been confirmed by running the game under a mod when it was written; the section after it has the live results.

### Verified facts

- The Steam Director's Cut executable is a 64-bit PE with no Denuvo, EAC or BattlEye files in the install and conventional section layout; protection absence is moderate confidence only.
- The install is about 80 GB (two depots: 144 `data/*.bin` archives plus one 17 GB movie archive). It calls `SteamAPI_Init` and `SteamAPI_RestartAppIfNecessary` (both delay-imported) and ships no `steam_appid.txt`. A copy launched without that file may relaunch the original install through Steam, which defeats any isolation. Normal Steamworks behaviour with the file present (`1850570`) avoids that; no Steam replacement is needed or acceptable.
- Saves live under the user's Local AppData in `KojimaProductions\DeathStrandingDC\<steam id>\`, with Steam Auto-Cloud on a second path. `LOCALAPPDATA` set in the environment does not redirect them, because the game resolves the folder through the known-folder API. Disable Steam Cloud for the app before any test run: an in-process hook cannot stop the Steam client from syncing.

### Techniques that worked

- Two test instances can share the read-only archives by NTFS hard links (about 0.4 GB of extra disk each) with a private copy of the executable. Hard links work from both WSL and Windows on the same volume. File modes are useless here (every file reports 0777 through WSL), so classify by an explicit allowlist and verify afterwards with size, mtime, file id and link-count checks. A write through a link modifies the original; the checks detect it after the fact, they do not prevent it.
- Save and config redirection by patching the import tables of every loaded module (known-folder, environment, and file-create/delete/move APIs), with deny-by-default for anything that would write outside the instance. A tiny probe program that calls these APIs proves it without launching the game. The loader is a proxy of a statically imported system-style DLL that the executable ships (the shader compiler DLL), forwarding all exports to a renamed private original.
- Hashing the whole install over the Windows filesystem is far too slow from WSL: pin and hash only the files you need, and use metadata for the rest.
- Windows timing: a 15.6 ms default timer granularity and relative sleeps made a localhost presence test drift (p95 position error 0.32 to 0.42 m). A monotonic high-resolution clock, paired timer-resolution requests, fixed deadlines and accurate pose timestamps brought it to about 1 cm with 2 to 4 ms round trips. Check timing on real sockets, not only in a simulator.

### Static evidence (hypotheses, not confirmed)

- Decima reflection names for the local player, entity, position, velocity, animation and cutscene types exist as plain strings, and several getters can be located with short masked signatures plus a RIP-relative resolve. Six candidates matched uniquely on the pinned build. Their object layouts are hypotheses until a live run confirms them.
- Candidate data path: a manager global leads to the local player, then an entity pointer, then position and transform blocks. A visual cutscene or loading gate was not found; the cutscene getter located so far is audio-only.

### Open

- Whether the executable starts under a proxy loader and what it writes at runtime.
- Whether two copies can both initialise Steamworks as the same Steam user (a second account or machine may be required).
- Player position units, axes, regions, animation mapping, vehicle and cargo state.

## 2026-11-01 to 2026-11-10: the mod, live on build 13300582

Source for this section: the mod's public repository at commit `bb7fcfc`
([STATUS.md](https://github.com/TommoDotCommo/death-stranding-coop/blob/bb7fcfc/STATUS.md),
[mod/game_offsets.md](https://github.com/TommoDotCommo/death-stranding-coop/blob/bb7fcfc/mod/game_offsets.md), the
research record with every address and the evidence for it). Target: Steam Director's Cut, app 1850570, build 13300582,
`ds.exe` SHA-256 `9d5306d7eb2f5d3135c63accc3e74b8f9b029f9d76112a6aae4a38fdbb4edd2b`. Everything ran in disposable copies
kept offline, on one PC.

### Observed live

- **The open questions of 2026-10-28 are answered.** The exe starts under the proxy loader. Two copies run at once under
  one Steam account once the game's single-instance mutex is made per-copy. Saves, settings and checkpoints are written
  only inside each copy; the real saves were byte-identical after every run.
- **Modded copies stay offline.** The proxy refuses the game's config and online-server connections and the game
  switches to its own offline mode.
- **Player pose.** Position is three float64, rotation a 3x3 float32 matrix, read through the game's own local-player
  path. Exchanged between two copies over loopback: median error 5 mm.
- **A first attempt to create a second Sam crashed.** Calling the Sam resource's creator from a game-thread callback
  crashed during component activation with a null `DSPlayerController`. The player's creation path assigns the
  controller before activation, and that assignment also changes player ownership, so it cannot make an independent
  puppet.
- **The game keeps a spare Sam.** `DSPlayerAKukanEntity`, the private-room body, is parked asleep and hidden at the
  world origin, with no AI, controller or player state. Borrowing it (the game's own SetSleeping, SetVisible,
  SetWorldTransform, its mover's target, the animation variables) gave the first partner body: it walked, ran, crouched
  and jumped within 1 cm of the real partner. It needs an instance of the porter's animation network, because its own
  has almost no clips, and it must go back to the game for private rooms, events and the cargo menu's preview.
- **The partner's own body.** The engine's common entity creator, given the double's resource, makes a second Sam that
  the mod owns. Dressing it by applying a second outfit showed two outfits overlapping; the game's in-place art-parts
  swap (the call the porter's streaming control makes on the double), after waiting until the porter is dressed, gives
  a clean body. Backpack and cuff are copied with the creator and the attach call.
- **Cargo.** A placed item is created in the partner's world with the game's `CreateBaggage` and removed with
  `DeleteBaggage` when either porter picks it up. A cargo record names its slot, not its carrier, and the game uses
  records beyond the first 511. An item put down from the menu keeps a carry-frame position for over a second; the cargo
  menu stages items next to the porter exactly like a pick-up; story cargo must not be shared.
- **Structures.** The construction manager's table has one record per structure. A partner's structure is built with
  `CreateConstruction` as the game's "Net" variant (the one it uses for other players' structures), which has its own id
  range and so is never sent back. Run live with ladders; posts, shelters, watchtowers, bridges and ziplines stayed
  pending in that run and stood after the next load.
- **Vehicles.** A seated porter is a child entity of the vehicle. The partner's vehicle comes from the game's pooled
  vehicle creator and is moved by its own mover's transform playback. A 140 m drive at up to 43 km/h was followed with a
  median gap of 1.35 m and no jumps. It is display-only: no collision, no wheel animation, no passengers.
- **Rain and timefall.** A guest places the game's own timed local weather influence where the host reports rain; the
  mod owns it, so the game does not save it.
- **Map pin, pings, name label, likes** use the game's own pin, hologram-name label and like popup. Likes are real
  (saved and counted), so they are off by default.
- **Input without the desktop.** The proxy feeds scripted keyboard and mouse to the game as raw input on the game
  thread, so tests need no window focus. The game never polls XInput: a connected controller holds pad slot 0 through
  Windows.Gaming.Input.
- **Cost.** The mod's game-thread tick cost about 105 ms with two copies (each memory read was two system calls, and
  stages re-read the same chains); caching region checks and reading each chain once per tick brought it to about 5 ms.
- **The proxy crashed the game once by itself:** two threads patching import tables at once. One lock fixed it; a
  crash recorder with module+offset stacks found it.
- **Release loading.** The proxy also builds as `avrt.dll`, a DLL the game imports but does not ship, so an install
  replaces no game file. Tests used a proxy of the shipped `d3dcompiler_47.dll`.

### Derived

- Before trying to construct a second player, look for one the game already keeps (a menu, private-room or preview
  body). Borrowing it needs no creation path and showed which calls move and animate a non-player Sam.
- Use the game's variant for "another player's object" where it has one: it comes with its own id range, which is an
  echo guard for free.
- Shared ids that live only in memory are lost on a restart. Events that remove something should also name the item and
  its place so the partner can find its copy without the id; a two-world fuzzer with restarts found this.

### Unknown

- A session between two PCs: built (any unicast IPv4 peer on a LAN or VPN), run over loopback only.
- BTs, MULEs, death and voidouts, orders and deliveries, cutscenes, saves of a shared world: not started.
- Structures built or dismantled through the game's own menus, and upgrades: not run.
- A real private-room entry while a partner is connected: not run.
- Builds other than 13300582 (the mod refuses them), the Epic release, the original 2020 PC release.

## 2026-11-13: a second PC, a native Windows build, one copy and the stand-in partner

Source for this section: one session on a PC that is not the author's. Mod commit
[`c05321f`](https://github.com/TommoDotCommo/death-stranding-coop/tree/c05321f), hub `488a8f3`. Same target: Steam
Director's Cut, app 1850570, build 13300582, the same `ds.exe` SHA-256, Windows 11. The mod was built on that PC with
MSYS2 (mingw-w64, GCC 16), not with the Linux cross-compiler its READMEs describe. One disposable copy (a full copy of
the install plus `steam_appid.txt`) and the mod's stand-in partner (`coop-partner`) ran on the same PC, with Steam
Cloud off for the game as the mod's README asks. A person played; nothing was driven by a script. No second PC and no second
game copy took part.

### Observed

Build and game-free checks:

- `make all` needed three things from outside the source tree: `MINGW=x86_64-w64-mingw32-g++` (two Makefiles default
  to the Debian name with a `-posix` suffix), a `x86_64-w64-mingw32-windres` wrapper around MSYS2's plain `windres`,
  and `-static` on every link. Without `-static`, `avrt.dll`, the probes and the launcher import
  `libwinpthread-1.dll` and fail to start with `0xC000007B`.
- `make test` reported 396 mutants killed and none surviving, then stopped with exit 2: the `core` host targets need
  `sys/syscall.h`. The redirect, install, launcher and world suites did not run in that pass.
- Run separately and passing: `make -C redirect portable-test` (needs ripgrep on `PATH`), `net-fixture.exe`,
  `discover-win-test.exe`, and `test_passthrough_windows.py` (the release proxy passes, the missing-config mutant
  exits 125).

Starting the copy:

- The launcher's documented one-copy command (`--dev-direct … --dev-peer 127.0.0.1`) wrote the expected `mod.json`
  with `--prepare-only`. Without it `ds.exe` exited at once with `redirect: invalid network peer list`.
- A direct start with the lab environment and a data folder separate from the game folder: exit 125,
  `MODULE_REFUSE <test copy>\steam_api64.dll`.
- Data folder inside the copy, module policy `allowlist`: the game ran a few seconds, then exit 125 with
  `MODULE_REFUSE` for the DLL of a third-party input method installed under `C:\Program Files\`.
- Data folder inside the copy, module policy `log-only`, network policy still `enforce`: the game stayed up. Proxy
  log: `READY log-only`, `NET_DENY WinHttpConnect` for the game's configuration host, `MOD_STARTED guarded game-thread
  dispatch`. All later runs used this setting.

Isolation, checked after the first run that stayed up:

- The game had never been started on this PC, so the real save folder (Local AppData `KojimaProductions`, and the
  Roaming, Documents and Saved Games equivalents) did not exist before the run. It still did not exist afterwards.
- The copy's `profile.dat`, shader cache and, later, its autosaves were inside the copy's `redirect-data`.
- Names, sizes and modification times of the real install's 158 files were unchanged.
- This check was not repeated after the long session below.

In the game, new save, prologue area:

- Mod log after starting a new game: `POSE available`, `HANDSHAKE … presence allowed`; the stand-in logged the
  matching `HANDSHAKE` and one `CARGO received`.
- **The proxy crashed the game about ten minutes in.** Windows event log: access violation in the proxy DLL. The
  proxy's crash record: a read of an unmapped address in its import patcher (the DOS header of a module), on the main
  thread, under a `LoadLibraryA` call from `dxgi.dll`, itself called through Steam's overlay renderer. That base was
  not in the module list written at the crash.
- With a local fix (below), the next run logged `MODULE_GONE …\windows.system.launcher.dll unloaded between snapshot
  and patch` within 40 seconds and kept running. That session lasted about 52 minutes and ended by a normal exit: at least 11 `MODULE_GONE` records, an empty crash log, no Windows error event.
- **With `partner: "own"` the partner had no body.** The stand-in was connected (`STATUS connected=1 distance=3.3
  rtt=15`); its name label and two gear pieces were drawn and followed; no Sam was drawn (looked at in the second
  session). Log, in both sessions: `BODY own body … made and parked`, `BODY art-parts swap refused: body already
  wears the double's art parts`, `BODY model components could not be built`.
- The live command `peer` (the resident double) fixed that in the running game: the double was acquired and placed,
  the made body released, and the player then saw a full Sam that followed (stand-in status: connected, 4 to 7 m
  away, 15 ms round trip).
- A Bluetooth Xbox controller was connected. The proxy logged two `DENY` records for opens of its XInput (XUSB) device
  interface, `\\?\hid#{00001812-…}&dev&vid_045e&pid_0b22…&ig_00#…#{ec87f1e3-c13b-4100-b5f7-8b84d54260cb}`, and
  `xinput1_4.dll` was loaded four times in that run.
- A scan of `ds.exe`'s imports and strings: `XInputGetState` and `XInputSetState` resolved by name from
  `xinput1_4.dll`; SetupDi imports and `HidD_*` names (direct HID); `RoGetActivationFactory` by name
  (Windows.Gaming.Input); Steam Input strings; DualSense and DualShock strings. Keyboard and mouse only through raw
  input.

### Derived (read from the mod's source at `c05321f`, not run)

- The launcher always puts the peer into `COOP_NETWORK_PEERS`
  ([launch_win.h:61](https://github.com/TommoDotCommo/death-stranding-coop/blob/c05321f/launcher/launch_win.h#L61)),
  and the proxy's peer parser refuses any 127.x address
  ([network_policy.cpp:92](https://github.com/TommoDotCommo/death-stranding-coop/blob/c05321f/redirect/network_policy.cpp#L92)).
  Loopback is reachable without a list entry, so starting the copy without that variable works.
- On the lab path the module allowlist admits the instance folder but not the executable's folder; that is added
  only for a release ticket
  ([module_policy.cpp](https://github.com/TommoDotCommo/death-stranding-coop/blob/c05321f/redirect/module_policy.cpp#L14-L23),
  [redirect.cpp:1021](https://github.com/TommoDotCommo/death-stranding-coop/blob/c05321f/redirect/redirect.cpp#L1021)).
  So a lab copy's data folder has to be the copy itself.
- The allowlist is fixed in code and a refused module ends the process. Any software that loads a DLL into every
  windowed process (here an input method) therefore ends an `allowlist` copy, and the launcher always asks for
  `allowlist`.
- The crash: the proxy re-patches import tables after every late module load by walking a toolhelp module snapshot
  ([redirect.cpp:483](https://github.com/TommoDotCommo/death-stranding-coop/blob/c05321f/redirect/redirect.cpp#L483)).
  The snapshot holds addresses, not references, so a module another thread unloads in between is read after it is
  unmapped. The local fix takes a loader reference (`GetModuleHandleExW` by address) for the patch and skips a module
  that is gone.
- The missing body: the mod waits up to 600 ticks for the porter to be "dressed" (more than one art part), then makes
  the body anyway; dressing it is refused because it already wears what the double wears; the undressed body is then
  kept for the rest of the world
  ([double.cpp:283-313](https://github.com/TommoDotCommo/death-stranding-coop/blob/c05321f/mod/double.cpp#L283-L313)).
- The XUSB opens were denied because the proxy's path rules admitted one device interface only, the HID one
  (`{4d1e55b2-…}`). A second local fix admits the XUSB interface behind `hid#` and `usb#`, with three mutants.

### Contradiction with the write-up, and response

- The write-up said the game "never polls XInput". On this PC, with an Xbox controller connected, `xinput1_4.dll` was
  loaded and opened the controller's device interface, and the exe resolves `XInputGetState` by name. The mod's own
  note is narrower: its wrapper for user 0 was never asked while Windows.Gaming.Input held the controller's slot. The
  write-up now says only that.

### Guesses

- In the prologue the porter himself may still wear a one-part outfit, so he never counts as dressed and there is
  nothing to copy. Not checked on a later save.
- Other software that injects a DLL (overlays, screen readers, other antivirus products) would end an `allowlist` copy
  the same way.

### Unknown

- Whether the controller works in the copy, with or without the XUSB fix: not looked at.
- `partner: "own"` on a later save on this PC.
- Whether a binary from MSYS2's GCC behaves like the author's Debian build in the game. Nothing was compared side by
  side.
- Sessions longer than about an hour, a second game copy, two PCs.

### Checks actually run

- Game-free, on this PC: listed under "Build and game-free checks" above. The whole-repository `make test` did not
  finish.
- Live: one copy and the stand-in partner, two sessions (about 10 minutes, ended by the proxy crash; about 52 minutes
  with the fix, normal exit). A person played. No assertion was automated; the evidence is the proxy, mod, crash and
  Windows event logs and what was on screen.
- The two local fixes, proposed to the mod as
  [#7](https://github.com/TommoDotCommo/death-stranding-coop/pull/7) and
  [#8](https://github.com/TommoDotCommo/death-stranding-coop/pull/8) (not merged): the module reference in the patcher, with a native race test whose
  mutant must fault (8 of 8 runs each way on this PC), and the XUSB device interface, with three mutants that the
  path test kills. `make -C redirect portable-test` passes with each.
