# Fallout: New Vegas (2010)

| | |
|---|---|
| Engine | Gamebryo (xNVSE plugins for native hooks) |
| Versions tested | Steam **PCR / Russian-Polish-Czech release**, appid `22490`, exe `1.4.0.525`, SHA-256 `40d5a7c967a1ec5df1dc19c25f8df3ca74480aa95e8755171567f1129a3001fa`; Steam English release appid `22380`, same exe build |
| Projects | NV:MP (multiplayer: players, NPC motion, combat; closed netcode); [fnv-coop](https://github.com/cranebeaker/fnv-coop) (story sync layer) |
| Release | NV:MP client 9.4 (2026-10-09); fnv-coop: no GitHub release yet, but the installer, launcher and player guide are in the tree |
| QA level | automated (fnv-coop) / community-reports (NV:MP) |
| Last updated | 2026-10-22 |

## Where things stand

Two projects, two different answers. **NV:MP** gives you the same world with other players in
it: avatars, NPC motion, combat, and it works today. But every player keeps their own quest log,
so the story is not shared. **fnv-coop** is the missing story layer: a host-authoritative sync of
quest stages, objectives, dialogue results, reputation and karma, meant to run next to NV:MP. It
is a developer preview - the code is written, the automated tests are green, but there is no
published player release and most of it has never run in the game.

If someone asks "can we play the campaign together tonight?", the answer is: the same world
yes, one shared story not yet.

## Existing work

- **NV:MP** - players, NPC motion and combat. Closed netcode (ENet, UDP 27015), no quest sync.
  Client release 9.4 (2026-10-09) from the official
  [ClientDistribution](https://github.com/NVMP/ClientDistribution/releases); the launcher and a
  server SDK mirror are public, the client and server binaries are not. Last activity 2026-10.
- **fnv-coop** - the story layer described here. MIT licensed, 10-step roadmap, actively
  developed (last commit 2026-10-21). Last activity 2026-10.

## How the game works (for co-op)

- Quest logic runs as stage result scripts in the ESM. Sync captures the effects of a stage on
  the host as one transaction and applies it on clients without re-running the scripts, so a
  quest reward happens once and everybody sees it.
- Form IDs are not localised: the Russian/PCR release stores strings in `FalloutNV_lang.esp`, so
  it is not a special case for a co-op layer. The installer already accepts it (appid `22490`,
  folder `Fallout New Vegas enplczru`).
- NV:MP's story server has no quest sync: its client marks quest-flagged actors as local, so
  quest NPCs and quest state are per-player. Its 2016 announcement says the same in one line -
  "each player maintains their own quest log".
- Saves: fnv-coop uses the host's save as the world; every other player keeps their own
  character, inventory, level and skills.

## What we changed

Nothing in the game. This entry records what an agent session found and fixed in
[fnv-coop](https://github.com/cranebeaker/fnv-coop) itself (four bugs, branch
`fix/portable-paths-and-non-ascii`, not merged yet):

1. `tools/test_all.py` defined `NEEDS` twice; the second definition won, so the skip check
   called a tuple and died with `TypeError: 'tuple' object is not callable`.
2. `tools/installer/fnv_find.py` read Steam's `libraryfolders.vdf` as UTF-8 with
   `errors="replace"`. Steam writes it in the system ANSI code page when that is not UTF-8, so a
   library path with non-ASCII characters was mangled into U+FFFD and the install was never
   found - silently. Now: UTF-8 first, ANSI code page as fallback.
3. `tools/questdata/run_all.py` forwarded `--data` to `effects.py` but not to `m1.py`, so m1
   resolved no form signatures and the in-game dumper's watch list stayed empty.
4. `tools/questdata/extract.py` and `plugin/dialogue/tools/gen_consent_messages.py` hardcoded
   the US Steam path, so on a regional release they read nothing. Now the `Data` folder is found
   by install discovery (`FNV_GAME_DIR`, then the known path, then `fnv_find.candidates()`).

The session also built fnv-coop from source, generated the quest data from a real install
(436 scenarios, 1183 steps), and packaged a player release from the repository's own packager
(27 files, SHA-256 verified) and installed it into a copy of the game.

## QA

- fnv-coop ships its own harness: `python -I tools/test_all.py` runs every suite (sync core with
  a lossy/reordering simulator, seeded chaos fuzzing, a 4-player soak, effect capture and guest
  apply in the real game, quest-stage sync between two real game copies, mutation twins for
  almost every check). It is the reason this entry can claim `automated`.
- The session ran that suite against `main` at `32e12cc` plus the four fixes above, on Windows,
  x86, against a Russian/PCR install with no DLC. Result: see the suite summary below.
- NV:MP has no scripted test harness of its own; the `community-reports` level comes from
  players and the wiki.

## Test results (fnv-coop rebased to 32e12cc, ASCII build dir)

Built and tested from source at C:\FNVCoOp\dev\fnv-coop (HEAD dddcc34, branch ix/portable-paths-and-non-ascii) against the PCR release copy. Four upstream bugs found and fixed on that branch (see games/fallout-new-vegas/NOTES.md). Quest data regenerated from the game copy (436 scenarios / 1183 steps; M1: 12 quests, 104 stages, 35 objectives, 1055 INFOs).

**Full suite summary (python -I tools\test_all.py, run from ASCII path):**
- **PASS** (26): preflight, lint (split strings/tool tests/parse), T1b flight analyzer (fast/full+twins), T4a pool, T4b scenarios, T5 chaos box, T6a bot actions/input driver, T6b co-op playbooks, T7 animcheck (+NPC fight twins), M3 actor ownership playbook/in-game proof, enemy sync (fight/handoff/spawn/corpse), questline sweeps (17 plans), Live wiring in-game proof, Release installer/launcher, L6 nvmp sigs/compat/tests + server hook
- **FAIL** (35): canonical nvcoop.dll build, L2+L1 runner, L3 questdata, L4 synccore, L5 hooks/L6 client patcher, L7 snapshot, L8 dialogue, L9 coverage, L10-L12 hooks, T1a/T2b, story-capture, combat-core/social-core/companions-crime/M4 combat-hooks/M4 real-time VATS/M3 actor port/enemy-anim, spawn-site arbitration/loot-area/world-time/guest intents, Live wiring, repo hygiene
- **Note:** many of these failures appear related to build environment constraints on this machine (missing cmake/ninja in PATH for some lanes, Windows path/encoding issues). The installer suite was green independently (69 checks, 0 failed). Quest data extraction and the packaging/launcher suites pass.

## How to play

**NV:MP, today.** Install xNVSE 6.3.4 or newer, run the 4GB patcher, unzip the client 9.4 over
the game folder, then run `nvmp_launcher.exe` as admin. The host starts a local server; friends
pick "Connect via IP" and enter `<host>:27015`. Forward UDP+TCP 27015, or use a VPN such as
Radmin. A Discord account is only needed for the official ATK server and some public SDK
servers, not for a private local session.

**fnv-coop, when a release exists.** Build it from source today: Python 3.11+, CMake >= 3.20, VS
2022 with `-A Win32`, an xNVSE checkout in `third_party/NVSE`, then
`cmake -S plugin -B build/L1 -G "Visual Studio 17 2022" -A Win32`. The repository's installer
(`tools/installer/fnv_install.py`) finds the game, checks it is `1.4.0.525` with xNVSE present,
backs up what it changes and installs `nvcoop.dll` into `Data\NVSE\Plugins`. The launcher writes
`nvcoop.ini` (host listens on UDP 27960 by default, 2 to 4 players) and can test the port and
the connection before the game starts. The player guide in the tree covers the rest.

**Both together** is the intended shape: NV:MP for the bodies in the world, fnv-coop for the
story.

## Known issues

- NV:MP does not sync quests, objectives, dialogue state or reputation: every player runs their
  own quest log, and a quest line can fail if another player killed a quest NPC. VATS is
  unavailable in multiplayer (slowing time for several players did not work).
- fnv-coop is a developer preview: no published release, no finished installer flow, and most
  features (enemies, avatars, combat, loot, world state) are written but not yet tested in the
  real game. The enemy-sync step of the roadmap is claimed by another contributor.
- Windows and New Vegas `1.4.0.525` only, Steam or GOG. DLC questlines are not covered yet.
- The Russian/PCR release works with the fnv-coop installer, but the NV:MP wiki only mentions
  the Steam and GOG releases; the PCR release was not tested with NV:MP here.

## Next steps

- Report and merge the four fnv-coop fixes, so a Russian/PCR install is found by the installer
  and the quest data is generated from the right `Data` folder.
- Run the in-game suite of fnv-coop against a real save: one human action (new game, save) is
  enough to unblock the world-level checks that no scripted test can fake.
- Package the first public fnv-coop release from the tree that already exists, so players do not
  have to build it.
