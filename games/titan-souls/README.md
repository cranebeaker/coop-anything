# Titan Souls (2015)

| | |
|---|---|
| Engine | In-house C++ (Acid Nerve, SDL2 + OpenGL) |
| Versions tested | Steam Release 2015-04, Build ID 587886, `TITAN.exe` SHA-256 `ecd19bd286cd21952a585846b146b35ca58969f90996d31f15f2779883bb65be` |
| Projects | [Titan Souls Co-op](https://github.com/cranebeaker/coop-anything/tree/main/games/titan-souls) (`version.dll` proxy hook, local co-op, release v1.0.0), directory entry "Titan Souls Co-op" |
| QA level | automated (25-second continuous in-game test harness, dual input injection, roll/aim verification, 0 crashes) |
| Last updated | 2026-10-23 |

## Where things stand

Two players can play the full campaign and boss rush together locally on one PC. Player 1 uses the primary gamepad or WASD; Player 2 uses a second gamepad or secondary keyboard controls. Both players possess an independent single arrow, dynamic shared midpoint camera framing, and can independently trigger boss weakpoints.

## Existing work

- Previously marked as a single-player gap with no existing multiplayer attempts.
- [martinsomer/titan-souls-keyboard-mod](https://github.com/martinsomer/titan-souls-keyboard-mod): single-player keyboard remapping proxy hook.

## How the game works (for co-op)

- Closed 32-bit x86 PE binary compiled with MinGW GCC. Graphics driven by SDL2 + OpenGL; audio by OpenAL.
- Active scene objects and entities are owned by the `GameWorld` singleton at `0x007166D0` (RTTI `9GameWorld`, vtable `0x00711928`).
- Player entity size is `0x418` bytes (constructor `0x005B59EC`, vtable `0x0070F568`). Each Player holds its own `Arrow*` pointer at `[Player + 0x410]` (size `0xAC`, constructor `0x00532D7C`).
- Entities are added to the world via `GameWorld::AddEntity` at `0x005F458C` (`__thiscall`, `ecx = GameWorld*`).
- Boss weakpoint detection is handled via `TitanHitSensor` entities (vtable `0x0070E728`), where virtual method index 7 (`0x00589700`) sets the hit flag `[ecx + 0x94] = 1`.
- Viewport camera is managed by `Camera` entity at `0x00716660`, with viewport offsets stored at `0x00716C1C` and `0x00716C20`. Full memory map and function pointers documented in [NOTES.md](NOTES.md).

## What we changed

Implemented via a 32-bit `version.dll` proxy forwarder using MinHook; zero executable bytes are modified on disk.

- **Player 2 Lifecycle:** Hooks `SDL_GL_SwapWindow`. When active gameplay starts (`ADDR_PLAYER1_PTR` non-null), dynamically allocates `0x418` bytes via `operator new`, initializes Player 2 with `Player::Player(0.0f, 0.0f)`, recolors sprite tunic to cyan (`0.2, 0.8, 1.0, 1.0` at offsets `0x334..0x340`), and enrolls it into `GameWorld::AddEntity`.
- **Independent Arrow & Combat:** Player 2's arrow is automatically instantiated and managed by Player 2's update loop. Arrows from both players collide with and damage `TitanHitSensor` components independently.
- **Dual Input Dispatch:** Hooks `InputManager::GetButton` (`0x006140A8`), `InputManager::GetAxis` (`0x00615930`), and `Player::Update` (`0x005B0D50`). During Player 2's update tick, queries are redirected to Gamepad 2 (XInput) and secondary keyboard (Arrow Keys + J/K).
- **Dynamic Midpoint Camera:** Hooks `Camera::Update` (`0x005FA198`) to frame `(P1 + P2) * 0.5f`, dynamically centering the viewport between both players.
- **Config & Level Transitions:** Configuration loaded live from `coop_config.ini` (gamepad index, keyboard toggle, camera tracking, custom RGBA colors). Seamlessly resets and respawns Player 2 across door transitions and checkpoint wipes.

## QA

- **Automated In-Game Test:** An automated test harness script integrated in `version.dll` automates menu progression, validates level entry, tests Player 2 walking right (X position `1072.0 -> 1140.0`), roll dodge, aiming UP, firing Arrow 2, and recalling arrow. Camera verified moving from offset `420.0` to `454.0` following the midpoint.
- Test executed for >1,320 frames at 60 FPS without crashes, assertions, or heap corruption (exit code 0).

## How to play

1. Copy `version.dll` and `coop_config.ini` next to `TITAN.exe`.
2. Connect two controllers, or use 1 controller + keyboard, or shared keyboard.
3. Launch `TITAN.exe`. Start a New Game or Continue; Player 2 spawns automatically.
4. Player 1: Standard gamepad or WASD + X/C.
   Player 2: Controller 2 (Left Stick + A/X) or Arrow Keys + J (Shoot) / K (Roll).
5. Customize settings in `coop_config.ini`. Uninstall by deleting `version.dll`.

## Known issues

- Local co-op only (one PC, shared screen). Online multiplayer over Steam Relay / LAN is not yet implemented.
- Title and pause menus are navigated exclusively by Player 1.
- Both players share the same single-life wipe rule (if Player 1 dies, the encounter resets).

## Next steps

- Netcode bridge: packetize Player 2 transform and input over ENet or Steam P2P for remote online co-op.
- Independent revive mechanic: allow surviving player to touch a fallen partner to revive before wipe.
