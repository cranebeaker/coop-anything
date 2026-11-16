# Release shape: what the player actually installs

Answer to "will co-op be a launcher?" — **a launcher plus a mod inside the game; the launcher alone
cannot do it.** This page fixes the shape early so no agent builds the wrong half.

## The two parts

| Part | Where it lives | What it does | Can it be skipped? |
|---|---|---|---|
| **Mod** | DLL in the game folder (proxy: `dinput8.dll` / `winmm.dll`, plus a mod folder under `data/`, which the engine reads — proven by the loose `.bk2` movies) | Second controllable body, authority, replication, transport, sync of turns and story, the in-game panel | No. All actual work happens in the game process. |
| **Launcher** | One `.exe` per OS, no installer required | Finds the install, verifies build/version and file hashes, buttons **Host** / **Join**, shows a join code, starts the game through Steam, shows logs, updates and uninstalls the mod, backs up the profile | It can be a plain script at first, but players need it. |

A launcher that only starts the game is not co-op. A mod with no launcher is unusable for a friend
who has never edited a config file.

## Why a launcher (and not "everything in game")

- This engine has no mod menu and no mod loader: there is nothing in the retail UI to hang a
  co-op menu on, and shipping a patched exe is out (Steam CEG, and we do not redistribute the game).
- Version and build talk: the engine's own join errors (`incompatible version`, `invalid
  connection params - bad level name`) must become a clear message **before** the game loads.
- The profile/save story: the harness already proved that isolating the profile matters. A launcher
  is the natural place to warn, back up and restore, instead of a support thread.
- Host/join outside the game keeps the in-game footprint small: a hotkey and a small overlay.

## First release (smallest honest product)

1. Launcher window: **Host** → shows a code; **Join** → paste code → **Play**.
2. It copies the mod DLL into the game folder, records what it added, and can remove it.
3. It starts the game through Steam (`steam://rungameid/213670` or the exe with the right env),
   with the mod's own config.
4. In game: hotkey opens a small panel (players, ping, copy code, disconnect).
5. Guest controls their own character; host owns story and combat; movement and animation are
   interpolated; a dropped guest can rejoin without restarting the host.
6. Uninstall = launcher button, and the game folder hash-verifies back to the retail state.

## Transport choice for the first release

- **Direct IP over a VPN is the first shipping target** (ZeroTier, Radmin VPN, Tailscale): the
  least code, no port forwarding, and it works whatever the engine's own netcode does.
- Steam P2P relay comes later. It needs a different app id (Spacewar 480) which conflicts with the
  game's real `steam_appid` and with anything that reads the Steam session; only worth it once the
  VPN path is proven and players complain about the extra app.

## What the player must own

Both players need **their own copy of the game**. One Steam account cannot run the same licence on
two PCs at the same time, and Steam Family Sharing does not allow two people to play the same
library simultaneously. The launcher should say this before anyone buys a VPN.

## Non-goals

- Not a split-screen tool, not a second window on the host's PC.
- Not a hosted service: no accounts, no servers of ours, no telemetry beyond a log the player can read.
- Not a cheat tool: no anti-cheat bypass, nothing that touches other players' games.

## What this decides for the agents

- Gate 5/6 (`prompts/06`, `prompts/07`) build against an interface the launcher can drive: a
  config file plus a small local control channel, never a GUI-only path.
- Gate 8 (shipping) is launcher work: install, version check, host/join, logs, uninstall, and the
  README a friend can follow without asking a question.
