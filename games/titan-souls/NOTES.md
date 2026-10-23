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
| Phase 8 | UDP Socket layer & protocol handshake | PASS | Winsock2 non-blocking socket bound; instant handshake & ACK between Host (7777) and Client (7778) |
| Phase 9 | Bidirectional 60Hz state synchronization | PASS | 58-byte `NetPlayerSyncPacket` streamed smoothly; Client Y (1176->1129) mirrored to Host puppet (1176->1136.5) |
| Phase 10 | Independent client camera & zero crash verify | PASS | Client camera followed local player to Y=770.2; Host camera tracked midpoint; 0 crashes across dual instances |

---

## 6. Networked Multiplayer Protocol & Wire Format

### Transport Layer
- **Protocol:** Non-blocking UDP via Winsock2 (`ws2_32.lib`).
- **Magic:** `0x4E544954` (`'TITN'`).
- **Version:** `0x0001` (v1).
- **Packet Rate:** 60 Hz tick synchronized with engine render loop (`Hooked_SDL_GL_SwapWindow`).
- **Bandwidth:** ~3.5 KB/s per client connection.

### Packet Structures (`#pragma pack(push, 1)`)

#### 1. Header (`NetHeader`, size: 8 bytes)
```c
typedef struct {
    uint32_t magic;       // 0x4E544954 ('TITN')
    uint8_t  type;        // 0x01 = Handshake, 0x02 = Ack, 0x03 = Heartbeat, 0x04 = Sync, 0x05 = Disconnect
    uint8_t  version;     // 1
    uint16_t sequence;    // Monotonically increasing sequence number
} NetHeader;
```

#### 2. Handshake (`NetHandshakePacket`, size: 10 bytes)
```c
typedef struct {
    NetHeader header;     // type = 0x01
    uint16_t client_port; // Local UDP port
} NetHandshakePacket;
```

#### 3. Handshake ACK (`NetHandshakeAckPacket`, size: 12 bytes)
```c
typedef struct {
    NetHeader header;     // type = 0x02
    uint8_t   assigned_id;// Player ID (2)
    uint8_t   status;     // 0 = Success
    uint16_t  reserved;
} NetHandshakeAckPacket;
```

#### 4. Player State Sync (`NetPlayerSyncPacket`, size: 58 bytes)
```c
typedef struct {
    NetHeader header;     // type = 0x04
    uint32_t  frame;      // Host / Client frame count
    float     pos_x;      // Player X coordinate
    float     pos_y;      // Player Y coordinate
    float     vel_x;      // Velocity X
    float     vel_y;      // Velocity Y
    uint32_t  state_flags;// Roll / aim / action bitfield
    float     input_move_x;
    float     input_move_y;
    uint8_t   input_roll;
    uint8_t   input_shoot;
    uint8_t   arrow_active; // 1 if arrow in flight / on ground
    uint8_t   pad;
    float     arrow_x;    // Arrow position X
    float     arrow_y;    // Arrow position Y
    float     arrow_angle;// Arrow aim / rotation angle
} NetPlayerSyncPacket;
```

### Interpolation & Puppet Smoothing
- On the receiving peer, the remote player entity is represented by Player 2.
- To eliminate network packet jitter, incoming coordinates are smoothed each frame via linear interpolation:
  `pos_x = pos_x + (target_x - pos_x) * 0.35f;`
  `pos_y = pos_y + (target_y - pos_y) * 0.35f;`
- Velocity and roll animation states are mirrored directly to ensure seamless movement animations.

---

## 7. Dual-Instance Live Verification Traces

Host instance running on port 7777; Client instance running on port 7778:

### Host Log (`test-install/coop_log.txt`)
```text
=== Titan Souls Co-op Mod Initializing ===
Configuration loaded from coop_config.ini:
  QuickStart: 1 | RunAutomatedTest: 0 | MidpointCamera: 1
  Network: Enabled=1 | Role=HOST | BindPort=7777 | Remote=127.0.0.1:7778 | IndepCam=0
[Net] Initialized. Mode: HOST | Bound: 0.0.0.0:7777 | Target: 127.0.0.1:7778
All hooks installed successfully (SwapWindow, GetButton, GetAxis, Player::Update, Camera::Update).
[Frame 60] [MENU] P1: 0x00000000 | QuickStart: 1 | NetEnabled: 1 (Role: 1)
[Net] Handshake received from 127.0.0.1:7778! Sending ACK...
[Frame 201] Instantiating Player 2 entity near (1048.00, 1176.00)...
>>> PLAYER 2 SUCCESSFULLY SPAWNED! <<<
   Player 1: 0x204C0008 | Pos: (1048.00, 1176.00)
   Player 2: 0x361FCA10 | Pos: (1072.00, 1176.00)
   Player 2 Arrow: 0x1C8ECC48 | Color: RGBA(0.20, 0.80, 1.00, 1.00)
[F240] Net:[HOST state=2 TX=44 RX=4]   | Cam:(420.0,816.0) | P1:(1048.0,1176.0) | P2:(1072.0,1176.0)
[F360] Net:[HOST state=2 TX=166 RX=53] | Cam:(420.0,816.0) | P1:(1048.0,1176.0) | P2:(1062.4,1176.0)
[F420] Net:[HOST state=2 TX=227 RX=114]| Cam:(420.0,805.0) | P1:(1048.0,1176.0) | P2:(1062.4,1153.1)
[F480] Net:[HOST state=2 TX=288 RX=175]| Cam:(420.0,798.0) | P1:(1048.0,1176.0) | P2:(1062.4,1136.5)
```

### Client Log (`test-client/coop_log.txt`)
```text
=== Titan Souls Co-op Mod Initializing ===
Configuration loaded from coop_config.ini:
  QuickStart: 1 | RunAutomatedTest: 1 | MidpointCamera: 1
  Network: Enabled=1 | Role=CLIENT | BindPort=7778 | Remote=127.0.0.1:7777 | IndepCam=1
[Net] Initialized. Mode: CLIENT | Bound: 0.0.0.0:7778 | Target: 127.0.0.1:7777
All hooks installed successfully (SwapWindow, GetButton, GetAxis, Player::Update, Camera::Update).
[Net] Handshake ACK received! Connected to Host at 127.0.0.1:7777!
[Frame 201] Instantiating Player 2 entity near (1048.00, 1176.00)...
>>> PLAYER 2 SUCCESSFULLY SPAWNED! <<<
   Player 1: 0x1CB8E1A0 | Pos: (1048.00, 1176.00)
   Player 2: 0x30E4FB98 | Pos: (1072.00, 1176.00)
   Player 2 Arrow: 0x1BE25A60 | Color: RGBA(1.00, 0.50, 0.20, 1.00)
[F240] Net:[CLIENT state=2 TX=45 RX=156] | Cam:(415.1,816.0) | P1:(1048.0,1176.0) | P2:(1062.4,1176.0)
[F300] Net:[CLIENT state=2 TX=106 RX=217]| Cam:(415.1,816.0) | P1:(1048.0,1176.0) | P2:(1062.4,1176.0)
[F360] Net:[CLIENT state=2 TX=167 RX=278]| Cam:(415.1,770.2) | P1:(1048.0,1129.0) | P2:(1062.4,1176.0)
```
