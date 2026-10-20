# Eggs for Bart — co-op research notes

Game identity is in `local/identity.md` (kept out of the repo). Read it before trusting any
claim on this page.

Status: **Phase 0 (triage) and Phase 1 (research) complete. Phase 3 (understand) mostly
complete via decompilation. Phase 5 Stage-1 pipeline proof done (see §1.6). Phases 4, 6-8
not started.**

---

## 1. Observations (things that came from a source or from reading the files)

### 1.1 The game

- Eggs for Bart, by Fleebs, free, itch.io, v3.0, Windows/macOS/Linux, 2018-2019, third-person
  comedy horror. Chapter 1 you are Homer collecting eggs; later chapters swap the protagonist
  (Ned Flanders, then Chief Wiggum).
  Source: https://fleebs.itch.io/eggs-for-bart (page fetched 2026-10-19).
- The itch page claims the engine is "Unity 5 Personal". **That is wrong for the v3.0 build:
  the installed exe reports Unity 2019.1.4 and the data folder is a Mono build.** The page's
  "Tools used" box is the original 2018 metadata.
  Source: local observation of `<game dir>/EggsForBart.exe` version info.
- No co-op mod, no multiplayer mod, no netplay patch exists. Nothing in this hub's
  `DIRECTORY.md`, no `games/eggs-for-bart/` folder, zero issues/PRs in the hub mentioning it,
  and no community project found via web search.
  Sources: hub grep (2026-10-19), https://github.com/search (issues, 0 results),
  web search for `"Eggs for Bart" multiplayer mod unity` (2026-10-19).

### 1.2 The engine and what it gives us

- **Unity 2019.1.4f1, Mono scripting backend.** `EggsForBart_Data/Managed/` holds plain
  managed assemblies, so game code decompiles to ordinary C#.
  Source: local observation + successful decompilation (below).
- `Assembly-CSharp.dll` = 119808 bytes, **145 types**, decompiled with 0 failures.
- `Assembly-CSharp-firstpass.dll` = 75264 bytes, **97 types**, decompiled with 0 failures.
- **`UnityEngine.UNETModule.dll` and `com.unity.multiplayer-hlapi.Runtime.dll` are both
  present in `Managed/`.** The project shipped with Unity's legacy UNET HLAPI package
  installed. `UNETModule` is a stock Unity engine module (always present in 2019.1), but
  `com.unity.multiplayer-hlapi.Runtime.dll` is a *package*, not an engine module — it is only
  copied into a build if something referenced it. Its presence is strong evidence the
  project once imported the HLAPI networking package.
  Source: local directory listing of `EggsForBart_Data/Managed/`.
- **No gameplay code uses any of it.** `grep` across all 242 decompiled files found no
  `NetworkManager`, `NetworkIdentity`, `NetworkBehaviour`, `[SyncVar]`, `[Command]`,
  `[ClientRpc]`, `UNET`, or `NetworkServer` reference in game code. So the dev intended
  multiplayer at some point and never built it. **This is a helpful accident: the UNET HLAPI
  assembly is already in the install, so anything it provides is available to a patch without
  shipping new dependencies.**
  Source: local grep over `work/decompiled/` (2026-10-19).
- Unity Analytics (`Unity.Analytics.*`) is present but unused by gameplay. No online service
  is contacted during play and there is no anti-cheat. Nothing to stay away from.

### 1.3 Game architecture (from the decompilation)

The whole game is small and unusually friendly to patching. Key pieces:

| Concern | Type | Notes |
|---|---|---|
| Local player | `UnityStandardAssets.Characters.ThirdPerson.ThirdPersonUserControl` + `ThirdPersonCharacter` | Standard Assets. `ThirdPersonUserControl.FixedUpdate` reads `CrossPlatformInputManager` axes + `Input.GetButton` and calls `m_Character.Move(move, crouch, jump)`. **The `Move(Vector3, bool, bool)` signature is the perfect input-injection boundary.** |
| Player identity | tag `"Player"` | Every interaction in the game triggers off `coll.tag == "Player"`. Single-player assumption hardcoded everywhere. |
| Egg spawns | `HideEggs` | `spot1..spot6 = Random.Range(1,15)`; `currentnm` is the searchable location id. `Bye()` destroys the egg object for `currentnm`. |
| Egg counter | `SearchBar.eggs` (int) + `eggcount` text `"n / 6"` | Shared progress. Incremented in `SearchBar.Update` when `searched && hadegg`. |
| Search minigame | `Search` + `SearchBar` | `Search.OnTriggerEnter` sets `HideEggs.currentnm` and `SearchBar.search = true`. Mash `Fire2` to fill `mashbar.fillAmount`; at 1.0 `searched = true`. |
| Marge | `basicAI` / `ch2basicAI` | NavMeshAgent FSM, `PATROL`/`CHASE`, own `ThirdPersonCharacter`; `OnTriggerEnter` with tag `Player` switches to `CHASE` and sets `target`. Detection range is driven by `notice` (0-2) which reads `Input` for shift/ctrl/Fire3. |
| Maggie | `MaggieScript` / `Ch2MaggieScript` | Timer + `Random.Range` cry state, `cankill`/`killcount`; calls `Endscript.Death(1)`. |
| Lisa | `LisaScript` | Moves straight at `player` transform every frame, `OnTriggerEnter` -> `Death(2)`. |
| Bart (ch1) | `BartScript` | `BartLoc = Random.Range(0,5)` teleport camping between `b1..b5`. |
| Death / level end | `Endscript` | `Death(int enemy)` freezes the player and loads scene 1; `OnTriggerEnter` fires `victoryon`. `EndLevel1..4` write `PlayerPrefs.SetInt("Level", n)` and load scenes 4..7. |
| Save | `PlayerPrefs` `"Level"` int only | That is the entire persistence: one integer in the registry. |
| Scene flow | `LoadingScreenManager.LoadScene(n)` | loading scene is index 2; gameplay scenes are 1, 3, 4..22; `level0..level22` in the data folder. **Corrected after observation, see §1.6: indices are `0=Start`, `1=TitleScene`, `2=LoadScene`, `3=Chapter1Game`.** |

