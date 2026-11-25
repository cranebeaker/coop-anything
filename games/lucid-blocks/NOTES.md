# Lucid Blocks — agent notes

Session 2026-11-15. Owner wants two-player co-op with friends.

## Observations (checked)

- Steam app `3495730`, installed, build `26206996`, exe `5.2.0.0`.
- exe SHA-256 `CB6EA42754BA9075F4FC2141AEC39A13159DA7DDF835F13334D367E9FBC4A558`.
- Engine: Godot 4.6 **double**, GodotSteam 4.17.1 (exe version text + `.dll` names).
- Install is one exe with an embedded PCK v3 (header at `0x03F84820`, 7,522 files,
  1,499,486,968 bytes of files). Steam store lists Single-player only, no Remote Play
  Together and no co-op tags.
- Built-in mod loader decompiled from `main/autoload/mod_loader.gdc`:
  - scans `mods/` next to the exe for `*.pck`, calls `ProjectSettings.load_resource_pack`
    (default `replace_files=true`, so a mod can replace `res://main/...`),
  - then instantiates every `res://mods/<name>/mod_init.gd` it can find,
  - prints `Game version: <application/config/version>` and `Mod: <file>`.
- `mods/` does not exist in the install; no mods installed. User data in
  `%APPDATA%\lucid blocks` has no co-op files.
- No Lucid Blocks entry or issue in `cranebeaker/coop-anything` (directory and issue
  search checked 2026-11-15).
- Existing co-op work (all found via GitHub search, 2026-11-15):
  - `parkers0405/lucid-blocks-coop` — 25 stars, first co-op mod, 5 releases, last build
    2026-07-05 for game **4.0.1**. Open bugs: crash on 5.0.1 (#12), stutter (#11),
    water not updating on client (#2), reconnect button dead (#3), client can kill
    Meeshu (#5), capsule spawning client-specific (#9), Tiamana-leyline break (#8).
  - PR #16 by `trism-prism` against parkers (open, 20 files, +5101/−1022, last commit
    2026-11-12) claims compatibility with build 26206996 / 5.2.0 and adds independent
    dimensions, personal Apotheosis, identity auth, personal ender chest, ownership
    handoffs. Fork: `trism-prism/lucid-blocks-coop-updated`, branch main pushed
    2026-11-14. README targets exactly this build, warns the inherited `dist/` builds are
    stale, and says no new release was installed or published.
  - `Syphon1205/lucid-blocks-coop` — July 2026 fork of parkers, sync fixes.
  - `Puzzku/Lucid-blocks-COOP-mod` — 0 stars, Oct 2026, binary-only `.pck` release
    claiming 5.2.0 support, source absent. Not auditable; low trust.
  - `sos-mislom/lucid-blocks-multiplayer` — May 2026 dedicated-server experiments.
- Community infrastructure: base game has a mod loader; `sordsith/BubbleLoader` extends
  it with `override.cfg` + a C++ console; other mods use a "Qualia" mod API.

## Derived

- Path 1 (revive) applies. The most credible current-version work is trism-prism's PR
  #16, not the 4.0.1 releases. Building it from source and testing on 5.2 is the
  shortest route to a playable install.
- Because the mod is GDScript plus `.pck` overrides, an install is one file in `mods/`
  and is reversible by deleting it.
- A matching Godot 4.6 **double** editor is not published in the official
  `godotengine/godot-builds` release (only single and mono win64). We can still pack
  with GDRE 2.7.4 and test with the real game exe; the editor is only needed to
  generate `.godot/imported` assets (one GLB) and to run the author's headless script
  gate.

## Unknowns

- Whether PR #16 actually works on 5.2 in a live two-client session (author says a real
  Steam two-account test and human playfeel remain untested).
- Whether the host/join UI uses Steam invites or manual IP by default in this build.
- Whether two game instances can run on one PC with one Steam account (GodotSteam may
  refuse the second; LAN mode may avoid it).

## 2026-11-15 build and QA session (observations)

Built trism-prism's update (`lucid-blocks-coop-updated`, the branch behind PR #16) from
source against a copy of the 5.2.0.0 install, then ran the author's own LAN fixture with
two real game clients on one PC.

- The stack download for a matching Godot 4.6 **double** editor does not exist (only
  `win64` and `mono_win64` assets in `godotengine/godot-builds`). Importing the 102 mod
  assets with the official single-precision 4.6 editor was enough: the game loaded the
  generated `.scn`/`.ctex` files without parse errors, and the remote avatar rendered and
  animated in game. `.remap` files for scripts were generated as the author's builder does.
- Packing with GDRE 2.7.4 (`--pck-create`, v2 PCK, engine 4.6.0) produces a 52.7 MB
  `lucid-blocks-coop.pck`. SHA-256
  `2708AA8BE55DFA2B77053A5E07A7EF2AAFB067DF586F31142FDB1B64AD6DC770`.
- Installed by copying that one file to `mods\` next to the exe. Boot log shows
  `Mod: lucid-blocks-coop.pck`, `[lucid-blocks-coop] Ref override loaded`,
  `[lucid-blocks-coop] manager ready`, avatar bone mapping and 11 retargeted clips.
- Two-client LAN run (headless and windowed, real game exe, isolated `APPDATA`/`TEMP`):
  host completed 8 checks, guest 11, 0 failures, exit code 0 on both; the guest model,
  name tag and world rendered on screen. The fixture covers host/join transport,
  admission identity, guest block edit reaching the host's canonical region save,
  independent guest chunk bubble, independent dimension travel, host teleport following
  the guest across dimensions, and clean host shutdown.
- Post-exit `ERROR:` lines (Jolt/RID/resource leaks, "resources still in use") reproduce
  the Shutdown noise the author documented. They are not gameplay failures.
- The same PC running one Steam account + two game instances works over direct IP;
  Steam invite was not tested.

### Derived

- The mod is a one-file install and a one-file uninstall; no game files change.
- A single-precision editor is usable as an asset-import helper only because the runtime
  consumes already-imported binary resources; do not run the game's PCK through it.
- PR #16 is playable on 5.2.0.0 for direct-IP co-op. Steam invites and human playfeel are
  still unverified, and two players is the designed limit.

### Unknowns

- Whether a 4.6 double editor changes any imported animation data measurably; visual
  comparison against a double-built PCK was not possible.
- Behaviour across two physical machines/NAT, and with different mod file versions.

## Sources

- https://store.steampowered.com/app/3495730/Lucid_Blocks/
- https://github.com/parkers0405/lucid-blocks-coop
- https://github.com/parkers0405/lucid-blocks-coop/pull/16
- https://github.com/trism-prism/lucid-blocks-coop-updated
- https://github.com/sordsith/BubbleLoader
- https://github.com/Puzzku/Lucid-blocks-COOP-mod
