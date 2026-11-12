# Kill The Crows — session notes

## Game identity (Phase 0, recorded 2026-11-08)

- Title: Kill The Crows (5minlab / KRAFTON)
- Store: Steam, appid 2441270 (folder name "Revolver" — the working title)
- Steam buildid: 16441009; StreamingAssets/build_info: "Build from DESKTOP-I89R27F at 2024-11-18"
- SHA-256 of revolver.exe: 0da2a14c143a0959c075f9cf401f2fc2875de6d031079ae7b3d2cfb7c8ba2909
- exe size 666624 bytes, FileVersion 2022.1.5.16706142 (Unity version stamp)
- Engine: Unity 2022.1.5f1 (Mono — MonoBleedingEdge + Assembly-CSharp.dll present)
- Install: D:\SteamLibrary\steamapps\common\Revolver (private, not committed)
- steam_appid.txt ships in the install → two simultaneous local instances init Steam fine.

## Phase 0 — triage result

- No entry in DIRECTORY.md / data/directory.yaml; no prior projects found on the web
  (web search 2026-11-08: single-player only, no known co-op mods).
- Game: top-down one-hit-kill wave arena roguelite; single gameplay scene;
  Steamworks.NET, UniRx/UniTask, Odin, GameAnalytics shipped; no netcode libraries.
- → Path: build from zero (coop-build-from-zero). BepInEx 5.4.23.2 + HarmonyX.

## Phase 1..3 — build (2026-11-08..09)

- Decompiled Assembly-CSharp.dll with ilspycmd 11.0 → 356 files (kept outside the hub).
- Mod source: `<work dir>/KillTheCrowsCoop/Mod` (SDK-style csproj, net462, references
  game Managed + BepInEx core; `dotnet build` auto-installs to BepInEx/plugins).
- Architecture: host-authoritative, LAN UDP 7777 custom transport (reliable seq/ack
  events + unreliable state streams), static session state + scene-loaded pump
  recreation (the game kills DDOL roots), proxies for enemies, ghost Player for the
  client on the host, damage via host physics, shared kill counter for boss pacing.
- Test harness: `-ktcHost` / `-ktcClient` / `-ktcLog=` command-line switches; auto
  host→join→run→god-mode bots firing at the nearest enemy (see README QA section).

## Incidents worth remembering

- `System.ValueTuple` TypeLoadException in Unity Mono — replaced all tuples with structs.
- Transport timeout compared ticks to milliseconds → instant handshake death; fixed.
- Client transport gate kept `Connected=false` until first RX → Hello never sent
  (both sides waiting) — client must mark itself connected when the remote is known.
- Disabling stripped proxy components removes them from `Character.Characters`
  (OnDisable) — re-add after stripping.
- Boss-death camera NRE window (vanilla) — healed by re-targeting the camera when
  `camTarget == null` (auto-test only).

## Status

v0.3: LAN co-op + Steam lobby path (boot-tested) + partner revive (full cycle
automated-tested). PR #351 open against cranebeaker/coop-anything.

## Extra incidents

- Revive shipped with a ValueTuple field again (Revive._invuln) - same
  TypeLoadException; replaced with a small struct. Rule: no tuples, ever, in this
  runtime.
- Revive-protection used Player.isInvincibleOneCoinMore with a timed reset that only
  the host's damage path evaluated - the client stayed invulnerable forever after
  its first revive. Expired unconditionally in the pump update.
- UDP reordering can replay a stale alive=true after the death packet, which
  re-triggered the death edge and double-charged a revive credit; stale
  resurrections are ignored while a revive is pending.
