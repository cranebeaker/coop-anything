# Cuphead (2017)

| | |
|---|---|
| Engine | Unity 2017.4.9, Mono, Windows x64 (BepInEx 5.4.23.5, Harmony) |
| Versions tested | Steam (app 268910), buildid 20241530. `Cuphead.exe` SHA-256 `c5fffd221234ea520b9b5d545d9fff65eba497a0ce1b852334d293770d7ee02d` |
| Projects | CupHeads by Germanized ([Germanized/CupHeads](https://github.com/Germanized/CupHeads)); the Multiplayer Everything fork ([multiplayereverything/CupHeads](https://github.com/multiplayereverything/CupHeads), [v1.7.0 pre-release](https://github.com/multiplayereverything/CupHeads/releases/tag/v1.7.0)); directory entries "CupHeads" and "CupHeads (Multiplayer Everything fork)" |
| QA level | automated |
| Last updated | 2026-10-30 |

## Where things stand

Cuphead has local two-player co-op. CupHeads, by Germanized (with Sh0kr), makes it work online. Two players share the host's save, the world map, run-and-gun levels, plane levels and boss fights, and join through Steam invites with no port forwarding. The Multiplayer Everything fork (v1.7.0) adds a LAN fallback and fixes the partner's health, death and revive. Its same-PC lab with two game copies passes every step from connecting to returning to the map after a boss fight. Steam P2P between two real accounts has not been tested yet.

## Existing work

- **CupHeads** by Germanized: BepInEx 5 plugin, last release v1.6.2 on 2026-10-15. The source is public, but the repo has no license file. All the netcode and features below come from it.
- **CupHeads, Multiplayer Everything fork:** continues from 1.6.2. Changes are listed under "What we changed" and in its `CHANGELOG.md`.
- **Steam Remote Play Together:** Cuphead is listed as compatible. It streams one PC's local co-op to a friend instead of running the game on both PCs.

## How the game works (for co-op)

- **Engine:** Unity 2017.4 Mono, so the game logic is in `Cuphead_Data/Managed/Assembly-CSharp.dll` and Harmony can hook it. BepInEx 5 x64 loads it. The fork keeps a decompiled copy for reading only; `tools/audit-hooks.ps1` checks every patch target against it before each build, and it is never packaged.
- **Players:** the game already has a Player Two (couch co-op), so online play drives that slot from the network instead of adding a new character. Multiplayer is limited to two players, like the game.
- **Steam:** the game ships its own Steamworks.NET (`Assembly-CSharp-firstpass.dll`) and `steam_api64.dll`. They are too old for `SteamNetworkingSockets` or `ISteamNetworkingMessages`, so the mod uses `ISteamNetworking` P2P with `AllowP2PPacketRelay(true)`, which still goes through Valve's relay (see the [Steam networking page](../../playbook/networking-steam-spacewar.md)). Lobbies and invites run under the game's own app id.
- **Focus:** the game pauses when its window loses focus. The mod sets `Application.runInBackground` during a session; lab copies need it too.
- **Saves:** they live in `%APPDATA%\Cuphead`, and display settings in `HKCU\Software\Studio MDHR\Cuphead`. The guest plays on an in-memory mirror of the host's save, which is never written to the guest's disk.

## What we changed

From Germanized's 1.6.2 to the fork's 1.7.0 (wire protocol 4, so it still connects to 1.5.x and 1.6.x):

- **The partner's cup kept hearts its owner had lost.** Each hit is replayed on the other machine as a damage event. The game drops a replayed hit that lands while that copy is still invulnerable, so the copy could stay standing after its owner died. Now each owner also sends its health and life state, and a copy that disagrees for 1.5 s follows the owner. Tested by the lab's guest-death checks and by the `health` mutant.
- **Revives showed on one screen only.** A ghost floating off the top of the screen, or a parry on the partner's ghost, revived the player only where it was seen. Revives now go through the host, so both screens agree. Tested by `revive` and `parry-revive` and by the `revive` mutant.
- **A revive for a player who was already standing reset their health to 1.** Those revives are now ignored.
- **LAN or direct-IP fallback:** the same packets go over TCP port 27140. It is for play on one network or a VPN, and for the lab, since one Steam account can't P2P to itself.
- **Packaging:** the Electron installer is gone. The mod ships as a mod-only ZIP, with BepInEx in its own pinned release.

## QA

- `scripts/setup-lab.ps1` makes two copies of the install (hardlinks plus a junction to `Cuphead_Data`), each with its own `lab-saves/`. It backs up and restores the registry key and blocks achievements and stats.
- `scripts/run-lab.ps1` starts both copies minimized as 640x360 windows at 30 fps and BelowNormal priority, with no input or focus. A bot drives both sides over the LAN fallback, and `tests/verify-lab.py` reads both logs. Its checks: no stalls on either copy, connecting, both reaching the world map, each cup moving on the map and in the level, both entering The Root Pack together, boss HP reaching the guest, three guest hits and death, revive with a heart, a second death, revive by parry, and both returning to the map.
- `scripts/mutants.ps1` drops one kind of packet per run: InputFrame, PlayerState, boss state, scene packets, health, or revive packets. Each run has to fail the matching check, and all six do. A seventh drops only the damage events, and the health backup still kills and revives the guest correctly.
- Last result: 15/15 on 2026-10-27 and again on 2026-10-29, on two different PCs. All seven mutants behaved as expected on 2026-10-27.

## How to play

1. Unzip `BepInEx_win_x64_5.4.23.5.zip` from the fork's [`bepinex-5.4.23.5` release](https://github.com/multiplayereverything/CupHeads/releases/tag/bepinex-5.4.23.5) into `<game dir>`.
2. Unzip `CupHeads-BepInEx-<version>.zip` from the [latest release](https://github.com/multiplayereverything/CupHeads/releases) into the same folder.
3. Start Cuphead from Steam. The host picks `MULTIPLAYER`, then `HOST A GAME`, then `INVITE A FRIEND`. The friend accepts the Steam invite, and the host picks a save and presses `START!`. Both players need the same mod version.

## Known issues

- You need two different Steam accounts. One account can't connect to itself, so on one PC use `HOST ON LAN`.
- Up to two players. More are tracked, but many scenes assume two.
- Ms. Chalice needs the DLC on both PCs.

## Next steps

- Run a real session with two Steam accounts over the internet, then move the fork's release out of pre-release.
- Extend the lab tour to shops, equip cards, pause, plane levels and the Dice Palace. They come from the original mod and the lab doesn't cover them yet.
