# FTL: Faster Than Light — co-op notes

## Game identity (observed 2026-11-05)
- Store: Steam, app 212680, buildid 4710954, Linux native (no Proton).
- `<game dir>/data/FTL.amd64` SHA-256 `23897b7e7b538da7b5858e86aed87bd2a1e567d33d12d956761099628f557d80`
  ELF x86-64, "with debug_info, not stripped".
- `<game dir>/data/FTL.x86` SHA-256 `be037b0ba1146bc66e303c99c432104de9aa4ac91b488b7487e63cde4ed87cc8` (ELF i386, debug info).
- Version: 1.6.13 — derived: the amd64 hash equals the one Hyperspace documents for "FTL 1.6.13 Steam, Linux amd64"
  (https://github.com/FTL-Hyperspace/FTL-Hyperspace, wiki/FTL-Source-Layout.md). A string "1.6.29" also occurs in the
  binary; its meaning is unknown (not the game version per the hash match).
- Engine: Subset's own C++ engine (SDL/OpenGL); data in `ftl.dat`; libsteam_api.so shipped.

## Prior co-op attempts (research 2026-11-05)
- tomahawk812/FTL---Faster-than-light-Multiplayer — README-only stub, last push 2013-09-06. Nothing usable.
- Tachyon / TachyonsCE (https://github.com/Dav-Edward/TachyonsCE) — standalone FTL-like multiplayer game, not a mod.
- Final Frontier (Garry's Mod gamemode, https://www.pcgamer.com/ftl-garrys-mod/) — separate first-person game.
- Subset Games: no plans for multiplayer (quoted at https://playco-opgame.com/ftl-faster-than-light/).
- No co-op mod found on Steam discussions (https://steamcommunity.com/app/212680/discussions/0/558752449822405389/).
- Hub: no directory entry, no issues/PRs for FTL (checked 2026-11-05).
- Claim issue: https://github.com/cranebeaker/coop-anything/issues/329 (opened 2026-11-05).

## Building blocks
- FTL-Hyperspace (GPL-3.0-or-later, active: release v1.23.2 2026-09-25, push 2026-11-04): binary hook mod with Linux
  1.6.13 build scripts. Not multiplayer. Derived: best base for a co-op layer (hooks, function map from DWARF already done).

## Path
- Build from zero (AGENTS.md path 2); nothing to revive.

## Design decisions (owner, 2026-11-05)
- Ship sharing is a host setting, switchable mid-run: free-for-all (anyone commands anything) and own-crew
  (players command their crew + systems those crew staff). Host-authoritative; guests send commands; the mode is a
  permission filter on the host. Free-for-all built first. Captain-only (for now): jump, events, store, upgrades.
