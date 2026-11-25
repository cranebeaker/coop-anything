# Starsector co-op — NOTES

Session notes, kept as the work happens (AGENTS.md rule 3). Owner: local Windows install.
Written 2026-11-25.

## Game identity

- **Game:** Starsector **0.98.5a-RC9** (hotfix 2026-11-23), store: fractalsoftworks.com (BMT
  Micro direct sale, no Steam). Latest version as of 2026-11-25.
- **Installer:** `starsector_install-0.98.5a-RC9.exe` (NSIS), 314 MB,
  SHA-256 `d7563c1900b49c6f6acbaccd9f979b9d921f9f6b0ce761cef22be9c62cc60e75`.
  A second download, `Starsector_0.98.5a-RC9.exe` (294 MB,
  SHA-256 `f4d51141262b06d490305243f82d4bdf66344e66d7ff04ae4c65a7c8f3f6dac6`, torrent), differs
  — not an NSIS binary; the NSIS one was used.
- **Install:** `<game dir>` (owner convention). starsector.exe
  SHA-256 `49ca31596bced575ce8bd2dee4c0687b0a3024629a36c209c36ddee9826aa681`.
- **Engine:** proprietary Java ("Star Engine"), LWJGL, moddable via public mod API
  (mod_info.json + jars + data/), script sandbox. No netcode in vanilla.

## Existing co-op work (all found)

### The Wingman Project (NEOysod) — CHOSEN
- https://github.com/NEOysod/TheWingmanProject — online co-op **battles**, host + up to 3
  joiners each fly a ship from the host's fleet. Campaign stays single-player. GPLv3.
- Version **0.9.4-beta**, committed 2026-11-25 (same day): "Starsector 0.98.5a-RC9 (protocol 63)",
  chat on campaign map, officers for friends, refit rework, ping, rejoin, UI scale.
  mod_info gameVersion `0.98.5a`; the RC9 build also tolerates RC8-and-older names via fallback
  tables (changelog says so; not tested by us).
- Latest release ZIP is `v0.9.3-beta_RC6` (for RC6). The RC9 build lives on `main` with prebuilt
  jars: Wingman.jar SHA-256 `3f094ee9e88de1a900730ce8b97425d28e2999dc57fbf6cf009df0a8617a6ea8`
  (verified against the file the owner installed — matches repo `main`).
- Port **24816** TCP (+UDP optional). Direct IP; LAN, virtual LAN (Radmin/Tailscale/ZeroTier/
  Hamachi) or port forward. Password supported; traffic plaintext.
- Bug reports: GitHub issues + forum thread 36153.
- Installed 2026-11-25 by the owner at `<game dir>\mods\Wingman` (full repo clone — extra
  src/test/docs folders are inert). enabled_mods.json: `wingman_project`.
  Windows Firewall rules "Starsector Wingman TCP/UDP" 24816 inbound added 2026-11-25.
- Agent (`jars/WingmanAgent.jar`, optional, `-javaagent`) for mods' player-ship scripts; bat +
  ps1 placed in game root by owner.

### Starsector Coop (AyoKeito) — full campaign co-op, 2 players
- https://github.com/AyoKeito/starsector-coop — shared sector, markets, storage, colonies,
  reputation, coordinated clock. Host-authoritative. One guest. CC BY-NC 4.0.
- Stable 0.1.5 targets **0.98a-RC8**; preview 0.2.1 (2026-11-12) targets **0.98.5a-RC5**.
  Port 7777 TCP+UDP, UPnP/NAT-PMP/port-forward/VPN, launcher (Windows-only) edits vmparams +
  enabled_mods (with backup), version + seed lock refuse mismatched installs.
- **Blocker for us:** no build for RC7/RC8/RC9. Version check refuses mismatch (a Developer
  override exists but is explicitly unsupported — the mod forks game classes). RC5 is no longer
  on fractalsoftworks.com/starsector/release/ (only 4 recent builds: RC6-RC9 per the index
  listing). Older-version archives not offered officially. → campaign co-op is not installable
  today without an old RC5 installer.
- Automated checks exist (3,791 tests + two-player smoke, per 0.1.5 changelog).
- Open issue #6 (2026-11-15, Chinese players): COOP-SEED false positive with Knights of Ludd
  (mod creates systems post-procgen with unseeded Math.random()); `coop.adoptCampaignId`
  consumed but doesn't bypass. Open PR #4 (wumingsaberarcher): proximity/remote combat join
  phase 2. Author active until 2026-11-12; quiet since (RC7/8/9 not yet ported).

### Cooperative Multiplayer Combat v3.10 (2026-05-02)
- Forum topic 11598: couch / shared-screen combat co-op, up to 3 friends, for 0.98a.
  Not a fit for remote friends.

## Directory status in coop-anything hub

- No Starsector entry in DIRECTORY.md, no `games/starsector/` folder (checked 2026-11-25).
  Prepared: 3 directory entries + `games/starsector/README.md` (see PR branch).

## What was done locally (2026-11-25)

- Verified owner's install: game RC9 + Wingman 0.9.4-beta, jar hash matches repo main.
- Firewall: TCP+UDP 24816 inbound, all profiles (needs admin; done via UAC).
- Friends package: a friends package ZIP (mod folder + agent launcher) (mod folder + agent bat/ps1).
- Game first run (serial registration) and co-op smoke test: **pending owner** — the game has
  never run (saves/ empty, no Java prefs), first launch asks for the serial key.

## Observed / derived / unknown

- Observed: everything above with a hash, a link, a date, or a command that ran.
- Derived: RC5 is gone from the official release index (derived from the index listing showing
  only 4 builds; the site is Cloudflare-walled for bots, so this was not re-verified by direct
  fetch). "RC9 build tolerates RC8 names" is from the changelog, untested.
- Unknown: whether friends are on LAN or internet; whether Wingman 0.9.4 RC9 tolerates the
  RC9-since-update game classes in real battles (only author-tested); whether starsector-coop
  will get an RC9 port.


## 2026-11-25 (session 2): RC9 port shipped + Wingman battles (0.2.3)

- Forked upstream to openmans/starsector-coop, branch port/rc9 from v0.2.1.
- Port to 0.98.5a-RC9 (0.2.2): javap -p -s over all 21 fork classes showed identical signatures
  and field layouts; only real gap was Misc.hasFunctionalHeavyIndustry (added by a 0.98.5a
  hotfix) — carried into the fork. Full-fidelity driver's ADVANCE_PARAM_CLASS updated to RC9's
  com.fs.starfarer.util.oOOO$new. StarfarerSettings toggle handles and CampaignState.fastForward
  verified unchanged. Full gradle suite green (4,546 tests). Two-client run (owner-driven):
  version check MATCH, forks active, session up, campaign replicated.
- 0.2.3 Wingman battles (battle sync through The Wingman Project): new coop.wingman package
  (reflective WingmanAPI access, fails closed), coop.wingmanBattles option (default on),
  WINGMAN_HELLO reliable message (guest's Wingman name), host openHost/pump/reserveShip of the
  guest mirror flagship, guest-side bridge latch that keeps the coop bridge a spectator inside
  the Wingman join battle. 9 new unit tests + policy tables updated. Full suite green.
- Battle sync verification: NOT yet run live (owner will test). Risks to watch: Wingman's
  reserveShip matching against the ally-deployed mirror members; the guest-side latch race at
  battle end (handled by latching until the first campaign frame); Wingman port assumption
  (24816 default) in the guest hint.
