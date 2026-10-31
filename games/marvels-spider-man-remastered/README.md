# Marvel's Spider-Man Remastered (2022)

| | |
|---|---|
| Engine | Insomniac proprietary engine, 64-bit, DirectX 12. Closed binary: one `Spider-Man.exe` (121,901,008 bytes) plus middleware DLLs; game data in `<game dir>\asset_archive\` (a `toc` file, `dag`, `a00s034.us` and archives `g00s000` to `g00s033`) |
| Versions tested | Steam, app id 1817070, build id 23986256. `Spider-Man.exe` SHA-256 `e297d4d94f1ffe4febf289745e79e7b6fa233a788e7a00f480fc77c55db81ad1`. The exe carries no file version resource |
| Projects | Spider-Man Remastered Multiplayer by hbgda: https://www.patreon.com/hbgda (directory entry "Spider-Man Remastered Multiplayer (hbgda)"). LAN co-op prototype built here: [mod/](mod/) (directory entry "Spider-Man Remastered LAN co-op (prototype)") |
| QA level | hbgda's mod: community-reports. The prototype here: automated (two copies of the game on one PC, driven by scripts; no two-PC or human test yet) |
| Last updated | 2026-10-22 |

## Where things stand

Yes, with a paid closed beta. hbgda's mod lets several players (up to 7 reported) swing
around the open world together, and free roam, hideouts and combat or stealth challenges
are reported to work. The story and side missions can be entered in co-op but were not built
for it; players report bugs, crashes and broken cutscenes there.

A second, open-source route now exists as a prototype: [mod/](mod/) is a proxy `winmm.dll`
that turns on the LAN session code left in the exe and shows the partner as a puppet
Spider-Man. Each player still runs their own single-player game; only the two heroes are
shared. It has only been run as two copies on one PC.

## Existing work

Searched on 2026-10-19: this repo's directory, web search (results were news articles; a
query aimed at Reddit, GitHub and X returned none of those sites), the Nexus page and the
Patreon page. Reddit, GitHub and X were not searched directly.

- **Spider-Man Remastered Multiplayer (hbgda).** Closed beta for patrons of the $10
  "Web-Head" tier (ComicBook, 2025-08-26). Not open source; no public repo found.
  - Coverage starts late August 2025: ComicBook 2025-08-26, Kotaku and Game Rant
    2025-08-27, 80.lv 2025-08-28.
  - Last activity: the newest Patreon post, "SMT Progress Update and Recent Issues", was
    about four days old when fetched on 2026-10-19 (the post is locked; its content is
    unknown).
  - A community Discord had 11,000+ members, 2,000+ active (Insider Gaming, 2025-08-28); no public invite link is
    given here.
  - The Nexus page (mods/5696) returned HTTP 403 to an automated fetch on 2026-10-19. The
    earlier directory note said Nexus staff removed it; that could not be rechecked.
- No other co-op project for the Remastered edition was found. The Marvel's Spider-Man 2
  multiplayer mods are separate projects for a different game.

## How the game works (for co-op)

From the installed copy, Ghidra and in-game tests (details and addresses in [NOTES.md](NOTES.md)):

- One large native exe; the install root has no loose script files. Data sits in
  `asset_archive\` (a `toc` file plus numbered archive files).
- Middleware DLLs present: `steam_api64.dll`, `bink2w64.dll`, `XAudio2_7.dll`, NVIDIA
  Streamline / DLSS, AMD FidelityFX, Intel XeSS, `crs-client.dll` / `crs-handler.exe`
  (crash reporter).
- **The retail exe still contains Insomniac's LAN session code.** An earlier version of this
  write-up said there was no stock netcode; that was wrong. Present and working once enabled:
  a LAN room list (UDP 59003, multicast 226.0.0.1), host and join (`GameNetManager`,
  `SessionLan`), a connect state machine with game-state and load-state sync, late join into a
  running host, and an in-process sync host (`EskimoSync::SyncHost`). It is switched off by a
  `LanEnable` setting, the host call has no callers, and story game modes allow one player.
- What it does not give you: the engine's remote hero. A late-joining client spawns heroes that
  lack components single player always has, and they crash on activate/update. The prototype
  blocks the remote hero and draws its own puppet instead.

Unknown: how hbgda's mod hooks the game, what it syncs, its transport (Steam or its own
servers), and which game builds it supports. The Patreon posts that would say are locked.

## What we changed

The prototype in [mod/](mod/) (C++, one proxy DLL, no game files):

- Patches the LAN gate, allows a second instance, raises the game modes' player limit to 2,
  and hosts or joins through the game's own LAN session.
- Stops the engine from creating the remote hero and spawns a plain hero actor as the
  partner's puppet. It dresses the puppet in the partner's suit through the game's own
  item loader (the one the suit menu uses), which streams the suit's assets in.
- Sends position and the full skeleton pose (237 joints) over its own UDP channel (port 59010),
  about 15 KB per packet at up to 30 Hz. The partner's game finds the host by LAN broadcast.
- F7 hosts at the title screen, F9 joins and warps the partner to the host, F8 warps again.
  A partner who goes silent for 2 s has their puppet parked out of sight.

## QA

Prototype, automated level only. Scripts in `mod/tools/` start two copies of the game on one PC,
press F7 in one and F9 in the other, and take screenshots of both. On the release build both
games showed both Spider-Men together on a rooftop, with the partner's movement and animation.
The two games happened to wear different suits, and each showed the partner in the
partner's suit. Changing the suit in the game's suit menu changed the puppet too.
Killing the partner's game hid its puppet on the host, which kept running. Saves were backed
up before every run and restored after, checked by hash.

Not tested: two PCs, a human playing, long sessions. hbgda's mod was not run.

## How to play

- hbgda's mod: join the Patreon tier that grants beta access and follow the install steps
  there. Download only from the Patreon; re-uploads elsewhere are unvetted.
- The prototype: build it and follow [mod/README.md](mod/README.md). Back up your saves first.

## Known issues

hbgda's mod, from player reports in 2025:

- Main story and side missions are not designed for co-op: expect bugs, crashes and broken
  cutscenes.
- Closed beta: access is limited and paid.

The prototype:

- The partner's suit loads in its default style; suit options (such as variants) may not match.
- No shared missions, enemies or world. Each player is in their own game.
- The puppet is parked, not destroyed, when the partner leaves.
- Build 23986256 only. Any other build is detected and the DLL does nothing.

## Next steps

- Prototype: a two-PC LAN test, destroying the puppet on leave, then shared combat.
- hbgda's mod: record which game build(s) the current beta supports, and whether Steam
  updates break it. If someone with beta access agrees, document the transport and what state
  is synced, without copying the mod's files.

## Sources

- https://comicbook.com/gaming/news/marvels-spider-man-co-op-multiplayer-mod-pc/
- https://kotaku.com/spider-man-ps4-pc-multiplayer-mod-great-web-insomniac-2000620767
- https://gamerant.com/spider-man-multiplayer-pc-mod/
- https://80.lv/articles/spider-man-remastered-received-a-fan-made-multiplayer-option
- https://insider-gaming.com/spider-man-co-op-mod/
- https://www.patreon.com/hbgda
