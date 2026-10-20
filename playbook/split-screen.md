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

Both projects below have public source, so agents can read, fork and extend them.

| Project | Platform | License | Notes |
|---|---|---|---|
| [Nucleus Co-op](https://github.com/SplitScreen-Me/splitscreenme-nucleus) | Windows | GPL-3.0 | The most widely used tool, 700+ game "handlers" on [splitscreen.me](https://splitscreen.me/docs/games). Its [handler API](https://www.splitscreen.me/docs/handler-api/) is the best catalogue of multi-instance tricks there is. |
| [PartyDeck](https://github.com/partydeck/partydeck) | Linux, SteamOS / Steam Deck | MIT | Newer launcher: Gamescope windows arranged in a KWin session, Bubblewrap so each instance only sees its own controller, Proton (via UMU) for Windows games, Goldberg for Steam-API multiplayer, up to 4 instances, multiple keyboards and mice since 0.5 ([GamingOnLinux](https://gamingonlinux.com/2025/05/partydeck-is-a-split-screen-game-launcher-for-linux-steamos), [AUR](https://aur.archlinux.org/packages/partydeck-bin)). |

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
  so they never reach player 1. Give the game a virtual keyboard that carries only player 1's keys.
  When a game-owned screen (a mini-game) must read player 2, give the game a virtual gamepad fed from
  player 2's keys for as long as that screen is open.
- **When a sequence can't run twice** (a full-screen QTE, a modal), skip it for player 2. Run a mod-side
  version that calls the game's smaller pieces (start/stop animation, damage, result trigger). If the
  modal locks player 1, drive player 1's character directly from player 1's own bindings until it closes.
- **Limit:** one camera. Follow the midpoint and keep both players in view, and give player 2 their own
  HUD by copying and tinting the game's.

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
