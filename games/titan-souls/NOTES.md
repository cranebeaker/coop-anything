# Titan Souls — Reverse Engineering & Co-op Mod Architecture Notes

Running log and technical dossier for *Titan Souls* (Steam AppID 297130, Build 587886). All observations, derivations, and reverse-engineered seams are strictly recorded with verified addresses and structures.

---

## 1. Target Identity & Environment

| Field | Value |
|---|---|
| Title | Titan Souls (2015) |
| Platform / Store | Steam |
| App ID | 297130 |
| Depot ID | 297131 |
| Build ID | 587886 |
| Manifest | `4623628864147143610` (`appmanifest_297130.acf`) |
| Version | Release 2015-04 (PE timestamp `1428500778`: 2015-04-08 13:46:18 UTC) |
| Main Exe | `TITAN.exe` (3,264,000 bytes) |
| Architecture | x86 (32-bit PE32, MinGW GCC toolchain) |
| Image Base | `0x00400000` |
| SHA-256 | `ecd19bd286cd21952a585846b146b35ca58969f90996d31f15f2779883bb65be` |
| Engine | Acid Nerve in-house C++ ("New Engine" / "TITAN CODE"), SDL2 + OpenGL + OpenAL |
| OS Tested | Windows 11 Pro (Windows NT 10.0.26200.0, x64) |
| Sandbox Path | `<project root>/local/test-install/` |

---

## 2. Core Engine Seams & Memory Map (Verified)

### Global Singletons & Pointers
- `0x007165EC`: Active Scene / Game singleton (`Game*`).
  - `NULL` on Title Screen and Main Menu; non-null during active gameplay.
  - Cleared back to `NULL` on level exit / return to menu (`005531DF`, `005817C3`).
  - `+0x0C`: `Entity** _M_start` (std::vector of active entities)
  - `+0x10`: `Entity** _M_finish`
  - `+0x30`: In-game frame counter (`frames`)
  - `+0x34`: Death counter
  - `+0x38`: Kill counter
- `0x00716664`: Active Player entity pointer (`Player*`). `NULL` on menu and during level loading transitions.
- `0x00716660`: Active Camera entity pointer (`Camera*`).
- `0x0071665C`: Global Map / Tilemap World pointer (`World*`).
- `0x007166D0`: Global Entity Container singleton (`GameWorld*`).
  - RTTI Mangled Symbol: `9GameWorld` (`0x00709218`)
  - Vtable: `0x00711928`
  - All scene entities (Player, Arrow, Titan, Particles) reside inside `GameWorld`'s entity vectors (`[GameWorld + 4]..[GameWorld + 8]`).
  - `GameWorld::AddEntity(Entity* ent)`: `0x005F458C` (`__thiscall`, `ecx = GameWorld*`, `arg = Entity*`, returns void, cleans stack `ret 4`).
- `0x007168BC`: Active Menu singleton (`m_main` vtable `0x0070C908`, `m_selectsave` vtable `0x0070E628`).
- `0x00716C1C`: Camera Viewport Offset X (`float`).
- `0x00716C20`: Camera Viewport Offset Y (`float`).
- `0x00716C30`: Viewport Half Width (`float` = 240.0f).
- `0x00716C2C`: Viewport Half Height (`float` = 135.0f).

---

## 3. Entity Structures & Offsets

### Player Entity (`Player*`, size: `0x418` bytes = 1048 bytes)
- RTTI Vtable: `0x0070F568`
- Constructor: `Player::Player(float x, float y)` at `0x005B59EC` (`__thiscall`, `ecx = pMem`, callee cleans stack `ret 8`).
- Memory Allocator: `operator new(0x418)` at `0x0064F310` (`__cdecl`).
- Position X: `float` at `[Player + 0x18]`
- Position Y: `float` at `[Player + 0x1C]`
- Velocity X: `float` at `[Player + 0x88]`
- Velocity Y: `float` at `[Player + 0x8C]`
- Spawn Coordinate X: `float` at `[Player + 0x94]`
- Spawn Coordinate Y: `float` at `[Player + 0x98]`
- Sprite Recolor Tint:
  - Red: `float` at `[Player + 0x334]`
  - Green: `float` at `[Player + 0x338]`
  - Blue: `float` at `[Player + 0x33C]`
  - Alpha: `float` at `[Player + 0x340]`
