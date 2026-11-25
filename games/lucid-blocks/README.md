# Lucid Blocks (2026)

| | |
|---|---|
| Engine | Godot 4.6 **double**, GodotSteam 4.17.1; one `lucid-blocks.exe` with an embedded PCK v3 |
| Versions tested | Steam appid 3495730, build 26206996, `lucid-blocks.exe` 5.2.0.0, SHA-256 `CB6EA42754BA9075F4FC2141AEC39A13159DA7DDF835F13334D367E9FBC4A558`, Windows 10/11 x64 |
| Projects | [parkers0405/lucid-blocks-coop](https://github.com/parkers0405/lucid-blocks-coop) (original, 4.0.1) and [PR #16 by trism-prism](https://github.com/parkers0405/lucid-blocks-coop/pull/16) / [trism-prism/lucid-blocks-coop-updated](https://github.com/trism-prism/lucid-blocks-coop-updated) (5.2.0 update) |
| QA level | automated (author's two-client LAN fixture, 19 checks, run against the real 5.2.0.0 exe) |
| Last updated | 2026-11-15 |

## Where things stand

Two players can play Lucid Blocks 5.2.0.0 together over direct IP/LAN with trism-prism's
update of the co-op mod: both explore the same world, see each other as animated avatars,
place blocks that persist on the host, travel to dimensions independently, visit each
other's Apotheosis, and use a personal ender chest. Two players is the designed limit.
No release exists; the update is an open pull request, so the PCK has to be built from
source.

## Existing work

- [parkers0405/lucid-blocks-coop](https://github.com/parkers0405/lucid-blocks-coop)
  (25 stars): first co-op mod, five releases, last one 2026-07-05 for game 4.0.1. The
  4.0.1 save-format change broke older builds; open bugs include a 5.0.1 crash (#12),
  stutter (#11), client water not updating (#2), dead reconnect button (#3), client
  killing Meeshu (#5), client-specific capsule spawning (#9) and a Tiamana-leyline break
  (#8).
- [PR #16](https://github.com/parkers0405/lucid-blocks-coop/pull/16) (open, +5101/−1022,
  last commit 2026-11-12) and its fork `trism-prism/lucid-blocks-coop-updated`: retargets
  the mod at build 26206996 / 5.2.0.0, adds authenticated per-player identity in the host
  ledger, independent dimension travel, personal Apotheosis with visits, personal ender
  chest, acknowledged ownership handoffs, and a LAN test fixture. Its README says the
  inherited `dist/` artifacts are stale, no release was installed or published, and a
  real two-account Steam test plus human playfeel remain untested.
- `Syphon1205/lucid-blocks-coop`: July 2026 fork of parkers with sync fixes for the 4.0.x
  line.
- `Puzzku/Lucid-blocks-COOP-mod`: October 2026, binary-only `.pck` claiming 5.2.0 support,
  no source; not auditable, not used here.
- `sos-mislom/lucid-blocks-multiplayer`: May 2026 dedicated-server experiments.

## How the game works (for co-op)

- The install is one 1.57 GB `lucid-blocks.exe` with an embedded PCK v3 (7,522 files)
  plus three native DLLs. GDScript ships as `.gdc` bytecode; GDRE 2.7.4 decompiles it.
- Built-in mod loader (`main/autoload/mod_loader.gd`): scans `mods/*.pck` next to the
  exe, calls `ProjectSettings.load_resource_pack` with `replace_files=true` (so a mod can
  replace any `res://main/...` script or scene), then instantiates every
  `res://mods/<name>/mod_init.gd` it finds.
- The co-op mod replaces base scripts (`main/autoload/ref.gd`, world, player, save
  manager, item handling) through that PCK override plus a `coop_mod` autoload and a
  `chat_mod` command layer. Scripts are text `.gd` with a `.remap` beside each file so
  the engine loads them over the shipped bytecode.
- Transport: ENet over UDP, host-authoritative. Identity is an authenticated Steam
  transport ID or a host-issued LAN reconnect credential; profiles, chests and preserved
  world objects live in an atomic host ledger, terrain in separate verified region
  checkpoints. Guests keep their own loading bubble and lease their chunks from the host.
- Save format 5.2 writes per-region chunk files, so co-op adds sidecar files rather than
  rewriting the world dictionary; back up the whole save folder before playing.

## What we changed

Nothing in the mod logic: the only work here was producing a build for 5.2.0.0 and
verifying it, because PR #16 has no release and its `dist/` folder is stale.

- Built the mod from the fork's source without a matching Godot 4.6 **double** editor
  (none is published): the official single-precision 4.6 editor was used only to generate
  `.godot/imported` resources for the 102 `.import` files, since the fork gitignores that
  folder. Packing uses GDRE 2.7.4 (`--pck-create`, PCK v2, engine version 4.6.0), the same
  tool and parameters as the author's `build_mod.ps1`.
- Replaced the author's editor-based launcher with a runner that starts the real game exe
  twice, each with its own hardlinked exe directory, `mods/` folder, profile and temp
  directory, so the fixture exercises the shipping binary rather than an editor runtime.
- Resulting PCK: 52.7 MB, SHA-256
  `2708AA8BE55DFA2B77053A5E07A7EF2AAFB067DF586F31142FDB1B64AD6DC770`.

## QA

- Suite: the author's `tests/lan/local_coop.gd`, packed as a second mod
  (`res://mods/local_coop/mod_init.gd`) that drives both clients and writes JSON markers
  and final check counts.
- Run with the real game exe, isolated `APPDATA`/`TEMP` per client, random port, headless
  and windowed.
- Last result, 2026-11-15: host 8 checks, guest 11 checks, 0 failures, both exit code 0,
  in both headless and windowed runs. Covered: real ENet host/join, one admitted guest
  bound to its transport key, guest block edit present in the host's canonical region
  file, guest keeps an independent loading bubble and lease, guest changes dimension
  while the host stays, host teleport follows the guest across dimensions, clean host
  shutdown that commits the world.
- Windowed run: the guest avatar with name tag rendered in the host's view.
- Boot log on a clean install shows `Mod: lucid-blocks-coop.pck`,
  `[lucid-blocks-coop] Ref override loaded` and `[lucid-blocks-coop] manager ready`.
- Both runs print shutdown-only `ERROR:` leak lines (Jolt shapes, RIDs, resources in
  use), the same noise the author documents. No script, parse or gameplay errors.

## How to play

1. Build `lucid-blocks-coop.pck` from the fork's source (or use a shared copy from
   someone who already built it) and put it in `mods/` next to `lucid-blocks.exe`.
2. Both players need game 5.2.0.0 and the same PCK.
3. Host: enter a world, press F5 or type `/host`. Guest: type `/join <host-ip>` at the
   menu, over the same LAN or a VPN such as Tailscale; port forwarding also works.
4. Uninstall by deleting the PCK.

## Known issues

- Two players only; no dedicated server.
- Steam invite / real two-account Steam connection is untested — direct IP is the tested
  path.
- Human playfeel (animation feel, latency, long sessions) is unverified; only the
  automated fixture ran.
- The log ends with resource-leak warnings on shutdown.
- Back up the world folder first: co-op writes sidecar save files.
- The single-precision import workaround is a build detail, not a gameplay fix; a real
  4.6 double editor may regenerate identical resources, but that was not compared.

## Next steps

- Build with a Godot 4.6 double editor (compile one, or ask the PR author for the release
  they used) and compare imported resources and animation timing.
- Run the fixture and a play session across two physical machines and over a VPN.
- Test the Steam invite path with two accounts.
- Get PR #16 reviewed/merged, then attach a pinned PCK to a release so players do not have
  to build it.
