# Rick and Morty: Virtual Rick-ality (2017)

| | |
|---|---|
| Engine | Unity 5.4.3p4, Mono, .NET 3.5 profile, Windows x64 (BepInEx 5.4.23.5, Harmony) |
| Versions tested | Steam (app 469610), build 2288226. `RickAndMortyVR.exe` SHA-256 `01678ebd02edec86258c482fbb5d00f3f0474e1a6515a905531f81df1a80ec50` |
| Projects | Rick and Morty Together ([rick-and-morty-coop](https://github.com/multiplayereverything/rick-and-morty-coop), [v0.7.0 pre-release](https://github.com/multiplayereverything/rick-and-morty-coop/releases/tag/v0.7.0)); directory entry "Rick and Morty Together" |
| QA level | automated |
| Last updated | 2026-10-30 |

## Where things stand

Two players can play the story together: the living room, Rick's garage and every mission area, up to the final
boss. Each player is a Morty-clone head with Morty hands. Items, doors, levers, buttons, the story, the levels, Rick's
and Morty's lines, enemies and the boss are the same for both, with the host's game in charge. It is an alpha. Every
feature is covered by scripted tours with two or three game copies, and each fix has a mutation check. A player
without a headset can join in flat-screen mode. The author has started real two-player sessions.

## Existing work

No other co-op project for this game was found in this directory, on Nexus or on GitHub. The game is single-player
only.

## How the game works (for co-op)

- **Engine and hooking:** Unity 5.4 Mono, so the logic is in `Assembly-CSharp.dll` (Owlchemy's `OwlchemyVR` framework).
  Unity 5.4 loads the game's scripts into a child domain. BepInEx's default entrypoint (`Application..cctor`) runs the
  chainloader before `Assembly-CSharp` is loaded, and every scene then logs "The referenced script on this Behaviour is
  missing!". With `[Preloader.Entrypoint] Type = SceneManager`, `Method = Internal_SceneLoaded` the chainloader starts
  at the first scene load and everything works, so the mod ships that `BepInEx.cfg`.
- **Player:** one VR rig (`MasterHMDAndInputController`) with a head camera and two `HandController`s. A grab is a
  `ConfigurableJoint` from the hand to a `GrabbableItem`. Teleporting moves the rig between `TeleportZone` pads.
- **Story:** the "Brain", a data-driven cause → node → effect graph. Every input is a world event string
  (`WorldEventsManager.EventOccurred*`); effects run on `BrainControlledModule_*` components (voice lines, Rick's
  animations, scene activation, spawns). The linear progression stage comes from `ProgressionManager.SetStage`.
- **Levels:** the living room is its own scene. The garage and every mission are one multi-scene loaded additively;
  moving between them activates and deactivates scenes.
- **Netcode:** none. The game ships Steamworks.NET, old enough that it has no Steam Networking Sockets, so the mod uses
  `ISteamNetworking.SendP2PPacket`, which goes through Valve's relay.
- **Without a headset:** `-vrmode None` keeps Unity from loading OpenVR. The game then needs three repairs (platform
  detection, the play-space size wait, the first scene load). After that the rig runs in the game's own debug mode,
  where a public `isGrabbing` flag drives the grab, so a test bot uses the game's real grab and release code.

## What we changed

- **Players:** each copy draws the other player as the game's own Morty-clone head and hands, following their head and
  controllers, with open and closed grip and what each hand holds.
- **Items:** host-granted ownership with an epoch per claim. The last hand to grab wins and the previous holder lets
  go. The owner simulates and streams at 20 Hz; everyone else follows kinematically.
- **Controls:** levers, doors and sliders use the same ownership and stream their position and lock state, driven on
  followers through the game's own lock calls. Buttons and pull cords are events replayed by the host. Each player's
  own height lever and spectator-camera lever stay theirs.
- **Story:** the host's Brain decides. Its cause keys are replayed on clients; a client's own causes become requests
  to the host, which drops echoes of an event it just ran. The Brain's dice are seeded the same on every copy, so
  random line choices match.
- **Levels:** clients follow the host's scene loads and garage areas, and their own loads are held back.
- **Things the game makes:** every new "(Clone)" grabbable on the host gets an id and is announced. A client adopts
  its own matching copy (same name, within 1.5 m, about the same moment), copies one it lacks (from a prefab if
  needed), and removes one only it made. Something destroyed on the host goes everywhere.
- **Enemies and boss:** the host's game runs them; clients show them, either player can shoot them (hits go to the
  host), and they aim at the nearest player. One shared final boss state.
- **Death:** each player goes to their own death scene and comes back; the other plays on.
- **Late join:** a player joining a scene the host has been in for a while skips its opening lines, and characters
  look at the nearest player's head.
- **Flat-screen mode:** keyboard and mouse for a player without a headset (walk, crouch, reach, grab with either hand,
  trigger, teleport), able to play with a headset player.

Each fix has a `Mutant("name")` switch that turns it off; the mutation run checks that its tour then fails.

## QA

- `scripts/setup-lab.ps1` makes test copies of the game. `scripts/run-tour.ps1 -Tour <tour> [-Steam] [-Lag 120]`
  starts a host and one or two clients, minimized and without a headset, and an in-game bot drives each through a
  tour. Each copy writes named checks; the runner also fails on game errors and compares item and control positions
  between copies (desync).
- Tours: `handoff` (an item passed hand to hand; also over a Steam lobby and with 120 ms lag), `controls`, `story`,
  `spectator`, `flat`, `spawns`, `enemies`, `death`, `boss`, `playthrough` (every mission area together), `latejoin`,
  `full` (a third player is refused).
- `scripts/mutants.ps1` runs each fix's mutant against its tour.
- Last results for v0.7.0 (2026-10-28 and 2026-10-29): every tour PASS, for example `playthrough` 14/14 host checks
  and 8/8 client checks; 23 mutants, each caught by its tour.

## How to play

Install the [BepInEx 5.4.23.5 loader release](https://github.com/multiplayereverything/rick-and-morty-coop/releases/tag/bepinex-5.4.23.5)
into the game folder, then the mod ZIP from the
[releases](https://github.com/multiplayereverything/rick-and-morty-coop/releases), keeping the mod's `BepInEx.cfg`.
Press F5 on the desktop window to host on Steam (friends or public lobby, invites), browse public lobbies, or host and
join over LAN or direct IP (TCP 27040). Two players.

## Known issues

- Putting a part into something (the computer's RAM stick, crystal and fuse) happens on that player's side only;
  pulling a part out is shared.
- A player who joins mid-scene hears the characters' timed banter a little behind the host until the next story event.

## Next steps

- Share attach points (a part put into the computer goes in for both).
- Copy the Brain's running nodes for a mid-scene joiner, if players notice the banter lag.