- Roll / Dodge State: `[Player + 0x368]`
- Dedicated Arrow Pointer: `[Player + 0x410]` (`Arrow*`)

### Arrow Entity (`Arrow*`, size: `0xAC` bytes = 172 bytes)
- Instantiated automatically inside `Player::Player` via `operator new(0xAC)` and `Arrow::Arrow(0.0f, 0.0f)` at `0x00532D7C`.
- Aim direction, charging state, flight trajectory, and magnetic recall are managed directly by each `Player` instance's update loop.

### Titan Hit Sensors (`TitanHitSensor`, size: `0xA0` bytes)
- RTTI Mangled Symbol: `14TitanHitSensor` (`0x007089B4`)
- Vtable: `0x0070E728`
- Weakpoint Hit Registration: virtual function index 7 at `0x00589700`:
  Sets hit flag `byte ptr [ecx + 0x94] = 1`, registering lethal arrow impact on the titan encounter.

---

## 4. Implementation Architecture

### 1. Proxy DLL Forwarding (`version.dll`)
Intercepts the Windows loader using standard proxy exports (17 functions forward to `C:\Windows\SysWOW64\version.dll`). Initializes MinHook in `DLL_PROCESS_ATTACH`.

### 2. Dual Input Injection & Dispatch
- Hooked `InputManager::GetButton` (`0x006140A8`) and `InputManager::GetAxis` (`0x00615930`).
- Hooked `Player::Update` (`0x005B0D50`): Sets `g_pCurrentUpdatingPlayer = pThis`.
- During `Player 1->Update()`, vanilla input queries pass through to Gamepad 1 / primary keyboard.
- During `Player 2->Update()`, button queries (`shoot`, `roll`, `move_x`, `move_y`) are intercepted and fulfilled from Gamepad 2 (XInput Index 1) or secondary keyboard inputs (Arrow keys + J/K).

### 3. Dynamic Player 2 Instantiation & Container Integration
- At game frame 527 (scene load completion), `ADDR_PLAYER1_PTR` (`0x00716664`) and `ADDR_GAMEWORLD_PTR` (`0x007166D0`) are detected.
- Allocates `0x418` bytes with `operator new`.
- Calls `Player::Player(0.0f, 0.0f)`.
- Applies cyan tunic tint `(0.2, 0.8, 1.0, 1.0)` at `[Player 2 + 0x334..0x340]`.
- Enrolls Player 2 into `GameWorld` via `GameWorld::AddEntity(pGameWorld, pPlayer2)`.

### 4. Dynamic Midpoint Camera Centering
- Hooked `Camera::Update` (`0x005FA198`):
  Computes `mid_x = (P1_x + P2_x) * 0.5f` and `mid_y = (P1_y + P2_y) * 0.5f`.
  Updates viewport offset registers `0x00716C1C` and `0x00716C20` subtracting half-dimensions (240x135), keeping both players centered in the screen.

---

## 5. Verification Log Summary

| Phase | Test Description | Result | Evidence |
|---|---|---|---|
| Phase 1 | Sandbox copy & save file snapshot | PASS | `local/backup-saves/0.txt` backed up (3520 bytes); steam_appid 297130 standalone verified |
| Phase 2 | 32-bit `version.dll` proxy compilation | PASS | 17 exports forwarded, MSVC /TP compilation, MinHook initialized cleanly |
| Phase 3 | Main menu fade automation & level loading | PASS | Level reached at frame 527, running stably past frame 1320 (22+ seconds @ 60fps) |
| Phase 4 | Dual input dispatch & secondary keyboard | PASS | Gamepad 2 XInput + Keyboard fallback hooked to `GetButton`/`GetAxis` |
| Phase 5 | Player 2 spawn & Arrow 2 instantiation | PASS | `0x1FE0E278` (P1) and `0x35829CF8` (P2) + `0x358C5480` (A2) active simultaneously |
| Phase 5.5 | Automated movement, roll & aim test | PASS | P2 walked right (pos 1072 -> 1144), performed roll, aimed UP, camera tracked midpoint |
| Phase 6 | Checkpoint respawn & level sync | PASS | Dynamic re-spawn on level transition without heap leak or crash |
| Phase 7 | `coop_config.ini` dynamic loading & release packaging | PASS | Release bundle verified in `games/titan-souls/release/` |
