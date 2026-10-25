# Story co-op for Mafia (2002): options and build plan

Draft, 2026-10-23. Target: the Steam build (app 40990, build 2300614, `Game.exe` SHA-256
`303eb95e…f298`). Evidence is in [NOTES.md](NOTES.md). Time estimates come from the playbook's
table for a "closed native engine, no loader" (`playbook/how-coop-mods-work.md` §4). They are
estimates, not measurements.

## What "story co-op" means here

You and one to three friends play the campaign missions together. The host plays Tommy and owns
the story: mission scripts, triggers, cutscenes, objectives and saves. Each partner is an extra
gunman and driver in the same mission. Everyone sees the same enemies, cars, doors and cutscenes.

This "leader-centric" design is the realistic first goal. Mission scripts check "the player"
(Tommy), so letting a partner set off triggers on Tommy's behalf is a later step (playbook
`effect-capture-story-sync.md` §8, "intents").

## The options

| | A. Extend LHMP | B. Add story co-op to Mafia Online | C. New mod from zero | D. New mod, LHMP as donor (recommended if B is refused) |
|---|---|---|---|---|
| Base | [LHMPTeam/lhmp-old](https://github.com/LHMPTeam/lhmp-old), 2013-2015, archived | [MafiaHub/MafiaOnline](https://github.com/MafiaHub/MafiaOnline), active Sept 2026 | Nothing | Our own repo, reusing LHMP code and struct layouts where they help |
| License | Apache-2.0: we may fork and ship | MafiaHub OSS license: changes only as contributions to them; personal use needs their written permission | Ours (Apache-2.0 suggested) | Apache-2.0 (keeps LHMP code usable) |
| Matches your `Game.exe` | **No.** Bytes at its hook sites differ, and its player global `0x6F9464` lies outside your exe's image (ends `0x6DAD76`). About 326 fixed addresses to re-find. | **Yes**, exact hash, with audited hooks | We find every address ourselves | We re-find the addresses LHMP used, by signature, and keep LHMP's object offsets as hints |
| Already done | Player and car puppets, weapons, shots, grenades, deaths, doors, pickups, car explosions, chat, Squirrel server | Player and car replicas, server-chosen mission loading for all clients, chat, save-write blocking, two-client Wine smoke test, lag/loss proxy | Nothing | Whatever ports cleanly from LHMP |
| Story support | None (it turns off traffic and pedestrians) | None yet (it suppresses mission actors) | Designed in from day one | Designed in from day one |
| Toolchain | Visual Studio 2013 solution, inline asm, RakNet, DirectX 8.1 SDK, Qt | MafiaHub Framework, CMake, CEF, V8, Node; developed on Linux/Wine | Ours: MSVC x86, CMake, MinHook, ENet or GameNetworkingSockets | Same as C |
| Main risk | Porting 326 addresses to old code that its authors call a mess, then building story co-op on top | MafiaHub says no, or wants a different design | Most reverse engineering of all | Same as C, slightly less |
| Time to friends visible in game | weeks (port) | days, if MafiaHub agrees | weeks | weeks |
| Time to one story mission together | months | weeks to months | months | months |
| Full campaign | likely months+ | months | months to "out of scope" per playbook | months to "out of scope" per playbook |

Not viable: **engine rewrites** (OpenMF, GPL-3.0; MafiaUnity, Apache-2.0). Both are archived and
cannot run missions, so the whole game logic would need rewriting. Their file-format readers are
still useful (below). **LHMP on its own exe:** it would need either a downgrade patch, which is not
an official download (AGENTS.md rule 8), or everyone owning a retail disc copy of the version
LHMP expects. That version is still unknown. **Mafia: Oakwood** (found later, 2026-10-24): its client
source is public and its README targets English 1.2, Steam and GOG, but its license (clause f) forbids
derivative works. Use it for background reading at most.

**Decision (2026-10-23):** the owner chose **option D** and did not contact MafiaHub. The
recommendation below is kept for the record.

### Recommendation

1. **Ask MafiaHub first** (you send it; I draft the message). Mafia Online already does the hard,
   address-specific half on your exact exe. If they want story co-op upstream, option B is by far
   the fastest, and it stays legal.
2. **If they say no or don't answer within a week or two, take option D.** It keeps a license we
   can ship with friends, reuses LHMP's knowledge, and is designed around story sync. Do **not**
   copy Mafia Online's code or its fixed addresses into D: its license forbids it. Reading their
   public docs for background is fine, but D should derive its own addresses from our own Ghidra
   analysis.

The rest of this plan is for D. Most milestones also apply to B, inside MafiaHub's codebase.

## Story-sync design (D), following the playbook

- **Authority:** the host runs every mission script. Client copies load the same mission with
  their scripts **gated off** (hook the script interpreter's start). See
  `effect-capture-story-sync.md` §7.
- **Actors:** the host owns all mission NPCs and traffic and streams their pose, animation,
  health and death. Clients show puppets. Each player owns their own avatar and the car they
  drive. Ownership epochs as in `actor-ownership.md`.
- **Effect capture:** hook the Mafia Script command dispatcher on the host. Commands that change
  world state other than actors are recorded as effects, sent as numbered transactions with acks
  and applied on clients. Examples: doors, object enable/disable, objectives and HUD text,
  cutscene start, weather and lights, mission end. Commands that only steer AI are not replayed,
  because the actor stream already carries their result.
- **Cutscenes:** "play cutscene X" is an effect. Clients play it locally, with a barrier at the
  end so nobody resumes early.
- **Missions and saves:** the host picks the mission and every client loads it (barrier). Only
  the host's profile saves. Clients block save writes, a technique Mafia Online documents too.
- **Partner rules (proposed, your call):** if Tommy dies, the mission fails as in single player.
  If a partner dies, they respawn near the host after a few seconds. Mission rewards and story
  stay with the host's profile.
- **Network:** ENet or GameNetworkingSockets on loopback for tests. For friends, Steam lobbies and
  relay through AppID 480 "Spacewar" (`networking-steam-spacewar.md`), so nobody types an IP or
  forwards ports.

## Milestones (each ends in a bot-driven test, AGENTS.md rule 11)

| # | Goal | Exit test |
|---|---|---|
| M0 | Workspace: project folder with a short path, fork and clone of coop-anything, claim issue, isolated test copy of the game. Find where Mafia writes saves and settings (its registry key is shimmed from HKLM to HKCU by `mafia.sdb`) and hash them before the first launch. | Test copy launches. The owner's Steam install, saves and registry key are byte-identical after the run. |
| M1 | Static recon: Ghidra headless decompile of `Game.exe` and `LS3DF.dll`; a Python DTA reader (MafiaUnity's Apache-2.0 `DTALoader.cs` as reference); dump every story mission's `scene2.bin` scripts to text; count which script commands each mission uses (the draft effect vocabulary). | Script dump of all story missions plus a command-frequency table in NOTES. |
| M2 | Hook point: a `winmm.dll` proxy in the test copy loads our DLL (`Game.exe` imports `WINMM.dll`, which is not a KnownDLL). Per-frame tick, local player position, windowed mode, intro skip, second-instance checks removed. | Two copies run side by side, and each logs its player position every second. |
| M3 | Puppets: host and one client over loopback; a remote human spawned from the game's own human code, driven at 20 Hz with interpolation. | A bot walks a circle on copy A; copy B's puppet stays within tolerance. Repeat through the lag/loss relay in `tools/`. |
| M4 | Mission sync: the host selects a mission, clients load it, client scripts are gated off, and host-owned actors are streamed. | In mission 1, both copies show the same NPCs and cars at a checkpoint (state hash compared). |
| M5 | Effect capture plus story log: transactions, acks, resend, cutscenes with barriers, objectives. | A host-side bot advances mission 1 to its end; the client's state hash matches at every checkpoint; the test fails when capture is switched off (mutation twin). |
| M6 | Combat: partner hits sent as claims, host validates, enemy death once; partner respawn. | Both clients damage one enemy; same health on both; one death. |
| M7 | Vehicles in missions: driver owns the car, passengers seated, chases. | Scripted drive with the partner as passenger; positions match within tolerance. |
| M8 | Campaign soak: run every story mission through M4 to M7 with bots, forcing progress through the mission's own triggers. | Suite summary pasted in the write-up; `qa: automated` only after it passes. |
| M9 | Ship: Spacewar relay, one-click host and join, README with known issues, directory and write-up PR. | A fresh test copy installs and joins through the packaged launcher. |

First thing you and your friends could actually do together: walk and drive around after M3
(weeks). First story mission together: M5 (months). These are playbook estimates, not
measurements.

## What I need you to install

Already on your PC, checked: Visual Studio Build Tools 2026 (x86 MSVC 14.51, bundled CMake),
Windows SDK 10.0.26100, Python 3.14, Node 24, Git. No Docker, Wine or IDA needed.

| What | Why | Size | Source (official, verify by SHA-256) | When |
|---|---|---|---|---|
| JDK 21 (e.g. Eclipse Temurin) | Ghidra needs Java 21; you only have Java 8 | about 190 MB | adoptium.net | M1 |
| Ghidra (unzipped into the project, no system install) | Decompile `Game.exe` and `LS3DF.dll` | about 500 MB zip | github.com/NationalSecurityAgency/ghidra/releases | M1 |
| x64dbg (optional) | Live debugging of the 32-bit game | about 50 MB | github.com/x64dbg/x64dbg/releases | M2 onward |
| GitHub CLI `gh` (or you do the GitHub steps in the browser) | Fork, claim issue, draft PRs | about 15 MB | cli.github.com | M0 |
| Small source libraries (MinHook, ENet or GameNetworkingSockets), vendored into our repo at pinned commits | Hooks and networking | under 20 MB | their GitHub repos | M2 and M3 |
| 32-bit `steam_api.dll` from Steamworks.NET Standalone 2025.164.1 (SHA-256 `fdafb7c6…65da`, per the playbook) | Steam lobbies and relay for friends | about 300 KB | github.com/rlabrecque/Steamworks.NET/releases | M9 |

## What I need you to approve or decide

1. **A project folder with a short path outside this session's folder**, e.g. `C:\coop\mafia`.
   `git clone` fails here because the path is too long, and AGENTS.md has me ask before writing
   outside the workspace.
2. **A test copy of the game** (about 2.8 GB) inside that folder. Your Steam install is never
   touched.
3. **Contacting MafiaHub**: you send the message, I draft it. Their answer decides B or D.
4. **GitHub under your account**: fork coop-anything, open a "claim a game" issue for Mafia, later
   a separate repo for the mod and draft PRs. I show you every diff before anything is published.
5. **License for the new mod** (D): Apache-2.0 suggested, to match the LHMP code we would reuse.
6. **Partner rules** above (Tommy's death fails the mission, partners respawn): keep or change.
7. **Which copies your friends own.** This plan targets the Steam build only. A GOG or disc
   `Game.exe` has different addresses and doubles the hook work.
8. **One UAC prompt later (M3):** Windows Firewall allow rules for the test exes, limited to
   127.0.0.1.

Also worth knowing: the scope is large. The playbook marks a full campaign on a closed native
engine as possibly out of scope. Its advice is to start, ship each milestone, and record progress
in the directory so the work is never lost (AGENTS.md "Handing off an unfinished mod").
