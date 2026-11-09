# Assassin's Creed Odyssey (2018)

| | |
|---|---|
| Engine | AnvilNext 2.0 (Ubisoft, proprietary) |
| Versions tested | Steam app 812140, build 17083392; `ACOdyssey.exe` x64, 286,453,072 bytes, SHA-256 `ac327dad2cbbdd72a3fda8e99cbeab9d12af328363e4f09bc5674bdd36b8c483` |
| Projects | none; directory entry "Assassin's Creed Origins, Odyssey and Valhalla" (gap) |
| QA level | none |
| Last updated | 2026-11-09 |

## Where things stand

The story can't be played together today. No co-op mod or fork exists, and the
build-from-zero path is blocked because the exe is DRM-protected (see below).

## Existing work

None found. The directory search on 2026-10-16 covered GitHub, Nexus, ModDB and Steam.
The nearest series option is Assassin's Creed Unity, which has official co-op side missions.

## How the game works (for co-op)

- **Exe protection:** `game_identity` shows `.vmp0` / `.vmp1` sections, which means
  VMProtect, and the game also ships with Denuvo. Static analysis would need an unpacked
  binary, i.e. DRM removal, which this project does not do or publish.
- **Netcode:** none. The game is single-player only, so it has no replication layer to reuse.
- **Source and modding:** no source, decompilation or script extender is known.
  `detect_engine` found no AnvilNext signature; the only extra signal was `steam_api64.dll`.
- **Launch:** the Steam build requires Ubisoft Connect.

## What we changed

Nothing; this is a triage record.

## QA

No tests; there is nothing to test.

## How to play

There is no co-op build. Players who want to play together can share one session with a
streaming tool such as Parsec or Steam Remote Play. That is not real co-op.

## Known issues

None; there is no build.

## Next steps

Revisit if a legitimate modding route appears, such as an official mod API, a script
extender that runs without unpacking the exe, or a DRM-free release. Origins and Valhalla
are probably blocked the same way, but nobody has checked yet.
