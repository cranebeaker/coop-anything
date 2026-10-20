# Eggs for Bart (2019)

| | |
|---|---|
| Engine | Unity 2019.1.4f1, Mono scripting backend (32-bit Windows build) |
| Versions tested | itch.io v3.0 Windows (`Eggs_For_Bart3.0-WINDOWS`), exe version 2019.1.4.16753575 |
| Projects | none yet — this is a gap entry; research + pipeline proof in [NOTES.md](NOTES.md) |
| QA level | none (no playable co-op exists yet; one automated plumbing test has run, see QA) |
| Last updated | 2026-10-19 |

## Where things stand

**Nobody can play Eggs for Bart together today.** No co-op mod, multiplayer mod or netplay
patch exists for this game anywhere — not in this hub, not on GitHub, not in web search. This
page records triage, a code map, and the first step of a build-from-zero attempt: the mod
loader and the patch pipeline are proven to work on the shipped v3.0 build.

## Existing work

None found. Searched 2026-10-19: this hub's `DIRECTORY.md` and `games/`, hub issues and PRs,
GitHub repo search, web search for a co-op/multiplayer mod. No Discord server found for the
game; no Nexus page. Details in [NOTES.md](NOTES.md) §1.1.

## How the game works (for co-op)

- **Engine/scripting:** Unity 2019.1.4f1, Mono backend — *not* IL2CPP. `EggsForBart_Data/
  Managed/Assembly-CSharp.dll` and `Assembly-CSharp-firstpass.dll` are plain managed assemblies
  and decompile cleanly (145 and 97 types, zero failures). The itch page claims "Unity 5
  Personal"; that is stale metadata and wrong for the v3.0 build.
- **Netcode:** none in gameplay. Zero `NetworkManager` / `NetworkIdentity` / `[SyncVar]` /
  `UNET` references across the code. `UnityEngine.UNETModule.dll` **and**
  `com.unity.multiplayer-hlapi.Runtime.dll` do ship in `Managed/`, so the project once imported
  Unity's legacy HLAPI package and then never used it. That assembly being present means a
  UNET-HLAPI approach is available without shipping a new dependency, though LiteNetLib is the
  better fit.
- **Save system:** the entire persistence is one `PlayerPrefs` integer, `"Level"`. Syncing it is
  nearly free.
- **Level loading:** `LoadingScreenManager.LoadScene(n)` switches to a loading scene (index 2)
  which then async-loads the target. Scene indices were observed live, not guessed:
  `0=Start` (a "press any key" gate, `FirstLoad`), `1=TitleScene` (the title menu),
  `2=LoadScene`, `3=Chapter1Game`. So `FirstLoad`'s `LoadScene(1)` and the death path's
  `LoadScene(1)` both go to the **title**, not to gameplay.
- **Client-side vs server-side:** there is no server. The single player owns everything. The
  local character is driven by `ThirdPersonUserControl.FixedUpdate` → `ThirdPersonCharacter.
  Move(Vector3, bool, bool)`, a public method — the cleanest input-injection boundary in the
  game. Every interaction in the game keys off the object tag `"Player"`. The AI (Marge) runs
  a NavMesh FSM and calls the *same* `Move` method, so a patch on `Move` sees the AI as well as
  the player and must filter.
- **Tools used to read it:** ILSpy 8.2 CLI against the managed assemblies (no Python on the
  research machine; the directory generator was ported to C# locally).

### Co-op outlook

Tractable as a build-from-zero: small game, plain Mono, one int of save state, one obvious
input boundary, eggs tracked by a single counter plus six spawn slots, and AI/world state
reduce to a handful of transforms and enums. The one genuinely hard design problem is **shared
death**: `Endscript.Death()` is global — it kills *the* player and returns to the title. In
co-op, one player dying must not end the session, and nothing in the code supports that yet.
See [NOTES.md](NOTES.md) §1.4.

## What we changed

Nothing that affects the game yet — the original install has not been touched. Work is in a
byte-identical test copy. Two findings so far, both from a run on 2026-10-19:

1. **BepInEx 5 loads into this build.** BepInEx **5.4.23.5 win_x86** is required: the game is
   a 32-bit process, and the win_x64 `winhttp.dll` is silently ignored (no log at all). Loads
   under `Unity v2019.1.4.16753575`, CLR 4.0.30319.42000, no Doorstop version pin needed.
2. **Plugins must target net472.** The game's Mono has `mscorlib 4.0.0.0` and **no**
   `netstandard` facade, so a netstandard2.0 plugin fails to load with
   `Could not load file or assembly 'netstandard, Version=2.0.0.0'`.

With those two facts, a proof-of-concept plugin applied a Harmony postfix to
`ThirdPersonCharacter.Move(Vector3, bool, bool)` and logged the local player's transform every
physics step in `Chapter1Game`. The log confirms it sees the player (`Homer`, tag `Player`) and,
separately, an untagged AI character (`Marge`).

## QA

This is **not** a playable co-op build, so the QA level is `none` in the directory. One
automated plumbing check has run:

- **How to run:** copy the game to a test folder, install BepInEx 5 win_x86, build the Stage-1
  plugin (`net472`, `x86`) against the game's own assemblies, set `AutoEnterGameplay = true` in
  the BepInEx plugin config, launch the exe, and inspect `BepInEx/LogOutput.log`.
- **What it asserts:** BepInEx loads under Unity 2019.1.4; both Harmony patches apply; the scene
  sequence `0 → 2 → 1 → 2 → 3` is reached; `[player]` position lines appear every physics step;
  the heartbeat reports exactly one tag-`Player` object and two `ThirdPersonCharacter`s.
- **Last result:** passed, 2026-10-19. Automated runs are headless, so the plugin patches
  `FirstLoad.Update` to issue the *same* `LoadingScreenManager.LoadScene(1)` a human's key press
  would, then skip the vanilla body; the flag defaults off so a normal install is untouched.
  Full log excerpt and reasoning in [NOTES.md](NOTES.md) §1.6.

## How to play

Not playable in co-op yet. When there is something to try it will be a BepInEx plugin plus the
player's own copy of the game (the game is a paid/itch download, so no game files are
redistributed).

## Known issues

- No co-op implementation exists. Everything below is design debt for whoever builds it.
- **Shared death is unsolved.** Death is global and returns to the title menu; a co-op session
  needs per-player death and a host-owned respawn/round decision.
- **The AI reads the keyboard.** `basicAI.notice` (the detection radius) is driven by the local
  machine's Shift/Ctrl keys, so on a client it must be fed from network state, not local input.
- **`LisaScript` chases one `Transform`.** It needs a target-picking rule among N players.
- **Tag collisions.** Every trigger tests `tag == "Player"`; a second player object must carry
  that tag or all interaction code must learn about it.

## Next steps

1. Decide, with the owner, the sequencing: solve shared-death semantics first, or build the
   smallest two-instance networking proof (one host, one guest, exchange a position).
2. Build the two-instance networking layer (LiteNetLib transport, host-authoritative) on top of
   the proven BepInEx + Harmony base, with a bot harness driving the second instance rather than
   requiring a human playtester.
3. Handle the AI/player distinction in the `Move` hook (filter on the `Player` tag or the
   presence of a `ThirdPersonUserControl`).
4. Write the eventual mod up in a separate repo (its own code only, no game files) and link it
   from this page, then flip the directory entry from `kind: gap` to the real project.
