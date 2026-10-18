# Poppy Playtime — notes

Game slug: `poppy-playtime`. Sessions: 2026-10-18.

## Phase 0 — Triage (2026-10-18)

### Identity (rule 5)

| Field | Value | Source |
|---|---|---|
| Title | Poppy Playtime (base game = Chapter 1) | Steam store API appid 1721470 |
| Store | Steam | owner confirmation + `appmanifest_1721470.acf` |
| Install path | `<game dir>` = `D:\Steam\steamapps\common\Poppy Playtime` (owner's PC; local-only) | read-only scan of Steam libraryfolders.vdf |
| Version / build id | Steam buildid **21905565** (appmanifest StateFlags 4 = complete, SizeOnDisk 5,899,523,032 bytes, LastUpdated 1792338989) | `appmanifest_1721470.acf`, 2026-10-18 |
| Main launcher exe SHA-256 | `1E6CE253F97D663E8E3D6E8F2C7D3E20314D9906BEC5208BFF07578C4E31CC4D` — `<game dir>\WindowsNoEditor\Poppy_Playtime.exe` (345,600 bytes) | Get-FileHash, 2026-10-18 |
| Game exe SHA-256 | `0808123FFE2A8A002CA83DDD090C7796010DEB7C79CB52A79B71463F3DAD5912` — `<game dir>\WindowsNoEditor\Poppy_Playtime\Binaries\Win64\Poppy_Playtime-Win64-Shipping.exe` (80,168,960 bytes) | Get-FileHash, 2026-10-18 |
| Engine | **Unreal Engine 4.27.2** — exe `FileVersionRaw 4.27.2.0`; layout `WindowsNoEditor/`, paks `pakchunk0-WindowsNoEditor.pak` (2.65 GB) + `pakchunk0optional` (27 MB) | exe version info, dir listing |
| Release | 12 Oct 2021 | Steam store API |
| DLC | Chapters 2–5: appids 1817490, 2555190, 3008670, 4100940 | Steam store API |
| Steam categories | **Single-player only** (no Remote Play Together, no LAN, no co-op categories) | Steam store API `categories` |
| OS | Windows only | Steam store API `platforms` |
| Multiplayer leftovers in binaries | `Poppy_Playtime\Binaries\Win64` ships **`vivoxsdk_x64.dll` (5.3 MB)** and **`ortp_x64.dll`** — VoxeX voice-chat SDK (multiplayer voice infra) inside a single-player game; also `OpenImageDenoise.dll`, `tbb12.dll` | dir listing, 2026-10-18 |

### Answers to the triage questions

1. Which exact game? Poppy Playtime base (Chapter 1), Steam — install still downloading.
2. Already done? **No.** No `DIRECTORY.md` entry, no `games/` write-up, no issue/PR in
   cranebeaker/coop-anything mentions Poppy Playtime (checked via GitHub search API,
   `total_count: 0`, 2026-10-18).
3. Engine? Unreal Engine 4 (pending install verification).
4. Source available? None. Closed binary; no script source shipped. Modding scene exists
   on Nexus (58 mods: utilities, cosmetics, cheat menus — "UETools", "PlaytimeReborn"
   mod menu) → UE4 modding via UE4SS/blueprints is proven feasible by the community.
5. Engine multiplayer? Poppy Playtime itself: none shipped (single-player category only).
   **Lead:** TCRF documents an unused multiplayer minigame inside Chapter 2 builds
   (https://tcrf.net/Poppy_Playtime:_Chapter_2/Multiplayer — fetch returned 403 to the
   agent, text seen only via search snippet) → MOB Games had in-house UE multiplayer work.
   MOB's separate game Project Playtime is full UE multiplayer (6p) in the same universe.
6. Legal? Players own the game (base is free-to-play on Steam anyway). A co-op mod would
   ship as a separate package, no game files, no anti-cheat involved (single-player game).

## Phase 1 — Research (2026-10-18, agent search pass)

### Findings (observations — each with source)

1. **No usable co-op mod exists** as of 2026-10-18:
   - Nexus Mods Poppy Playtime: 58 mods, tops are utilities/cosmetics/cutscene
     compressors, no multiplayer mod (nexusmods.com/poppyplaytime, search pass).
   - GitHub repo search "poppy playtime" (435 results) and "poppy playtime multiplayer":
     no co-op/multiplayer projects — fan-game clones and junk only. One result looked
     like a piracy download page; not used (rule 10).
2. **A fan multiplayer mod existed and was deleted.** Reddit r/PoppyPlaytime thread
   "Anybody have a poppy playtime multiplayer mod?" (2022):
   https://www.reddit.com/r/PoppyPlaytime/comments/uut6zj/ — dev of the YouTube mod
   "Poppy Playtime's Multiplayer Mod Is INSANE"
   (https://www.youtube.com/watch?v=7nRBqSjCB0k) confirmed it was possible, files
   deleted. Follow-up video: "Scrapped Mods: Poppy Playtime Multiplayer WIP Mod"
   (https://www.youtube.com/watch?v=RuR7kCDEjeQ).
3. **Unused multiplayer content in Chapter 2** (TCRF, link above; snippet-level evidence
   only, direct fetch 403).
4. **Official multiplayer spin-off: Project Playtime** (Steam appid 1961460, free,
   6 players: 5 survivors + 1 monster). Private lobbies/playing with friends: community
   guides and 2025–2026 videos confirm it works; older threads complain about missing
   invite system. Server status pages report operational (low-confidence source).
   https://store.steampowered.com/app/1961460/
5. **Steam Remote Play Together: not supported** for Poppy Playtime (absent from store
   categories). Parsec (game-streaming with shared control) remains possible but gives
   friends control of the same single character — not true co-op.
6. **Nucleus Co-op: no Poppy Playtime handler found** (searches on splitscreen.me /
   nucleuscoop community, 2026-10-18). Nucleus needs the game to have local-split or
   LAN play to link instances; Poppy Playtime has neither → not applicable.

### Derived (conclusions from the observations)

- True campaign co-op for Chapter 1 = **Path 2 (build from zero)**: UE4 reverse
  engineering (UE4SS, SDK dump/Dumper-7), host-authority state sync; per README this is
  days+ of agent work with intermediate non-playable states.
- Quick options for the owner's actual goal ("поиграть с друзьями"):
  1. Project Playtime — official, free, works with friends today (different game mode).
  2. Parsec co-pilot session — watch/help one player, not real co-op.
  3. The mod itself — declined for now by the owner.

### Guesses (unverified)

- ~~Chapter 1 build likely also contains leftover multiplayer references like Chapter 2's~~
  **Confirmed on binary level 2026-10-18:** VoxeX voice SDK (`vivoxsdk_x64.dll`,
  `ortp_x64.dll`) ships in `Binaries\Win64` of the Chapter 1 base install — voice chat
  has no purpose in a single-player game, so multiplayer/online-service code existed in
  this UE4.27.2 build (likely shared with the Project Playtime codebase).

### Unknowns

- Exact installed build id / exe hash (install incomplete).
- Whether Chapter 1 Steam build matches any known modding write-up (UETools etc. target
  specific builds).
- Population/server health of Project Playtime long-term.

## TODO

- [x] When install finishes: record buildid, exe SHA-256, engine version — done
      2026-10-18 (buildid 21905565, UE 4.27.2, hashes above).
- [ ] Open "claim a game" issue in cranebeaker/coop-anything (owner approved;
      needs gh CLI — permission to install pending).
- [ ] Add gap entry to data/directory.yaml + regenerate DIRECTORY.md; draft PR
      (owner approved; needs gh CLI).
