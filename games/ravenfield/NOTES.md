# Ravenfield — working notes

## 2026-11-18 — triage and verification

### Observations (ran)

- Steam EA39 build 39, app 636480, buildid 26324504. `ravenfield.exe` SHA-256
  `7e7b00094299ea6d6139e66a6c2f8975a2a1a7cff1ea638a994c5dfcf4200e53`.
  Unity 2020.3.49f1, Mono, x64. Game log prints `EARLY ACCESS BUILD 39`.
- `DIRECTORY.md`, `games/`, issues and PRs in this repo had no Ravenfield work.
- Official RavenM last release 0.7 (2024-05-15, EA29); issue #68 says EA31 unsupported,
  #65 says an EA30 build was never stable enough to release.
- Community fork `RavenfieldCommunity/RavenM` publishes per-version assets EA35-EA39,
  latest `RavenM-EA39-CHNE1d4d0.zip` (asset dated 2026-10-04, commits to 2026-10-31).
- BepInEx 5.4.23.5 x64 + `RavenM_EN.dll` on an isolated copy: plugin loads, main menu,
  menu only in Instant Action. `Create Host Room` → `OK` logs `Joined lobby!`, room ID shown,
  members 1/250.
- Public lobby by default. A remote Steam user connected within ~60 s;
  host logged `Accepted the connection` and `Connected to server.` while the match ran.
- `Packet failed to send: k_EResultInvalidParam` appears twice before any lobby exists.
- The same package was then installed into the real game folder and launched: identical
  RavenM startup, no exceptions.

### Derived

- Current game is EA39, so the community EA39 build is the correct mod build; official
  0.7 cannot run on it.
- RavenM is Instant Action multiplayer; campaign story is not in its sync surface.

### Guesses

- The Halloween update (2026-10-29) and the later hotfix likely added content only, since
  the EA39 build still hosts and accepts clients. Not proven.

### Unknown

- Next game build (EA40) compatibility.
- Stable player/bot ceiling (upstream mentions problems at 100-200 bots).
- Whether the community edition patches the friends-only default anywhere.

### Sources

- https://github.com/iliadsh/RavenM and its releases/issues
- https://github.com/RavenfieldCommunity/RavenM and the `test` release assets
- https://ravenfieldcommunity.github.io/docs/en/Projects/ravenm.html
- https://store.steampowered.com/app/1104390/ (RFMP)
- https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=636480 (EA39, security
  patch 2026-09-19, Halloween 2026-10-29)

### Local

- Game folder: `<game dir>`, Steam library on `D:`. Test copy at `<test dir>` used
  junctions for `ravenfield_Data` and `MonoBleedingEdge` to avoid copying 4.4 GB.
- Installed into the real folder with owner approval: `winhttp.dll`, `doorstop_config.ini`,
  `.doorstop_version`, `changelog.txt`, `BepInEx/`.

## Next

- Two-client automated harness; needs a second Steam account or a LAN fork.
- Re-check on EA40 and record the first broken subsystem.