### 1.4 What this means for co-op

Cheap and safe to sync (all single scalars / small arrays):
- local player transform + animator floats (`Forward`, `Turn`, `Crouch`, `OnGround`, `Jump`, `JumpLeg`)
- `SearchBar.eggs`, `HideEggs.spot1..spot6` and `noRandom`
- `PlayerPrefs "Level"`
- the six AI transforms + their state enums + `BartScript.BartLoc`
- door open/closed flags (`DoorScript`), flashlight on/off (`flashlightscript`)

Risky / needs a design decision:
- **`Endscript.Death()` is global.** It kills *the* player and loads scene 1. In co-op one
  player dying must not end everyone's session. This is the single biggest design problem in
  the game and it is not solved by a transform sync.
- **`basicAI` reads `Input` directly** for its `notice` (detection radius) — it listens to the
  *local* machine's shift/ctrl keys. On a client puppet this must be driven from network state,
  not from that machine's keyboard.
- `LisaScript` chases a single `Transform player` reference. Needs to pick a target among N
  players.
- Every trigger is `tag == "Player"`. A second player object needs the `Player` tag too, or
  all interaction code must be taught about it. Safer: give puppets the tag and accept that
  local-only code must then be guarded.

### 1.5 The one clear path

There is no decompilation and no source port (this is a closed commercial-ish Unity build),
so this is `coop-build-from-zero`. Following `playbook/how-coop-mods-work.md` §2.7 (Nitrox,
the documented Unity+mod-loader precedent):

1. **Loader:** BepInEx 5 (Unity Mono) — ships HarmonyX, loads a plugin DLL next to the exe.
2. **Hook point:** Harmony patches on the methods in the table above.
3. **Transport:** LiteNetLib (reliable UDP, has built-in latency/loss simulation for the QA rig).
4. **Authority:** host runs all AI and the shared world; clients own only their own player and
   send inputs, and receive puppets.
5. **Steam relay** for the friends' connection so nobody needs port forwarding.

This is the "Unity with mod loader" row of the realistic-scope table: players visible in
days, combat in weeks, full campaign in months. Honest expectation, not a weekend.

---

### 1.6 Stage-1 pipeline proof (ran 2026-10-19)

The playbook's Stage-1 exit test — "your code runs inside the game, logs a line each frame,
and can read the local player's position" — **passes.** Reproduced on the byte-identical test
copy at `work/efb-test-01/` (original install untouched).

What was built:
- **BepInEx 5.4.23.5 win_x86** in the test copy. The game is 32-bit; the win_x64 `winhttp.dll`
  is silently ignored (no log at all), so win_x86 is mandatory.
- **`EggsForBart.Coop.Stage1`**, a plugin targeting **net472** (the game's Mono has mscorlib
  4.0.0.0 and no `netstandard` facade, so a netstandard2.0 plugin fails to load with
  `Could not load file or assembly 'netstandard, Version=2.0.0.0'`).
- Two Harmony patches: a **postfix on `ThirdPersonCharacter.Move(Vector3,bool,bool)`** that
  logs the caller's transform, and a **prefix on `FirstLoad.Update`** used only for the
  automated run (see below).

Observations from the run's `BepInEx/LogOutput.log`:
- BepInEx loads under `Unity v2019.1.4.16753575`, CLR 4.0.30319.42000. Both patches apply.
- With auto-entry on, the scene sequence observed live was
  `0 Start` → `2 LoadScene` → `1 TitleScene` → `2 LoadScene` → `3 Chapter1Game`.
- In `Chapter1Game` the postfix logs `[player]` lines every physics step, and the heartbeat
  reports `playerTagged=1`, `thirdPersonChars=2`.
- The two characters are **`Homer` (tag `Player`)** and **`Marge` (tag `Untagged`)**. Homer is
  the local player (stationary at `(64.11, 68.78, 51.79)` — no input is fed, so he does not
  move). Marge is the patrolling AI and her position changes every frame.

