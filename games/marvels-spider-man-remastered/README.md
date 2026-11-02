# Marvel's Spider-Man Remastered (2022)

| | |
|---|---|
| Engine | Insomniac proprietary engine, 64-bit, DirectX 12. Closed binary: one `Spider-Man.exe` (121,901,008 bytes) plus middleware DLLs; game data in `<game dir>\asset_archive\` (a `toc` file, `dag`, `a00s034.us` and archives `g00s000` to `g00s033`) |
| Versions tested | Version recorded, no mod tested: Steam, app id 1817070, build id 23986256. `Spider-Man.exe` SHA-256 `e297d4d94f1ffe4febf289745e79e7b6fa233a788e7a00f480fc77c55db81ad1`. The exe carries no file version resource |
| Projects | Spider-Man Remastered Multiplayer by hbgda: https://www.patreon.com/hbgda (directory entry "Spider-Man Remastered Multiplayer (hbgda)") |
| QA level | community-reports (nothing was run for this write-up; see QA) |
| Last updated | 2026-10-19 |

## Where things stand

Yes, with a paid closed beta. hbgda's mod lets several players (up to 7 reported) swing
around the open world together, and free roam, hideouts and combat or stealth challenges
are reported to work. The story and side missions can be entered in co-op but were not built
for it; players report bugs, crashes and broken cutscenes there.

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

Observed on the installed copy (file listing only, nothing was decompiled):

- One large native exe; the install root has no loose script files. Data sits in
  `asset_archive\` (a `toc` file plus numbered archive files). Whether the archives hold
  scripts was not checked.
- Middleware DLLs present: `steam_api64.dll`, `bink2w64.dll`, `XAudio2_7.dll`, NVIDIA
  Streamline / DLSS, AMD FidelityFX, Intel XeSS, `crs-client.dll` / `crs-handler.exe`
  (crash reporter).
- The single-player game ships no multiplayer mode, so there is no stock netcode to build on.

Unknown: how hbgda's mod hooks the game, what it syncs, its transport (Steam or its own
servers), and which game builds it supports. The Patreon posts that would say are locked.

## What we changed

Nothing. This write-up records the game's identity and the state of the existing mod.

## QA

No test was run. The QA level rests on player reports in the press coverage (ComicBook,
Kotaku). Nobody here has run the mod against build 23986256.

## How to play

Join hbgda's Patreon at the tier that grants beta access and follow the install steps there.
Download only from the Patreon; re-uploads elsewhere are unvetted.

## Known issues

From player reports, 2025:

- Main story and side missions are not designed for co-op: expect bugs, crashes and broken
  cutscenes.
- Closed beta: access is limited and paid.

## Next steps

- Record which game build(s) the current beta supports, and whether Steam updates break it.
- If someone with beta access agrees, document the transport (Steam P2P, direct IP or a
  hosted server) and what state is synced, without copying the mod's files.
- Building an independent co-op mod would be a closed-binary, no-netcode project, the
  hardest case in playbook/triage.md. Joining or waiting on the existing mod is the
  better route.

## Sources

- https://comicbook.com/gaming/news/marvels-spider-man-co-op-multiplayer-mod-pc/
- https://kotaku.com/spider-man-ps4-pc-multiplayer-mod-great-web-insomniac-2000620767
- https://gamerant.com/spider-man-multiplayer-pc-mod/
- https://80.lv/articles/spider-man-remastered-received-a-fan-made-multiplayer-option
- https://insider-gaming.com/spider-man-co-op-mod/
- https://www.patreon.com/hbgda
