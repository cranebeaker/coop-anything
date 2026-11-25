# Starsector (2011)

| | |
|---|---|
| Engine | Proprietary Java ("Star Engine"), LWJGL, public mod API |
| Versions tested | fractalsoftworks.com 0.98.5a-RC9 (2026-11-23 hotfix) |
| Projects | [Starsector Coop](https://github.com/AyoKeito/starsector-coop) + [openmans RC9/Wingman-battles port](https://github.com/openmans/starsector-coop) (campaign, 2p, RC9) · [The Wingman Project](https://github.com/NEOysod/TheWingmanProject) (battles, RC9) · [Cooperative Multiplayer Combat](https://fractalsoftworks.com/forum/index.php?topic=11598.0) (couch) |
| QA level | community-reports (Wingman: author-tested two players; Starsector Coop: author's automated suite, 3,791 tests) |
| Last updated | 2026-11-25 |

## Where things stand

Both halves of the dream now run on the current build (0.98.5a-RC9): a two-player shared
campaign (sector, markets, colonies, clock) via Starsector Coop, and real-time shared battles
inside that campaign. The openmans fork ports Starsector Coop 0.2.1 (pinned to RC5, which is no
longer downloadable) to RC9, and its 0.2.3 adds Wingman battles: the coop host opens a Wingman
session from the campaign map, the guest joins the host's battles over Wingman and pilots a ship
in real time, with real losses reconciled back into the shared campaign. Still true: exactly
two players, story missions are the host's, and battles without Wingman stay solo (AI-ally
toggle).

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
  RNG post-procgen, #6).
- **openmans port** (fork of Starsector Coop). 0.2.2 ports the 0.2.1 preview to 0.98.5a-RC9
  (all 21 forked engine classes verified against the RC9 jars; RC9's added
  `Misc.hasFunctionalHeavyIndustry` carried into the fork; full-fidelity guest-system drive
  retargeted to RC9's `BaseLocation.advance` signature). 0.2.3 adds Wingman battles: host opens
  a Wingman session from the campaign map, guest connects Wingman to the host once and flies in
  the host's battles; the guest's flagship is reserved via WingmanAPI; the coop battle bridge
  stays a spectator inside the Wingman join battle while `BATTLE_BEGIN/END` and the shared
  pause keep driving the campaign clock; a new reliable `WINGMAN_HELLO` message carries the
  guest's Wingman name. Fully degrades to the solo model without Wingman. Unit tests green
  (4,5xx); live two-PC battle test pending (owner-run).
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
