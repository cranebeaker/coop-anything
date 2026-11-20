# Hollow Knight: Silksong: working notes

Owner request (2026-11-15): couch co-op on one PC like LEGO or Resident Evil 5; player 2 is a copy of player 1;
money and items shared; each player free to go anywhere on the map; two copies on two monitors for a light game,
split screen otherwise.

## 2026-11-15: triage

Observed:
- Steam appid 1030300, buildid 22479045, version string 1.0.30000 in `Assembly-CSharp.dll`, Unity
  6000.0.50f1-uum-100966-branch1 (Mono), D3D12 present, Windows 11. Exe SHA-256 `5957b049...0d836496`,
  `Assembly-CSharp.dll` SHA-256 `70713bb2...4de7114bb`. Clean install, no BepInEx.
- Hub: only SilklessCoop in the directory; claim issue #300 (Silksong Together); no PRs.
- GitHub search (`silksong coop`, `silksong multiplayer`, `SilksongTogether`) found the projects in README.md.

Derived:
- Different rooms in one process would need two loaded scenes and two cameras; nothing does that. One copy per
  player gets it for free; a mod with a shared wallet plus Couchlink covers the rest. Silksong Co-op (Nirok85) is
  the only one found that shares rosaries.

## 2026-11-15: workspace

- Two copies of the install (7.6 GB each); mod v3.3.7 zip and Couchlink 0.1.21 zip, both hash-checked against the
  release digests. `steam_appid.txt` in each copy so the exe does not restart through Steam.
- Mod on this build: 7 Harmony patch classes, 0 failed.
- No .NET SDK on the PC: the plugin builds with `%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe` (C# 5),
  `/nostdlib` against the game's `mscorlib`, `netstandard`, `UnityEngine*`, `Assembly-CSharp`,
  `TeamCherry.SharedUtils`, plus BepInEx and 0Harmony. Old csc reads the Unity 6 assemblies fine.
- Game code read with Mono.Cecil from PowerShell (`BepInEx\core\Mono.Cecil.dll`): list IL, find string literals and
  member references. Enough to find the save, options, slot loading, exit and currency APIs.

## 2026-11-15: incidents and fixes, in order

1. First solo run stopped at the language screen: the copy's fresh save folder had `shared.dat`, but the language
   flag `GameLangSet` is a PlayerPrefs value. Fix: per-copy `localSharedData` (`JsonSharedData`) with the flag set.
2. The agent's shell ran in a sandbox that hid the real `HKCU`. The first two test runs had created
   `HKCU\Software\Team Cherry\Hollow Knight Silksong` (window size 960x540, session count 2) without the agent
   seeing it. Found with an unsandboxed `reg query`; the key was removed again (it did not exist before: the copies
   asked for the language and the session count was 2). Lesson in `playbook/multi-instance-testing.md`.
3. Pair run waited forever for the hero: a loaded save sits on a bench with `acceptingInput == false`.
4. Bot room change bounced between two rooms (`BeginSceneTransition` without a leave direction). Fix:
   `TransitionPoint.DoSceneTransition(true)` by reflection.
5. Shards check failed: the save's shell shards were at the 700 cap, so adding did nothing even locally. Bot spends 3.
6. Desktop screenshots for checking windows caught the owner's other apps; deleted. Use game logs instead (as
   `playbook/multi-instance-testing.md` already says).

## Results

See README.md, section QA. Runs cl1 and cl2 passed 14/14; cl2 also passed the per-player input check.

## Unknown

- Real controllers, keyboard players, long sessions, enemies and bosses in the couch setup.
- Whether Silksong Co-op v3.3.9 betas behave the same (only 3.3.7 tested).
