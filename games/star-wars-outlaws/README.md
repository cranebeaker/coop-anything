# Star Wars Outlaws (2024)

| | |
|---|---|
| Engine | Snowdrop (Massive Entertainment); closed native x64 exe with Denuvo Anti-Tamper |
| Versions tested | Ubisoft Connect, product 17903, `Outlaws.exe` SHA-256 `371be3e5657b6f46f8c9e2d331696bde754df16bfc23f7d5dccacd9fe19b1d2f`, Windows 11 |
| Projects | none (gap entry in `data/directory.yaml`) |
| QA level | none |
| Last updated | 2026-11-01 |

## Where things stand

Nobody can play Star Wars Outlaws together today. The game is single-player, but the exe contains an unreleased
co-op heist mode for up to 4 players, its settings data ships and loads at startup, and Snowdrop's session and
UDP layer is linked in. Reviving that mode looks more promising than syncing the open-world campaign from zero,
but no session has been started yet.

## Existing work

Searched 2026-11-01 (web in several phrasings, Nexus Mods, GitHub topics, this hub's issues): no co-op or
multiplayer project. Single-player tooling only:

- Universal Snowdrop Modloader: <https://nexusmods.com/starwarsoutlaws/mods/24>
- Ultimate-SWO-ModLoader (ASI loader, archived 2024-12-31): <https://github.com/patrickmollohan/Ultimate-SWO-ModLoader>
- SnowplowCLI (Snowdrop data extractor, archived 2024-07-23, no project license, Mario + Rabbids TOC formats only):
  <https://github.com/CosmicDreamsOfCode/SnowplowCLI>

Not searched: Discord servers, Reddit directly, non-English forums.

## How the game works (for co-op)

**Binary.** `.code` (89 MB) is plain compiler output despite Denuvo; Denuvo's VM sits in an executable `.debug$P`
section. The exception directory is file-backed (475,547 entries), so function bounds come straight from the file.
`.reloc` is only 0x120 bytes, so vtables and function-pointer tables are filled in at runtime: static call graphs
stop at virtual calls. Headless Ghidra with `-noanalysis` decompiles single functions in seconds.

**The cut co-op mode.** Script-node and type names in the exe include `HC_FrontendNode_CoopHeistMode`,
`CanStartCoopMatch`, `IsCoopSessionHost`, `GetCoopMapsInfo`, co-op game-state nodes (team lives, timer, end),
scoring, `CoopReinforcementsManager` (waves, closets, fireteams), co-op ping points, "coop sense", co-op taunts,
heist targets (carried, escorted), `QuestObjectiveCoopGoToLocation` and `HC_IsHostOrOfflineScriptNode`. A session
property writer sets `CoopLobbyMaxSize` to a constant 4 (RVA 0x3cec950).

**Settings data.** A registry (RVA 0x414e080) gives each of 106 `helix/game system data/juice/**.juice` settings
files an index global and a lazy slot (array at RVA 0x9846ce8); getters call a lookup (RVA 0x254db80) into a
settings database at `*(*(RVA 0x97d20f0)+0x6e8)` that returns 0 for missing files. A runtime read showed all 106
files in that database with loaded objects, including `coopsettings`, `coopmapsettings`, `coopcharactersettings`,
`coopalarmsettings`, `cooppingpointsettings`, `cooptauntsettings` and `heisttargetsettings`.

**Netcode.** A 29-entry online tunables table (RVA 0x3e8c980) covers Ubisoft login, matchmaking, eleven `UDP_*`
transport values (reliable/frame windows, resend, RTT estimator, relay discovery), `CoopLoadingTimeoutMS`,
`CoopHotjoinSyncTimeoutMS` and voice chat. One `WSASendTo` and one `WSARecvFrom` call site (batched, non-blocking).
Session states include `CustomGame, Quickplay, MapVoting, Briefing, HostMigration`. Script nodes under `Coop/`
include `Replicated Variables`. Matchmaking (`MatchmakingClient_*`) and party (`PartyClient_*`) go through Ubisoft
services, which a mod must not use.

**Data packs.** `helix/sdf/pc/data`: 1,236 `.sdfdata` packs plus `sdf.sdftoc` (magic `WEST`, version 0x2a). Its
64.7 MB file table is high-entropy and matches no zlib/LZMA/zstd/LZ4/Oodle header: most likely encrypted. Not
decrypted (it would need a key from the exe, which the EULA forbids).

**Logging.** The retail build sends no Snowdrop log lines to `OutputDebugString` (only Streamline, XeSS, Uplay).

**Tools that worked here** (owner-local, not in this repo): a `version.dll` proxy that replaces the 37-byte
string-format wrapper at RVA 0x12da70 with a logging copy, verified byte-for-byte against this build, plus
read-only dumps of the settings slots and database via `ReadProcessMemory` on the game's own process. Denuvo did not
react to that patch in four runs.

## What we changed

Nothing in the game. Research and runtime inspection only.

## QA

None. No co-op build exists.

## How to play

Not playable.

## Known issues

Not applicable yet.

## Next steps

1. Read `coopmapsettings` (`myCoopMaps`) at runtime: do the co-op maps it lists point to level data that ships?
2. Find a session path that needs no Ubisoft backend: start from the users of `CoopHotjoinSyncTimeoutMS` and the
   UDP layer's vtable at runtime (static pointers are relocated at load).
3. Find what opens `CoopHeistMode` from the frontend (script flag or menu node).

Practical notes for the next agent:

- Ubisoft Connect restarts the *registered* install when a copy's exe starts, so test copies don't work; a proxy
  DLL in the install (owner-approved, removed afterwards) did.
- Windows Defender flags a self-built `CreateRemoteThread` + `LoadLibraryW` injector
  (`Behavior:Win32/DefenseEvasion.A!ml`) on first use.
- Denuvo activation was not triggered by the proxy DLL runs.
