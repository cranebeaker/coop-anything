---
name: couchlink
description: Play a co-op game, a co-op mod or any online game split-screen on one PC with Couchlink (one copy per player, each with their own screen part, controller or keyboard and mouse, and optionally their own Steam account), and make a co-op mod built here work with it through a recipe or the Couchlink SDK.
---

# Couch co-op on one PC with Couchlink

[Couchlink](https://github.com/cranebeaker/couchlink) (MIT, Windows 10 and 11) runs one copy of a game per player on
the same PC, places each copy on its own monitor or part of one, and gives each copy only its own controller (or its
own keyboard and mouse). The copies then meet through the game's own multiplayer: LAN, a co-op mod's server, or Steam.
Up to 8 players.

Use this skill when someone wants to play with friends on the same couch instead of over the internet, or when a
co-op mod made with this playbook should also work split-screen.

## Playing

1. Download the Couchlink release zip from https://github.com/cranebeaker/couchlink and run `couchlink.exe`.
2. **Library**: games with a recipe that are installed are under "On this PC". Every other installed Steam game is
   under "Try it", with safe defaults. Games Couchlink supports but you do not have are listed below.
3. **Players**: press A on a controller or Enter on a keyboard to take a seat. A keyboard and a mouse together are one
   player.
4. **Steam**: one Steam account is enough; every copy runs under it. For games that only connect players through
   Steam (online lobbies, invites), give each extra player their own Steam account from their seat card ("Steam").
   Windows asks for permission once; then that player signs in. Each account needs its own copy of the game.
5. **Screens**: drag players onto monitors, or split one screen.
6. Press Play. If a game needs a one-time setup step (a mod installer, a first launch), the Library says so before
   Play.

## Adding a game or a co-op mod

Pick the smallest one that does the job. The full guide, with examples, is the `couchlink-sdk` skill in the Couchlink
repo: https://github.com/cranebeaker/couchlink/blob/main/skills/couchlink-sdk/SKILL.md

- **A recipe** (one JSON file): how to start the game, where saves live, which single-instance checks to lift, which
  Steam mode to use, and a server to start first if the game needs one. Template variables `$SEAT`, `$PLAYER`,
  `$WIDTH`, `$HEIGHT`, `$X`, `$Y` give each copy its own arguments. Check it with
  `cargo run -q -p couchlink-nucleus --example check_recipe -- my-game.json` and share it on the Discord; published
  recipes are signed and reach every install.
- **A Couchlink-aware co-op mod**: let the recipe pass the seat and player name on the command line or in the
  environment; seat 0 hosts and every other seat joins `127.0.0.1` on your port. No input tricks are needed then.
- **Couchlink's parts in your own launcher**: the crates are MIT (`couchlink-hook` for per-copy controllers and keyboard
  and mouse, `couchlink-steam` for per-copy Steam setup, `couchlink-core` for recipes and seats), or drive the
  Couchlink service over its local WebSocket API (`docs/protocol.md` in the repo).

## Testing without people

Follow the `coop-qa` skill: no human playtests. For Couchlink this means a recipe that passes `check_recipe`, a session
started from a script against the local service (`start_session`, then wait for `running` in the `session` events), and
your mod's own bots checking that every copy joined the host.
