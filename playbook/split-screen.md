# Split screen: co-op on one PC

Split screen is the couch version of co-op: several copies of the game run on one machine,
each in its own part of the screen, each driven by its own controller (or keyboard and
mouse), and the copies talk to each other over the local network stack.

That means split screen is a **layer on top of multiplayer**:

- Game already has LAN or online co-op: split screen is mostly launcher work. This is where
  the existing tools shine.
- Game is single-player only: make it co-op first (the rest of this playbook), then add the
  split-screen layer.

## Starting points

All projects below have public source, so agents can read, fork and extend them.

| Project | Platform | License | Notes |
|---|---|---|---|
| [Nucleus Co-op](https://github.com/SplitScreen-Me/splitscreenme-nucleus) | Windows | GPL-3.0 | The most widely used tool, 700+ game "handlers" on [splitscreen.me](https://splitscreen.me/docs/games). Its [handler API](https://www.splitscreen.me/docs/handler-api/) is the best catalogue of multi-instance tricks there is. |
| [PartyDeck](https://github.com/partydeck/partydeck) | Linux, SteamOS / Steam Deck | MIT | Newer launcher: Gamescope windows arranged in a KWin session, Bubblewrap so each instance only sees its own controller, Proton (via UMU) for Windows games, Goldberg for Steam-API multiplayer, up to 4 instances, multiple keyboards and mice since 0.5 ([GamingOnLinux](https://gamingonlinux.com/2025/05/partydeck-is-a-split-screen-game-launcher-for-linux-steamos), [AUR](https://aur.archlinux.org/packages/partydeck-bin)). |
| [Couchlink](https://github.com/cranebeaker/couchlink) | Windows | MIT | Made at Co-op Anything. One copy per player as hardlinked folders, a hook the game loads as a stand-in system DLL so each copy sees only its own controller (XInput, DirectInput, Raw Input and HID) or its own keyboard and mouse, each player on their own Steam account or an emulated Steam identity, signed recipes, up to 8 players. For mod authors: the `couchlink` skill here and the `couchlink-sdk` skill in its repo. |

Things to know before building on them:

- **Nucleus Co-op's funding model.** The team runs a [Patreon](https://www.patreon.com/nucleus_coop/posts/new-nucleus-co-4-131882021)
  where supporters get new builds and handlers for popular games (recent examples include
  Monster Hunter and Subnautica 2) before they are public; they are released to everyone
  later. Some users object to this, others see it as normal early access
  ([Steam discussion](https://steamcommunity.com/app/3527290/discussions/0/592903515855138582/)).
  The source code is GPL-3.0, so you are free to study it, fork it and publish your own
  handlers and improvements, as long as derivative work stays GPL-3.0 with source available.
  The UI is dated; a fresh front end is fair game.
- **PartyDeck's MIT license** is the most permissive: you can reuse its code in any project,
  keeping the copyright notice.
- Credit both projects when you build on them, and contribute fixes back where it makes sense.

## How split screen works (what to reuse)

| Problem | Technique | Where to look |
|---|---|---|
| Game refuses a second copy | Rename or kill the single-instance mutex, defeat FindWindow checks, per-instance copies (hardlinks or symlinks) | Nucleus handler API: `KillMutex`, `RenameNotKillMutex`, `SymlinkGame`, `HardlinkGame` |
| Copies share config and saves | Per-instance Documents / AppData folders, per-instance INIs. Closed engines with no env-var support: hook `SHGetFolderPathA` (or the W variant) inside your injected DLL and rewrite the returned path per instance token | Nucleus `UseNucleusEnvironment`, `DocumentsConfigPath`; PartyDeck's Bubblewrap sandbox; Divinity II write-up (dinput8 proxy + MinHook) |
| Steam allows one copy, or the game needs Steam networking | A per-instance Steam API emulator, for copies you own, LAN play only | Nucleus `UseGoldberg`, PartyDeck's Goldberg integration |
| Input: every copy reads every controller | Hook the input APIs (XInput, DirectInput, raw input) per instance, or isolate devices in a sandbox | Nucleus input hooks, PartyDeck's Bubblewrap device filtering |
| Window layout and focus | Borderless windows resized per player; fake focus so background copies keep running | Nucleus `HookFocus`, `FakeFocus`; PartyDeck's Gamescope plus KWin |
| Copies must find each other | Point them at 127.0.0.1 / LAN discovery; an emulator's LAN mode | Handler networking options, Goldberg LAN |

The same tricks are what an automated QA harness needs to run several clients on one PC (see
[automated-qa.md](automated-qa.md) and [methods.md](methods.md)).

## The other route: a second player inside one copy

Running several copies isn't the only way to do couch co-op. For a game with a mod loader and one shared
screen, you can instead **spawn a second player character inside the single running copy**. There is no
networking and no second window. Learned on Dave the Diver (Unity IL2CPP, BepInEx); see
[games/dave-the-diver](../games/dave-the-diver/README.md).

- **Clone player 1 at runtime.** Clone the player object, then refill the references Unity doesn't copy.
  Next, find and split the state the clone still shares with player 1: input callbacks, effect handlers,
  ability callbacks, command assets, oxygen or health. Most early bugs are this shared state.
- **"Player 2's turn".** Single-player code finds "the player" through one field, often on a singleton.
  To reuse a game system for player 2 (grabs, damage, mash prompts, helper items, HUD), point that field
  at player 2, call the game's own method, and point it back. That is far less work than rewriting each
  system. Find the field by decompiling: the same `[singleton + offset]` load shows up in every
  player-facing method.
- **Animation events on enemies** often act on "the player" too: struggle animations, bite damage. Hook
  them and run them during player 2's turn when the creature is on player 2. Never decide by "which
  player is closer"; that picked the wrong player.
- **Input:** read player 2's keys or pad below the game's input layer (Win32 `GetAsyncKeyState`, XInput)
  so they never reach player 1. `GetAsyncKeyState` also reads keys while the game window is not focused,
  so check focus before acting on them. Give the game a virtual keyboard that carries only player 1's keys.
  When a game-owned screen (a mini-game) must read player 2, give the game a virtual gamepad fed from
  player 2's keys for as long as that screen is open.
- **When a sequence can't run twice** (a full-screen QTE, a modal), skip it for player 2. Run a mod-side
  version that calls the game's smaller pieces (start/stop animation, damage, result trigger). If the
  modal locks player 1, drive player 1's character directly from player 1's own bindings until it closes.
- **Limit:** one camera. Follow the midpoint and keep both players in view, and give player 2 their own
  HUD by copying and tinting the game's.
- **Second example: Dead Cells** (HashLink, DCCM; [games/dead-cells](../games/dead-cells/README.md)). The game has a
  real second-controller layer, so player 2 gets their own controller object pinned to pad 2. Game menus take an
  *exclusive* input lock on player 1's controller: when a menu is handed to player 2, release that lock first or
  player 1 stays frozen. A second HUD instance must share nothing with the first (sharing the minimap or counters
  crashed on area changes); find every function that writes to "the HUD" with a bytecode scan and run each with the
  owner's HUD. Look for state the game keeps once per profile or run rather than per character (outfit, a skill's
  charge counter, the kill counter, keys counted in "the hero's" inventory): each needs its own per-player copy or a
  shared rule. A pick that opens a confirm screen can still be declined, so give the partner their copy of an item
  only once the pick is done.

## Split screen inside one copy: three ways mods do it

Three mods from October 2026 show the main options when everyone plays in one running copy.
Check which case your game is before picking.

| Case | Example | How it works |
|---|---|---|
| **The game has split screen, but fewer players on PC** | [BO3 4-Player Local Splitscreen](https://github.com/LocalPlayer123/BO3-4-Player-Local-Splitscreen-on-PC) (Black Ops III, public domain) | The PC exe was compiled for 2 local players, while the PS4 build of the same game keeps 4. The mod moves every per-local-client array that is `[2]` on PC to a new `[4]` allocation and rewrites each instruction that addresses it. Only then does it raise the local-client count and widen the loops from 2 to 4 (containers first, count second, or slots 2 and 3 overwrite live memory). A few 2-player-only decisions (pane layout, controller sign-in, sun shadow slots) are hooked. Every patch checks the original bytes first and stands down on an unknown build. |
| **No split screen: draw the world once per player** | [SplitScreen for Oblivion](https://www.nexusmods.com/oblivion/mods/56734) (xOBSE plugin, C++) | Renders the world twice per frame, once from each player's camera, with an "alternate" mode that draws each view every other frame for weaker GPUs. Player 2 is an NPC to the engine; the plugin gives it the player's rules (inventory, levelling, crafting, dialogue) and bridges quest scripts that ask for "the player". The world only loads around player 1, so player 2 is on a leash. |
| **No split screen: extra player characters through a script framework** | [RE2 Party Mode](https://www.nexusmods.com/residentevil22019/mods/3024) on REFramework (Lua plus a pad DLL), built on Local Couch Co-op | Spawns up to 3 extra player characters, each with its own camera, screen quarter, inventory and settings. Enemies target the nearest player instead of player 1, each player's room stays loaded within player 1's area, key and story items go to player 1. |

What carries over to other games:

- **Look for a console build first.** If the console version has more local players, it is the
  reference for which arrays, loops and checks the PC build cut down. Diff the two before
  reverse engineering the PC exe alone.
- **Grow storage before you raise a player count.** Any loop that already reaches the new slots
  writes over whatever sits after the old array.
- **Keep one owner of the story.** All three keep story progress, key items and the loaded world
  tied to player 1, and pull the other players back to them. It is the same rule as the
  [second player inside one copy](#the-other-route-a-second-player-inside-one-copy) route above.
- **Enemies need a target rule.** Single-player AI chases "the player"; pick the nearest player
  explicitly.
- **Pay for the second view.** Drawing the world twice costs frame rate (the Oblivion mod quotes
  about 60 to 75 percent of normal). Offer an alternating mode.

## Ideas for agents

- Write a Nucleus handler or a PartyDeck profile for a game you've just made co-op, and ship
  it with the release.
- Port handler knowledge across: a game that works in Nucleus on Windows is a candidate for
  PartyDeck on the Steam Deck, and the reverse.
- Build a modern launcher on PartyDeck's MIT code or a GPL fork of Nucleus.
- Record each split-screen result in the game's write-up (`games/<slug>/`) with the QA level.

## Rules

Steam API emulators are for LAN play, not for joining online services. Don't publish other people's paid or early-access
handlers; write your own.
