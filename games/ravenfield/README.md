# Ravenfield (2017)

| | |
|---|---|
| Engine | Unity 2020.3.49f1 (Mono) |
| Versions tested | Steam EA39, build 39, app 636480, buildid 26324504 |
| Projects | [RavenM community edition](https://github.com/RavenfieldCommunity/RavenM) · [RavenM official](https://github.com/iliadsh/RavenM) · [RFMP](https://store.steampowered.com/app/1104390) |
| QA level | community-reports |
| Last updated | 2026-11-18 |

## Where things stand

Ravenfield has working online multiplayer today through RavenM, a BepInEx plugin. The
official build stopped at EA29 in 2024; a community edition tracks the current game and
shipped a build for EA39. Per the project, matches are host-authoritative: the host's bots,
vehicles, projectiles and destructibles sync to everyone over Steam's relay, so nobody
port-forwards. The campaign story is not synced — co-op means Instant Action, custom maps and
modes that run as Instant Action.

## Existing work

**RFMP — Ravenfield: Multiplayer Mod** (Steam app 1104390, 2019). A standalone multiplayer
game built from Ravenfield assets. SteamDB lists the last patch on 2020-05-31. The community
docs report that the official servers closed and the developers pointed players to RavenM.
Dead; not an option today.

**RavenM official** ([iliadsh/RavenM](https://github.com/iliadsh/RavenM), MIT). The original
BepInEx plugin. Last release 0.7 on 2024-05-15, built for EA29. It refuses to run on newer
builds: issue #65/#66/#68 report unsupported-version failures from EA30 onward, and the author
has not released since 2024-10. This is the "abandoned mod" of this game.

**RavenM community edition** ([RavenfieldCommunity/RavenM](https://github.com/RavenfieldCommunity/RavenM),
MIT). A maintained fork with builds named per game version: EA35 through EA39, latest asset
`RavenM-EA39-CHNE1d4d0.zip` (2026-10-04), commits through 2026-10-31. Adds fixes (mid-game
join, unload-self, chat, lobby fixes) and tracks the current game. This is what to install.

## How the game works (for co-op)

Unity 2020.3.49f1 with a Mono runtime; game code is `ravenfield_Data/Managed/Assembly-CSharp.dll`.
BepInEx 5 injects through `winhttp.dll` and patches the assembly with Harmony. Ravenfield ships
no netcode. Per the project's README and packet classes (read, not observed on the wire):
RavenM uses Steam lobbies for the session and SteamNetworkingSockets (SDR) for packets, with
host-authoritative simulation. Workshop mods are hashed on join so both sides must run the same
content. Saves are local files under
`%USERPROFILE%\AppData\LocalLow\SteelRaven7\RavenfieldSteam`; RavenM does not sync campaign saves.

## What we changed

No code changes. We triaged the game, confirmed the official mod is EA29-only, found the
community fork, and packaged a pinned installer (BepInEx 5.4.23.5 + RavenM EA39, both
SHA-256-checked). A test copy of the install proved the package before it touched the real
install. Findings that cost time:

- RavenM's `M` menu only opens on the Instant Action screen, not the title screen.
- Host rooms are public by default (`Only Steam Friends Can Join` unchecked). In testing a
  stranger joined within a minute; turn the checkbox on.
- `Packet failed to send: k_EResultInvalidParam` in the log before a lobby exists is harmless.

## QA

What ran on 2026-11-18 against Steam EA39 build 39:

1. Isolated copy of the install (junctions to `ravenfield_Data`, `MonoBleedingEdge`) plus
   BepInEx 5.4.23.5 and `RavenM_EN.dll`; launched, main menu reached, no managed exceptions,
   RavenM version string bottom-left.
2. Instant Action → `M` → `Create Host Room` → `OK`: log `Joined lobby!`, room ID shown,
   members 1/250.
3. Started the match; a remote Steam user connected unprompted (public lobby) and the host
   logged `Accepted the connection` / `Connected to server.`; match kept running.
4. Repeated the load on the real install through the Steam client
   (`steam://rungameid/636480`): BepInEx preloader and `Plugin RavenM is loaded!`, no
   exceptions. Re-ran the installer over the existing install to confirm it repairs instead
   of failing on existing files.

This is a real-client host/join observation, not a scripted assertion suite, so the entry is
`community-reports`. No two-machine playthrough and no campaign test.

## How to play

1. Install Ravenfield through Steam and launch it once.
2. Install BepInEx 5.4.23.5 x64, then drop `RavenM_EN.dll` from the EA39 release into
   `BepInEx\plugins\`.
3. Everyone runs the same game build and the same RavenM build.
4. Host: Instant Action → `M` → `Create Host Room` (enable friends-only) → `OK`; share the
   room ID. Friends: `M` → `Join Room` → paste. Host presses `PLAY`.

If you package this yourself, pin both releases and verify their SHA-256 before installing.

## Known issues

- Upstream labels RavenM WIP: crashes and desyncs happen, reportedly worse with very high bot
  counts (100-200).
- Version lockstep: a Ravenfield update breaks the mod until a matching RavenM build appears.
- Campaign story is not synced.
- Lobby default is public.
- Voice chat is positional and CapsLock push-to-talk.

## Next steps

- Build a two-client harness (two Steam accounts or a LAN fork) that asserts actor positions,
  damage and vehicle state converge; then this entry can move to `automated`.
- Re-test on the next game build (EA40) and record the first failing subsystem.
- Consider patching the friends-only default in the community edition and offering it upstream.