Derived from this run (evidence, not guess):
- **The scene indices in §1.3 were wrong and are now observed, not inferred:** `0=Start`,
  `1=TitleScene`, `2=LoadScene`, `3=Chapter1Game`. So `FirstLoad`'s `LoadScene(1)` opens the
  **title menu**, and `Endscript`'s "death loads scene 1" returns to the **title**, not to
  gameplay. Chapter 1 gameplay is scene 3.
- **The `Move` postfix sees AI characters too, not only the local player.** Marge's
  `ThirdPersonCharacter` drives her through the same `Move` method. Any future position-sync
  hook must filter on the `Player` tag or on a `ThirdPersonUserControl` component; logging
  every `Move` is not the same as logging the player.
- **BepInEx logging works normally**, including `Debug.Log`-level game output, so it is a
  usable QA channel.

The automated entry trick, and why it was needed:
- Scene 0 is a **"press any key" gate**: `FirstLoad.Update` is
  `if (Input.anyKeyDown) LoadingScreenManager.LoadScene(1)`. A headless/automated run has
  nobody to press the key, so the game sits in scene 0 forever and the `Move` postfix never
  fires. The earlier attempt (without this) produced a log that stopped right after the patch
  message — the pipeline was proven but the exit test could not be observed.
- The Stage-1 plugin therefore patches `FirstLoad.Update` with a prefix that, when
  `AutoEnterGameplay = true` in `BepInEx/config/eggsforbart.coop.stage1.cfg`, calls the **same**
  `LoadingScreenManager.LoadScene(1)` the human path calls and then skips the vanilla body. It
  drives the real loading coroutine and the real gameplay scene; nothing downstream is faked.
  The flag defaults to **false**, so a normal install behaves exactly like vanilla.
- This is the concrete answer to playbook step 11 ("test with bots, not your owner"): the key
  gate is the automation obstacle in this game, and a Harmony prefix that issues the same call
  the player would is enough to get past it.

## 2. Things derived (read from the code, not stated by anyone)

- Because `ThirdPersonCharacter.Move(Vector3, bool, bool)` is public and takes a plain input
  vector, a Harmony prefix can capture or replace the local player's movement without touching
  the third-person controller's internals. This is the cleanest input-capture point found.
- Because there is exactly one `PlayerPrefs` key (`"Level"`) and one integer of progress,
  save sync is nearly free: replicate `Level` and both sides are in the same place.
- Because eggs are destroyed locally by `HideEggs.Bye()` and the count lives in one int, egg
  sync is a small reliable event + counter, not a per-object system.
- The `UNETModule`/HLAPI presence means a Unity-HLAPI-based approach is *available* without
  shipping dependencies, even though no vanilla code uses it. Not necessarily better than
  LiteNetLib, but worth remembering.

## 3. Guesses (explicitly not verified)

- The drumstick/half-dozen egg limit and the "leave the house" objective probably live in an
  area trigger not yet read (`OOBTrigScript`, `LeaveEvent`, `IndoorTrigger`, `inlocScript`).
  Not confirmed.
- Cutscene scenes (indices 4-7 and the `Cutscene*` scripts) are probably not safe to run
  simultaneously on two clients without a barrier. Not tested.

## 4. Unknowns

- ~~Whether the game runs at all on this machine, and at what framerate.~~ **Partly answered
  (§1.6): it runs and reaches `Chapter1Game`.** Framerate not measured.
- Whether a second instance can run at the same time (single-instance mutex not yet checked).
- Whether `Assembly-CSharp.dll` has usable debug symbols (no `.mdb` was listed for it).
- Exact scene indices for every chapter. Only 0, 1, 2, 3 and 4..22 are evidenced (0/1/2/3
  observed live in §1.6; 4..22 only from `LoadingScreenManager.LoadScene` call sites).
- ~~Whether Unity 2019.1 Mono builds on this machine are compatible with the current BepInEx 5
  release without a Doorstop version pin.~~ **Answered (§1.6): yes, BepInEx 5.4.23.5 win_x86
  loads into this build with no Doorstop pin.**
- What the game does with `Time.timeScale` on death (unconfirmed) and whether pausing affects
  any future network loop.

## 5. Next step

Stage 1 is done (§1.6). The open sequencing question is unchanged: decide, with the owner,
whether to first solve shared-death semantics (the gating design problem) or to build the
smallest possible two-instance thing that proves *networking* (two processes, one host and one
guest, exchange a position). Note that the death path and the networking proof now both have a
concrete blocker to design around: the scene indices show death returns to the **title**, and
the `Move` hook sees AI characters as well as the player.

## 6. Sources

- Game page: https://fleebs.itch.io/eggs-for-bart (fetched 2026-10-19)
- GameJolt mirror: https://gamejolt.com/games/eggsforbart/355488
- Decompiled game code: local, `work/decompiled/` and `work/decompiled-firstpass/`
  (from `<game dir>/EggsForBart_Data/Managed/Assembly-CSharp.dll`, SHA recorded in
  `local/identity.md`)
- Hub playbook: `playbook/how-coop-mods-work.md` §2.7 (Nitrox), §3.1 (BepInEx/Harmony),
  §3.2 (LiteNetLib), §4 (build order)
- Hub skills: `.agents/skills/coop-build-from-zero/SKILL.md`, `coop-triage`, `coop-networking`
