# Fallout Tactics: Brotherhood of Steel (2001)

| | |
|---|---|
| Engine | Proprietary (Micro Forte), 32-bit, DirectX 7; Lua scripting through [FTSE](https://github.com/melindil/FTSE) |
| Versions tested | GOG 1.27, `BOS.exe` SHA-256 `C5B91FE56EF8DE118CE22CCD5E6512F217CA16C8B8758EA14C626229EB5A4E40`, with FTSE 0.60a, on Windows 11 |
| Projects | [fallout-tactics-coop](https://github.com/thennak3/fallout-tactics-coop) (own mod, built from zero, MIT; beta, latest [v0.10.4-beta](https://github.com/thennak3/fallout-tactics-coop/releases/tag/v0.10.4-beta) of 2026-10-30) |
| QA level | automated (in-game bots drive 2 to 4 real game copies on one PC and compare every copy's state), plus sessions played by people on two PCs |
| Last updated | 2026-10-28 |

## Where things stand

Two to four players can play the campaign together with one shared squad: every player can order every
character, and the host's campaign is the one being played. Joiners copy the host's save and can drop in
mid-mission. Conversations, trading, recruiting, level-ups, looting, turn-based combat, the world map and random
encounters are synced, and a drift check puts the copies back together without reloading. It is in beta
(v0.10.0-beta): the bots cover each feature, and two people have played it across two PCs on a home network, which found and fixed
several bugs (0.9.1 to 0.9.4).

## Existing work

- **fallout-tactics-coop** (this project, since 2026-10-20): a DLL (`netlog.dll`) loaded by FTSE's Lua script,
  which hooks the game in memory (nothing on disk is changed). Source:
  [thennak3/fallout-tactics-coop](https://github.com/thennak3/fallout-tactics-coop).
- Earlier attempts, none with a download (found by web search, 2026-10-20; the forum pages refused fetching, so only
  search summaries were read): a TBC clan co-op mod with a "battlemaster" player running the AI (TTLG, about 2003),
  "The COOP Mod" on NMA (original multiplayer maps, said to be 40% done), NMA and GameSAS how-tos that copy campaign
  maps into multiplayer. Steam threads from 2014 and 2022 say no campaign co-op exists. Details in [NOTES.md](NOTES.md).

## How the game works (for co-op)

- **Multiplayer is lockstep.** Every order is a 27-byte command record sent to the host, which echoes it to every
  copy, and every copy executes it on the same frame (the Actor command executor, `0x5747e0`). The game only shipped
  squad-versus-squad multiplayer, so the mod runs the campaign inside a multiplayer session with one shared squad.
  The mod's own messages ride the same stream as command `0x30` records.
- **What lockstep doesn't cover**: single-player screens that change the game on one copy only (conversations,
  barter, the recruit and character screens). The mod snapshots the entity table, inventories, variables and
  attributes when such a screen opens and sends the difference as a delta when it closes, applied by every copy on
  the same frame. The entity table (World+0x743, 8-byte entries, free list at World+0x74f) is the hard part: the
  delta carries its bookkeeping so every copy hands out the same slots afterwards.
- **Saves** are copied over the mod's own TCP connection (port 47701, also used for discovery over UDP and for the
  joiners' polls). Map changes, mid-session joins and the rare full re-sync use a host quick-save that every copy
  loads.
- **Randomness** comes from a per-frame seeded LCG in the World, identical on every copy. FTSE's Lua hooks are the
  exception: its `OnBurstAttack` rolled with `math.random` seeded from the clock, which split the copies in every
  fight with NPCs until replaced with a deterministic generator.
- Tools: Ghidra through REA, a crash-dump reader for WER minidumps, and the in-game bots described below. Addresses
  and how each was found are in [NOTES.md](NOTES.md).

## What we changed

The full history is in [NOTES.md](NOTES.md); the main fixes, each with the test that showed it:

- **NPC bursts landed differently on each copy**: FTSE's Lua `OnBurstAttack` used a clock-seeded RNG; replaced with a
  deterministic Park-Miller generator. Chaos runs (every copy acting at random) stopped drifting (3 players, 38/38).
- **Shops pause nobody**: the merchant's stock is made on every copy at the same frame, and the deal goes out as a
  delta. While a shop is open, other players' item moves (give, take, drop, equip, unequip, use) wait and run
  together when it closes, because the shop makes and destroys entities on the shopper's copy only
  (`trade_host --inv-chaos` failed twice, 48/48 after; `trade_join --inv-chaos` 49/49).
- **Looting**: only the looter's copy removed dead entries from a container's item list, so the copies drifted
  forever. Every copy now prunes them after a take (`loot`, 24/24).
- **Turn-based combat**: whose turn it is and End Turn work for every player; the host's game settings
  (turn-based mode, difficulty, friendly fire...) apply on every copy (`turnbased` with 4 players, 69/69;
  `tbauto` 29/29).
- **Drift check**: every 25 frames each copy hashes positions, HP, the entity table, inventories, equipment and
  attributes; on a mismatch every copy holds one frame and takes the host's values. It had stopped comparing after
  its first correction when a load restarted the frame count; now per loaded save (`drift_equip`, 25/25).
- **The entity table growing during a private screen** used to force a save and reload; the delta now says so and
  every copy grows its table the game's way (`talk_host --grow-table` 30/30, `talk_join --grow-table` 31/31).
- **Crash leaving a session to the main menu**: the Load Game screen function (`0x4ca7b0`) pops one stack argument
  (`ret 4`) and was called without one; found from a WER minidump (`menu_join --menu-cancel`: the old call exits
  0xC0000409, fixed 36/36).
- **World map**: only the host moves the party; the others see it travel and can ping a spot in their colour
  (`worldmap`, 33/33). Only the host's copy rolls random encounters, the others follow it (`encounter`, 26/26).
- **Joining**: fixed 1-2 s polls cut to 250 ms (drop-in join 7.9 s to 3.5 s), and the Multiplayer screen's
  DirectPlay object is left alive instead of released: its release blocked 3 s, and the next DirectPlay object waited
  for it even on another thread (Join to playing 3.6 s to 1.2 s, v0.10.1-beta).
- **Two PCs**: Windows' WSAECONNRESET after an ICMP port unreachable stopped discovery; turned off with
  SIO_UDP_CONNRESET (two PCs).

## QA

- `python -I test/run_test.py <test>` in the mod repo starts 2 to 4 test copies of the game on one PC, each with an
  in-game bot (`src/bot_scenarios.inc`), and checks their logs. Every test compares, at every shared snapshot
  frame: every actor's position and HP, every inventory with hands and armour, the entity table's bookkeeping, quest
  variables, attributes, the squad and the merchant's stock. Each scenario adds its own checks (the trade went
  through, the turn passed, no reload happened, and so on). Chaos runs fail on any drift report.
- Results for 0.9.3 and 0.9.4, the code in v0.10.0-beta (targeted runs, 2026-10-28): `worldmap` 33/33, `trade_host --inv-chaos` 48/48,
  `trade_join --inv-chaos` 49/49, `drift_equip` 25/25, `talk_host --grow-table` 30/30, `talk_join --grow-table`
  31/31, `menu_join` 34/34, `menu_join --menu-cancel` 36/36, `dropin` 37/37, `resync` 25/25, `encounter` 26/26,
  `recruitnew_host` 29/29, `chaos` (containers.sav, with equipping) 22/22. The full suite (`test/run_suite.py`)
  was not rerun for these releases.
- People: two players on two PCs on one home network, on 2026-10-27 (the bugs found there are fixed in 0.9.1 to
  0.9.3).

## How to play

Every player needs Fallout Tactics from GOG (1.27; check the hash above), [FTSE 0.60a](https://github.com/melindil/FTSE/releases/tag/0.60a),
and the mod zip from the [releases page](https://github.com/thennak3/fallout-tactics-coop/releases), extracted next
to `BOS.exe`. The host picks Multiplayer > Start Co-op and loads a save; friends pick the host's `[Co-op]` session
in the Multiplayer list and press Join. Players must reach the host's PC: a home network, or a virtual LAN such as
Tailscale or ZeroTier. Full steps in the mod's README.

## Known issues

- While a player talks to anyone other than a merchant, or uses the recruiter, the other players' games pause until
  the conversation closes.
- A character who joins the squad through a story conversation (not the recruiter) still means a save and reload
  for everyone.
- Not yet played over the internet.

## Next steps

- Corpse looting, a joiner loading its own save, forced speech from scripts, gambling, traders other than the
  quartermasters, and the multi-recruit screen have no bot test yet.
- The entity table grew from 2000 to 16000 slots in one evening of play; it only grows when the free list runs out,
  so free slots may be lost somewhere.
- A character joining through a story conversation: the switched-off path that recreates it on every copy changed
  one NPC's AI state on one copy.
