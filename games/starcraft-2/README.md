# StarCraft II (2010): campaign co-op

| | |
|---|---|
| Engine | Blizzard SC2 engine (Galaxy scripts, data XML, MPQ maps/mods); s2client API in the retail client |
| Versions tested | Battle.net retail 5.0.17.98310 (data version 9C1F3D3D30CA252E33FC67CA05CBE1EA), enUS, Windows 10/11 |
| Projects | [SC2Coop](https://github.com/Cryswar/sc2-coop) (launcher code, MIT, test releases) |
| QA level | automated (local two-client smoke test: all 29 missions of one WoL@Kit campaign and of three campaigns on Blizzard's scripts, 4 missions of a fourth) + two-PC sessions by testers |
| Last updated | 2026-10-23 |

## Where things stand

Alpha: public test builds at [Cryswar/sc2-coop](https://github.com/Cryswar/sc2-coop) (code and releases, MIT). Two
players play Wings of Liberty campaign missions together over Steam (Valve's free Spacewar app id as a
relay) or a direct connection, in two kinds of custom campaigns: WoL@Kit fan campaigns, and campaigns that keep
Blizzard's own campaign scripts (most WoL mods). Both share one army (player 1), or, on maps made for it, each has their
own. Each player walks around their own copy of the hub (Hyperion), and a mission pick starts the mission for both.
Two-PC sessions by testers played several missions, victories and bonus objectives without game-breaking problems. The
guest's input runs through a click bridge, so it feels slightly less direct than the host's.

## Existing work

- **Heart of the Swarm Co-Op** (LordDz and team, Team Liquid thread, last post 2019-12): an Arcade remake of HotS
  missions with per-player hero choices. Arcade-only (needs the map from Battle.net), not tested here.
- No non-Arcade co-op for the retail campaigns was found (searched 2026-10-16: GitHub, SC2Mapster, Team Liquid,
  Reddit, Steam via web search). Blizzard's own Co-op Missions mode is a separate PvE mode, not the campaign.
- Archipelago's SC2 client drives the retail game through the same API (single player) and was a useful reference.
- Blizzard's [s2client-proto](https://github.com/Blizzard/s2client-proto) and its
  [protocol docs](https://github.com/Blizzard/s2client-proto/blob/master/docs/protocol.md) describe the API used here.

## How the game works (for co-op)

- **The API.** The retail client accepts `-listen/-port` and speaks s2client-proto over a websocket. It can create
  multiplayer games of any local map, and players join with ports. Multiplayer only works when every client uses a
  loopback host address, so the launcher tunnels the four game ports between PCs itself (one TCP stream, or Steam's
  P2P relay; game UDP is sent unreliably).
- **Campaign = maps + mods + banks.** Missions and the hub (story map) are `.SC2Map` files depending on mods; a campaign
  library moves between them with `GameSetNextMap`/`GameOver`: Blizzard's `libCamp` in the retail campaign and in mods
  built on it, or Kit's renamed copy (`LibWoLC`/`LibWCMI`) in WoL@Kit campaigns. All campaign progress lives in player-1
  bank files.
- **Player slots.** Maps declare their players in `MapInfo`. Adding a user slot (a copy of player 1's record) is enough
  for a second human; an alliance with shared control, vision and spending makes them co-own player 1's army. A map
  with a second user slot of its own is played with an army each.
- **API quirks that shaped everything** (all observed, see NOTES.md; the full list, with the campaign-library and
  Windows input traps, is the project's [PITFALLS.md](https://github.com/Cryswar/sc2-coop/blob/main/docs/PITFALLS.md)):
  - The creator's native world input needs `raw=true` in InterfaceOptions; a joiner's world clicks are dropped, so the
    guest's clicks go through the API as feature-layer actions (the "click bridge").
  - Any objective set to completed or failed makes the API end the game for both clients (status 5).
  - `RequestObservation` crashes for guest player ids above 3 in some setups (the API's `3 - player` 1v1 assumption).
  - `RequestGameInfo` crashes on story maps. A story map's `GameOver` loads the next map unless it is cleared.
  - A running game doesn't re-read a bank file changed on disk (`BankLoad` returns the cached bank).
  - A process that already played a game does not join another multiplayer game (join hangs).
  - A joining client that was killed keeps its copy of the shared map; reusing the same map name later loads the stale
    copy and the join fails with ChecksumError, so map names must change with the content.
  - SC2 sometimes exits with -2000/-2001 right after start; a retry works.
  - `text` is a Galaxy type name: a local variable called `text` disables the whole map script without a message, and
    so does a function that reads a mission global declared further down, or a `/* */` comment (Galaxy has none).
  - Missions started through the API are not transition maps (`GameIsTransitionMap()` is false). Both campaign
    libraries treat that as "started from the editor": a debug tech purchase recomputes credits and progress, stale
    per-mission restart snapshots are loaded, and Mission Archives replays write old progress back. Each needed a
    rewrite or a repair after the mission (2026-10-22/23).
  - API games run at Faster; trigger code can set any speed mid-game once the minimum is lowered and the speed
    unlocked. Command-line display options (`-displayMode`, window size) override the player's own settings.

## What we changed

The SC2Coop launcher (C#, .NET Framework; no game files are shipped, everything is built from the player's own install).
How it fits together: [ARCHITECTURE.md](https://github.com/Cryswar/sc2-coop/blob/main/docs/ARCHITECTURE.md).

- **Mission adapter** (applied to each mission map at build time): guest slot in `MapInfo`; the guest gets player 1's
  alliances with every other player (re-applied every second); objective state calls routed so completed/failed
  objectives are shown as a name suffix instead of ending the API game; the 13 camera functions aimed at player 1
  mirrored to the guest (cutscenes); player-1 pings shown to the guest; skippable cutscenes skip on one player's Esc;
  next map cleared; difficulty, team colors and game speed set (`-coopspeed` changes it); the campaign libraries'
  "not a transition map" paths removed or repaired; defeat shows a co-op dialog (retry the mission or return to the
  hub); test chat commands (`-coopwin`, `-coopwinnow`, `-cooplose`, `-coopobj`).
- **Separate armies** (maps with a second user slot): player-1 unit counts and ownership checks count both armies;
  money and tech given to player 1 are mirrored; rescues, reinforcements and resource pickups go to whoever found them,
  alternate, or go to both, as the host chooses.
- **Hub adapter**: mission launches recorded in a bank for the launcher (Kit's and Blizzard's launch functions,
  Mission Archives replays in both); a pick in one hub launches the same mission in the other, so both players see the
  briefing; hub chat relayed between the two hubs; the buddy's shops view-only (one army, the host's progress); the
  host's hub saves itself so mission rewards are paid once; mission select defaults to the campaign difficulty;
  `-cooplaunch <mission>`.
- **Campaign flow**: both hubs as solo API games; a pick in either closes both and starts the multiplayer mission;
  afterwards banks sync host -> guest and both hubs reopen. One co-op save per campaign (campaigns share bank names),
  with a reset. Setup check at connect (launcher, maps, every mod by
  SHA-256), bank autosave before each mission and repair after a crash, crash and connection-loss handling, a loading
  curtain between games, player names, and team colors read from the campaign's own data.
- **Click bridge** for the guest: selection, box select, orders, targeted abilities (target-mode events plus a probe
  order swapped for the real ability), build placement, arrow-key scrolling, pings with a ping panel. Input is read with
  Raw Input (no system-wide hooks) while the guest's window is in front; overlays are skipped in exclusive fullscreen.
- **Transport**: TCP 6600 with UPnP, or Steam P2P via the Spacewar app id (friend invites, lobby), with a heartbeat.
- **Map checker** (`campaign-check`): lists per map what the launcher handles and what needs a map author. The porting
  checklist is in [PORTING.md](PORTING.md).

## QA

- **Smoke test** (one PC, two real clients, copy of the install, test hooks `SC2_COOP_TEST_LAUNCH`/`SC2_COOP_TEST_WIN`):
  each mission launched from the hub by chat command, won with `-coopwinnow`, back to both hubs. Passed: all 29 missions
  of one WoL@Kit campaign (2026-10-20) and all 29 missions of each of three campaigns that keep Blizzard's scripts
  (2026-10-22; two missions of one campaign passed on a second try after a first load ran past the 3-minute join limit,
  and one run was resumed after Windows Defender quarantined the test exe). On a fourth such campaign, 4 missions
  (2026-10-21). A/B choice missions were launched directly, skipping the choice dialog. Checks: both players join, API
  status stays "in game" until the win, the mission ends normally, banks sync, both hubs reopen.
- **Targeted tests** (one PC, test hooks): objective completion keeps both clients in game; a crash of either client
  returns both to the hub with banks intact (a truncated bank is restored); Stop and a lost connection end every wait;
  a setup mismatch is reported on both sides; cutscene camera identical on both clients; a guest-only Esc skips;
  separate armies: defeat only when both armies are gone, rescues go to the nearer player, pickups pay both.
- **Campaign state** (one PC, banks read after each step, 2026-10-23): credits after a win equal exactly one mission
  reward; completed missions and credits survive Mission Archives replays (checked over two replays, and on a replay of
  an early mission); game speed measured from the game clock (Faster 1.41, Normal 1.00, Slower 0.61 game seconds per
  real second), also on Brutal.
- **Two-PC sessions** by testers over Steam (2026-10-19 to 2026-10-23): missions 1-3 to the Hyperion, several later
  missions, victories, bonus objectives, defeats with retry, Mission Archives replays, one separate-armies test map.
- Not covered: a full campaign playthrough in co-op, long sessions. The guest's input quality is judged by players.

## How to play

Download `SC2Coop.zip` from the project's [releases](https://github.com/Cryswar/sc2-coop/releases). Both players
unzip it and start `SC2Coop.exe` (it finds and prepares their own SC2 install), tick Campaign and pick the campaign
folder, then one hosts (Steam or address) and the other joins. The player guide is `README.txt` in the zip.

## Known issues

- Antivirus: Windows Defender's cloud ML has quarantined builds (Trojan:Win32/Prowloc.A!cl). Replacing the system-wide
  input hooks with Raw Input made it rarer, but fresh unsigned console test builds are still flagged now and then.
  Code signing is the real fix.
- The guest's commands react about one network round trip later than the host's. Game traffic mostly uses the reliable
  channel, so packet loss on the relay shows up as stutters.
- In campaigns on Blizzard's scripts, the guest's campaign tech in separate armies is not hooked up yet. The Simulator
  and the Arcade in the hub are not supported.
- Early missions of campaigns on Blizzard's scripts (TRaynor01-03) still run the library's debug tech setup when they
  load; the launcher repairs its effects afterwards, but can't remove it.
- An SC2 client in exclusive fullscreen may crash when the launcher draws over it ("Lost D3D9 device"); the launcher
  detects this and stops drawing, and players should use Windowed (Fullscreen).
- Some mission triggers react only to the host's input (tutorial prompts, mission choice dialogs).

## Next steps

- Two-faction co-op with each player's own progress (both players' banks in one mission).
- Heart of the Swarm campaigns (several hub maps, a different campaign library, Kerrigan's progress): feasibility check
  under way.
- Later: Legacy of the Void (the Spear of Adun is tied to player 1), custom campaigns with their own hub maps, a race
  swap option.
- [NOTES.md](NOTES.md) is the dated lab notebook.
