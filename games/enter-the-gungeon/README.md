# Enter the Gungeon (2016)

| | |
|---|---|
| Engine | Unity 2017.4.27f1 (Mono, x64) |
| Versions tested | Steam, buildid 3829397 (game version 2.1.9, updated 2025-06-27) |
| Projects | [Gungeon Online](https://thunderstore.io/c/enter-the-gungeon/p/GoTeamAlex/Gungeon_Online/) (directory entry), [GungeonTogether 3-Player](https://thunderstore.io/c/enter-the-gungeon/p/Rec/GungeonTogether_3Player/) (directory entry), [GungeonTogether](https://github.com/llamerrr/GungeonTogether), [GungeonTogether LAN](https://thunderstore.io/c/enter-the-gungeon/p/mamonts/GungeonTogether_LAN/), [Jolly_Coop](https://github.com/kleirof/Jolly_Coop) |
| QA level | automated |
| Last updated | 2026-10-18 |

## Where things stand

You can play the main run online in two-player co-op today with **Gungeon Online 0.101.0**.
It ships a Steam lobby invite path and a LAN path (UDP 27460). Two real game clients
were driven through a full connection and into a shared run: they connected over LAN,
picked different characters, generated the *same* procedurally-built floor and kept it in
sync. Enemy *attack simulation* is not synced (each client simulates combat itself), and
items dropped when a player dies are not synced, so the game is playable, not a perfect
mirror.

For **three or more players** the only published option is the experimental
**GungeonTogether 3-Player 0.10.8** (Steam-only). It is a different fork of the
GungeonTogether line and is early: on load it installs progress-sync patches and initialises
a Steam P2P + lobby layer. It has *not* been play-tested with three clients here (that needs
three Steam accounts/PCs) — only a boot test was run (see QA). Use Gungeon Online for
reliable two-player runs and treat 3-Player as a work in progress.

## Existing work

| Project | Where | License | Last activity | What it does |
|---|---|---|---|---|
| **Gungeon Online** 0.101.0 | [Thunderstore](https://thunderstore.io/c/enter-the-gungeon/p/GoTeamAlex/Gungeon_Online/) | MIT | 2026-10 | Steam lobby invites + LAN UDP 27460; shared-seed run, player/anim/gun/pet/status, enemy HP/death, bullets/throws/blanks, breakables, ghost+revive, shared pause, optional PvP. **Tested here: works.** |
| **GungeonTogether 3-Player** 0.10.8 | [Thunderstore](https://thunderstore.io/c/enter-the-gungeon/p/Rec/GungeonTogether_3Player/) | MIT | 2026-10 | Steam lobby + P2P, goal ≥3 players; progress-sync patches + Steam P2P/lobby layer (needs ModTheGungeon API 1.9.2). Early build, marked "AI Generated". **Tested here: boots.** |
| **GungeonTogether** | [GitHub](https://github.com/llamerrr/GungeonTogether) | MIT | 2026-09 | Steam lobby + P2P, player position sync; enemies/dungeon planned. **No published build** (issues asking for the DLL). |
| **GungeonTogether LAN** 0.3.6 | [Thunderstore](https://thunderstore.io/c/enter-the-gungeon/p/mamonts/GungeonTogether_LAN/) | unknown | 0.3.6 | 2p LAN/TCP 27631. Reported blocker: 4th floor Sell Creep map mismatch stops runs. |
| **Jolly_Coop** 1.3.7 | [GitHub](https://github.com/kleirof/Jolly_Coop) | none stated | 2026-01 | Plugin boots; local co-op quality-of-life (player outlines, depends on ModTheGungeon API + Gunfig). No network code seen. |

## How the game works (for co-op)

- Unity 2017.4.27f1, C# managed code in `EtG_Data/Managed/Assembly-CSharp.dll`; mods load
  with BepInEx 5.4 (HarmonyX) or the older ModTheGungeon. This mod uses **BepInExPack_EtG 5.4.2101**.
- The base game has **local couch co-op only** (no online), so co-op mods have to transport
  the run state themselves; Gungeon Online uses a custom UDP transport (LAN) and Steam lobbies.
- Net model: **host authority for world/run state** (shared run seed, floor generation,
  level sync, save sync), with per-client enemy simulation. Protocol version 32; a
  `version mismatch` disconnect means clients run different mod versions.
- Save system: `%USERPROFILE%\AppData\LocalLow\Dodge Roll\Enter the Gungeon\SlotA.save`
  (this `LocalLow` path is **shared by every copy of the game on the machine**, so isolate
  it before any two-client test). The mod adds save sync.

## What we changed

No code changes. The mod was verified as-is; this write-up and the directory entry are the
deliverable. QA was automated using a **test harness the mod already contains** (found by
decompiling `GungeonOnline.dll`), so no bots or input scripting were needed:

- `--gungeon-online-name <name>`
- `--gungeon-online-test host|join` (hosts / joins LAN 127.0.0.1:27460 at the title screen)
- `--gungeon-online-test-run <CharacterPrefabSuffix>` (picks a character, host starts the run)

## QA

Two isolated copies of the game (never the owner's install) with BepInEx + Gungeon Online,
each given a Steamworks `steam_api.dll` + `steam_appid.txt=480` (Spacewar) so nothing touches
the owner's save or Steam Cloud. The real `LocalLow` save folder was renamed away for the
duration and restored afterwards.

Run 1, connection only:

```
host : EtG.exe --gungeon-online-name Hosty   --gungeon-online-test host
client: EtG.exe --gungeon-online-name Clienty --gungeon-online-test join
```

Run 2, full run sync:

```
host : EtG.exe --gungeon-online-name Hosty   --gungeon-online-test host --gungeon-online-test-run Marine
client: EtG.exe --gungeon-online-name Clienty --gungeon-online-test join --gungeon-online-test-run Guide
```

Last suite result (2026-10-18, PASS):

- Host: `Hosting LAN game on port 27460` then `Connected to Clienty over LAN!`
- Client: `Joining LAN game at 127.0.0.1:27460` then `Connected to Hosty over LAN!`
- Both: `Test run: picking PlayerMarine` / `PlayerGuide`; host `Test run: host starting the run.`
- Same generated floor on both: host `Floor fingerprint: 20 rooms, 2A7BE448 (LoadingDungeon|Base_Castle(Clone)|542264008)`; client `Floor matches the host's (2A7BE448).`
- Also observed synced: partner map marker, partner target body, pet (`Showing partner's pet Dog`), pause, save progress.
- ~4 minutes with no `timed out`, `Disconnected`, `Exception` or `mismatch` in either log.

Run 3, GungeonTogether 3-Player boot test (2026-10-18, PASS on load only):

A third isolated copy (`BepInEx 5.4.2101` + `ModTheGungeon API 1.9.2` +
`GungeonTogether.dll` 0.10.8) was launched the same way. Because the game is **x64** it
loads the shipped `steam_api64.dll`; the earlier boot attempt failed only because that file
had been swapped for a newer redistributable (the game's `CSteamworks.dll` then could not
find its exports). With the stock DLL kept and `steam_appid.txt=480`, the run gave:

- `Loading [Gungeon Together 0.10.8]` → `Gunseon Together starting...` → `[Progress] Installed 8 progress query patches.` → `Gunseon Together ready.`
- Steam initialised (`Starting Steam platform interface`, `Received stats and achievements from Steam`), and the mod's networking came up: found `SteamNetworking`/`SteamMatchmaking`/`SteamApps`, `SendP2PPacket`/`ReadP2PPacket`/`IsP2PPacketAvailable` all `Found`, and created `LobbyCreated`/`LobbyEnter`/`GameRichPresenceJoinRequested`/`LobbyMatchList` callbacks.
- Game reached the main menu; no exceptions from the mod (only harmless HarmonyX "patch the declared method" warnings and stock vanilla audio/XInput noise).

This is a **load/boot test, not a play test**: a real 3-client session needs three Steam
accounts/PCs, so gameplay sync, stability and the 3-player cap are unverified here.

## How to play

1. Install **BepInExPack_EtG 5.4.2101** into the game folder (Thunderstore), then drop
   **GungeonOnline.dll** into `BepInEx/plugins/`.
2. Everyone launches through Steam and starts the same mod versions.
3. Steam path: in game press **F8**, click **Host on Steam**, then **Invite friend (Steam overlay)**.
4. LAN path: host **F8** > **Host on LAN (port 27460)**; the other player **F8** > types the
   host's address > **Join** (or **Find LAN games**).

For three or more players, instead install **BepInExPack_EtG 5.4.2101** +
**ModTheGungeon API 1.9.2** + **GungeonTogether.dll** 0.10.8 (no GungeonOnline.dll), launch
through Steam, open the mod menu (**Ctrl+P**) and use **HOST LOBBY** / **JOIN LOBBY**.

## Known issues

- Enemy attack timing is not synced (each side simulates its own); expect small desyncs.
- Items dropped from a player on death are not synced.
- Gungeon Online networked co-op is two players; GungeonTogether 3-Player adds 3+ but is
  Steam-only and experimental (early; parts of the run not yet synced).
- Mixed mod versions disconnect with `version mismatch`.
- GungeonTogether 3-Player is not compatible with Gungeon Online: install one or the other
  (`BepInEx/plugins/GungeonOnline.dll` must be removed to use the 3-player mod).

## Next steps

- Verify the Steam-lobby path across two PCs / two Steam accounts (only LAN was tested here).
- Play-test GungeonTogether 3-Player with three real clients (needs three Steam accounts/PCs):
  confirm the 3-player cap, join flow and run sync, and log what desyncs.
- Longer stress run: bosses and floor 4 in particular.
- Open question: whether Gungeon Online plays well with other mods present on only one side.
