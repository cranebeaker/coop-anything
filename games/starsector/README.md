# Starsector (2011)

| | |
|---|---|
| Engine | Proprietary Java ("Star Engine"), LWJGL, public mod API |
| Versions tested | fractalsoftworks.com 0.98.5a-RC9 (2026-11-23 hotfix) |
| Projects | [The Wingman Project](https://github.com/NEOysod/TheWingmanProject) (battles, RC9) · [Starsector Coop](https://github.com/AyoKeito/starsector-coop) (campaign, 2p) · [Cooperative Multiplayer Combat](https://fractalsoftworks.com/forum/index.php?topic=11598.0) (couch) |
| QA level | community-reports (Wingman: author-tested two players; Starsector Coop: author's automated suite, 3,791 tests) |
| Last updated | 2026-11-25 |

## Where things stand

You can play battles together today, and only battles: The Wingman Project 0.9.4-beta (updated
2026-11-25) tracks the current game build 0.98.5a-RC9 and lets the host plus up to three friends
each fly a ship from the host's fleet in any battle. The full-shared-campaign mod (Starsector
Coop) exists and is actively developed, but each release is pinned to one game build and the
build its previews target (RC5) is no longer offered for download, so it is not installable
today; the 2-player limit and "one player pilots each battle" design also stand.

## Existing work

- **The Wingman Project** (NEOysod, GPLv3). Online co-op battles; host + 1-3 joiners, each flies
  a ship of the host's fleet (missions, simulator or campaign battles). Synced: ships/fighters/
  missiles, battle world, loadouts/officers/skills, host-authoritative hits. Not synced: the
  campaign (host-only). Port 24816 TCP (+UDP optional), direct IP / virtual LAN / port forward,
  plaintext + optional password. Version 0.9.4-beta targets RC9 (protocol 63, fallback names for
  RC8-). Very active: several releases a week in Nov 2026, commits authored with Claude Code.
  Latest packaged release zip is 0.9.3-beta-RC6; the RC9 build is on `main` with prebuilt jars.
- **Starsector Coop** (AyoKeito, CC BY-NC 4.0). Full 2-player campaign: shared sector, NPC
  fleets, markets/storage/colonies/reputation, coordinated clock; battles are solo with an
  optional AI-ally toggle. Stable 0.1.5 → 0.98a-RC8; preview 0.2.1 (2026-11-12) → 0.98.5a-RC5.
  Version- and seed-locked handshake; launcher (Windows) edits vmparams/enabled_mods with
  backups. Bug reports: GitHub issues (open: COOP-SEED false positive from mods using unseeded
  RNG post-procgen, #6). No build for RC7+ yet; RC5 download no longer offered officially.
- **Cooperative Multiplayer Combat** v3.10 (2026-05). Shared-screen/couch combat co-op for
  0.98a, up to three friends on one PC. Not remote-play.

## How the game works (for co-op)

No netcode in vanilla. Single JVM process per player (bundled JRE), everything — campaign state,
battle sim — is local, so both projects are host-authoritative sync layers written as mods.
Wingman rides the public modding API for most of the battle sync plus game internals of the
specific RC for the title-menu and a few hooks (its changelog lists which internals RC9 renamed);
Starsector Coop forks engine classes (that is why its version pin is hard) and adds an external
launcher. Saves are XStream XML; Wingman does not touch them (campaign stays vanilla), Starsector
Coop stores its state in the saves. Script sandbox: mods cannot read jar bytes; mod parity is
checked by mod_info.json hash.

## What we changed

Nothing yet — setup only. Installed RC9 + Wingman 0.9.4-beta (jar hash verified against repo
main), enabled the mod, opened Windows Firewall TCP+UDP 24816, packaged the mod folder + agent
launcher for the friends to install. First game run (serial registration) and a live two-PC
battle test are the open items; see NOTES.md.

## QA

- Author-side: Wingman — "tested plenty, still might be bugs", mostly two players over a virtual
  LAN (README). Starsector Coop — automated suite, 3,791 tests passed + two-player smoke on the
  release build (0.1.5 changelog).
- Ours: none yet. Plan: two game copies on the LAN (second PC needs its own game key), host a
  simulator battle, assert both logs show join + ready + no COOP-DOCTOR errors.

## How to play

1. Every player needs their own Starsector copy (sold per key) at the SAME build: 0.98.5a-RC9.
2. Every player: unzip Wingman into `mods/Wingman`, enable "The Wingman Project" in the
   launcher. Same mods and versions on all PCs (utility mods may differ).
3. Host: start a battle; friends connect to host:24816 (LAN, virtual LAN, or port-forward
   24816 TCP+UDP). Set a password if the port is reachable from strangers.
4. Joiners: title screen → CO-OP → address → pick a ship in the lobby; ready check starts the
   battle. In campaign battles a lost ship is lost for real.

## Known issues

- Beta: mostly two-player testing; 4-player is the design target, least tested.
- Joiner sees own controls instantly, the rest arrives with link delay.
- A game hotfix can rename the internals the title-menu hooks use; until the mod updates those
  parts switch off and the "Wingman: Join Game" mission still works.
- No encryption (plaintext + optional password); prefer a private virtual LAN.

## Next steps

- Live LAN test with two game copies; then a friends session over the owner's WireGuard.
- Watch starsector-coop for an RC9 port (issue/PR updates); then decide whether to add campaign
  co-op alongside.
- Contribute bug reports from real sessions to Wingman (F3 "Detailed log" on both PCs).
