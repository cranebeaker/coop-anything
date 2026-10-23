#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "dllproxy.h"
#include "MinHook.h"

#ifndef SDLCALL
#define SDLCALL __cdecl
#endif
struct SDL_Window;
typedef struct SDL_Window SDL_Window;

// ============================================================================
// Engine Pointers & Offsets (Titan Souls 2015 Steam Build 587886)
// ============================================================================
#define ADDR_GAME_PTR         0x007165EC  // Game / Scene singleton
#define ADDR_PLAYER1_PTR      0x00716664  // Player 1 entity pointer
#define ADDR_CAMERA_PTR       0x00716660  // Camera entity pointer
#define ADDR_WORLD_PTR        0x0071665C  // World map pointer
#define ADDR_GAMEWORLD_PTR    0x007166D0  // GameWorld entity container pointer
#define ADDR_CAM_OFFSET_X     0x00716C1C  // Camera float X offset
#define ADDR_CAM_OFFSET_Y     0x00716C20  // Camera float Y offset
#define ADDR_HALF_W           0x00716C30  // Half viewport width
#define ADDR_HALF_H           0x00716C2C  // Half viewport height

// Function Addresses
#define ADDR_OPERATOR_NEW     0x0064F310  // void* __cdecl operator new(size_t)
#define ADDR_PLAYER_CTOR      0x005B59EC  // Player::Player(float x, float y) __thiscall
#define ADDR_PLAYER_UPDATE    0x005B0D50  // Player::Update() __thiscall
#define ADDR_CAMERA_UPDATE    0x005FA198  // Camera::Update() __thiscall
#define ADDR_CAMERA_ADDTARGET 0x005FAA58  // Camera::AddTarget(float x, float y) __thiscall
#define ADDR_WORLD_ADD_ENTITY 0x005F458C  // World::AddEntity(Entity*) __thiscall
#define ADDR_GET_BUTTON       0x006140A8  // InputManager::GetButton(string) __cdecl
#define ADDR_GET_AXIS         0x00615930  // InputManager::GetAxis(string) __cdecl
#define ADDR_LOAD_GAME        0x005CD048  // Game::LoadGame(int slot) __thiscall

// Entity field offsets
#define OFF_POS_X             0x18
#define OFF_POS_Y             0x1C
#define OFF_VEL_X             0x88
#define OFF_VEL_Y             0x8C
#define OFF_COLOR_R           0x334
#define OFF_COLOR_G           0x338
#define OFF_COLOR_B           0x33C
#define OFF_COLOR_A           0x340
#define OFF_PLAYER_ROLL       0x368
#define OFF_PLAYER_ARROW      0x410

// Typedefs for engine functions
typedef void* (__cdecl *OperatorNew_t)(size_t size);
typedef void (__thiscall *PlayerCtor_t)(void* pThis, float x, float y);
typedef void (__thiscall *WorldAddEntity_t)(void* pWorld, void* pEntity);
typedef void (__thiscall *PlayerUpdate_t)(void* pThis);
typedef void (__thiscall *CameraUpdate_t)(void* pThis);
typedef void (__thiscall *CameraAddTarget_t)(void* pThis, float x, float y);
typedef char (__cdecl *GetButton_t)(const void* pStr);
typedef float (__cdecl *GetAxis_t)(const void* pStr);
typedef void (SDLCALL *SDL_GL_SwapWindow_t)(SDL_Window*);
typedef void (__thiscall *LoadGame_t)(void* pGameObj);

// Original function pointers & engine callables
static OperatorNew_t       g_fnOperatorNew       = (OperatorNew_t)ADDR_OPERATOR_NEW;
static PlayerCtor_t        g_fnPlayerCtor        = (PlayerCtor_t)ADDR_PLAYER_CTOR;
static WorldAddEntity_t    g_fnWorldAddEntity    = (WorldAddEntity_t)ADDR_WORLD_ADD_ENTITY;
static CameraAddTarget_t   g_fnCameraAddTarget   = (CameraAddTarget_t)ADDR_CAMERA_ADDTARGET;

static PlayerUpdate_t      g_orig_Player_Update  = NULL;
static CameraUpdate_t      g_orig_Camera_Update  = NULL;
static GetButton_t         g_orig_GetButton      = NULL;
static GetAxis_t           g_orig_GetAxis        = NULL;
static SDL_GL_SwapWindow_t g_orig_SwapWindow     = NULL;

// ============================================================================
// Network Protocol Definition
// ============================================================================
#define NET_MAGIC             0x4E544954  // 'TITN' (Titan Souls Network)
#define NET_PROTOCOL_VERSION  1

#define NET_PKT_HANDSHAKE     1
#define NET_PKT_HANDSHAKE_ACK 2
#define NET_PKT_HEARTBEAT     3
#define NET_PKT_PLAYER_SYNC   4
#define NET_PKT_DISCONNECT    5

#define NET_STATE_DISCONNECTED 0
#define NET_STATE_CONNECTING   1
#define NET_STATE_CONNECTED    2

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;       // NET_MAGIC
    uint8_t  version;     // NET_PROTOCOL_VERSION
    uint8_t  pktType;     // NET_PKT_*
    uint16_t senderRole;  // 1 = Host, 2 = Client
    uint32_t sequence;    // Monotonic sequence number
} NetHeader;

typedef struct {
    NetHeader header;
    char      tag[16];
} NetHandshakePacket;

typedef struct {
    NetHeader header;
    uint32_t  status;     // 1 = accepted
} NetHandshakeAckPacket;

typedef struct {
    NetHeader header;
    uint32_t  timestamp;
} NetHeartbeatPacket;

typedef struct {
    NetHeader header;
    // Player 1 transform & velocity
    float    pos_x;
    float    pos_y;
    float    vel_x;
    float    vel_y;
    // Arrow transform & state
    float    arrow_x;
    float    arrow_y;
    float    arrow_vel_x;
    float    arrow_vel_y;
    float    arrow_angle;
    uint8_t  has_arrow;   // 1 if in quiver, 0 if loose/fired
    // Inputs & actions
    float    input_x;     // Move axis X
    float    input_y;     // Move axis Y
    uint8_t  btn_roll;
    uint8_t  btn_shoot;
    uint8_t  is_alive;
    uint8_t  pad;
} NetPlayerSyncPacket;
#pragma pack(pop)

// ============================================================================
// Configuration & State
// ============================================================================
static BOOL  g_quickStart = TRUE;
static BOOL  g_runAutomatedTest = FALSE;
static BOOL  g_midpointCamera = TRUE;
static int   g_p2GamepadIndex = 1;
static BOOL  g_keyboard2Enabled = TRUE;
static float g_p2ColorR = 0.2f;
static float g_p2ColorG = 0.8f;
static float g_p2ColorB = 1.0f;
static float g_p2ColorA = 1.0f;

// Network Configuration
static BOOL  g_netEnabled = FALSE;
static int   g_netRole = 1; // 1 = Host, 2 = Client
static int   g_netBindPort = 7777;
static char  g_netRemoteIP[64] = "127.0.0.1";
static int   g_netRemotePort = 7778;
static BOOL  g_netIndependentCamera = TRUE;

// Network Runtime State
static SOCKET g_netSocket = INVALID_SOCKET;
static struct sockaddr_in g_netRemoteAddr;
static int    g_netState = NET_STATE_DISCONNECTED;
static uint32_t g_netSeqOut = 0;
static uint32_t g_netLastSeqIn = 0;
static DWORD  g_netLastRecvTick = 0;
static DWORD  g_netLastSendTick = 0;
static uint32_t g_netPacketsSent = 0;
static uint32_t g_netPacketsRecv = 0;
static NetPlayerSyncPacket g_remoteSyncState;
static BOOL   g_hasRemoteSync = FALSE;

// Global Engine State
static void* g_pPlayer1 = NULL;
static void* g_pPlayer2 = NULL;
static void* g_pCurrentUpdatingPlayer = NULL;
static int   g_frameCount = 0;
static int   g_inGameFrames = 0;
static BOOL  g_quickStartTriggered = FALSE;

// Player 1 Local Inputs
static float g_p1_axis_x = 0.0f;
static float g_p1_axis_y = 0.0f;
static char  g_p1_btn_shoot = 0;
static char  g_p1_btn_roll = 0;

// Player 2 Local / Remote Inputs
static float g_p2_axis_x = 0.0f;
static float g_p2_axis_y = 0.0f;
static char  g_p2_btn_shoot = 0;
static char  g_p2_btn_roll = 0;

// XInput dynamic binding
typedef struct _XINPUT_GAMEPAD {
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD;

typedef struct _XINPUT_STATE {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

typedef DWORD (WINAPI *XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);
static XInputGetState_t g_pfnXInputGetState = NULL;

static void LogMsg(const char* fmt, ...) {
    FILE* f = fopen("coop_log.txt", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fclose(f);
    }
}

static BOOL IsReadable(const void* p, size_t size) {
    if (!p) return FALSE;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return FALSE;
    if (mbi.State != MEM_COMMIT) return FALSE;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return FALSE;
    return TRUE;
}

static BOOL IsGameWindowActive(void) {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return (pid != 0 && pid == GetCurrentProcessId());
}

static const char* GetStdStringCStr(const void* pStr) {
    if (!pStr || !IsReadable(pStr, sizeof(void*))) return "";
    const char* cstr = *(const char**)pStr;
    if (!cstr || !IsReadable(cstr, 1)) return "";
    return cstr;
}

// ============================================================================
// Network Subsystem Implementation
// ============================================================================
static void Net_Init(void) {
    if (!g_netEnabled) return;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LogMsg("[Net] WSAStartup failed: %d\n", WSAGetLastError());
        return;
    }

    g_netSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_netSocket == INVALID_SOCKET) {
        LogMsg("[Net] socket() creation failed: %d\n", WSAGetLastError());
        return;
    }

    // Set socket to non-blocking
    u_long nonblock = 1;
    ioctlsocket(g_netSocket, FIONBIO, &nonblock);

    // Bind local socket to INADDR_ANY
    struct sockaddr_in localAddr;
    ZeroMemory(&localAddr, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons((u_short)g_netBindPort);

    if (bind(g_netSocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        LogMsg("[Net] bind() on port %d failed: %d\n", g_netBindPort, WSAGetLastError());
        closesocket(g_netSocket);
        g_netSocket = INVALID_SOCKET;
        return;
    }

    // Prepare remote target address
    ZeroMemory(&g_netRemoteAddr, sizeof(g_netRemoteAddr));
    g_netRemoteAddr.sin_family = AF_INET;
    g_netRemoteAddr.sin_addr.s_addr = inet_addr(g_netRemoteIP);
    g_netRemoteAddr.sin_port = htons((u_short)g_netRemotePort);

    g_netState = (g_netRole == 2) ? NET_STATE_CONNECTING : NET_STATE_DISCONNECTED;
    LogMsg("[Net] Initialized. Mode: %s | Bound: 0.0.0.0:%d | Target: %s:%d\n",
           (g_netRole == 1 ? "HOST" : "CLIENT"), g_netBindPort, g_netRemoteIP, g_netRemotePort);
}

static void Net_Shutdown(void) {
    if (g_netSocket != INVALID_SOCKET) {
        if (g_netState == NET_STATE_CONNECTED) {
            NetHeader bye;
            bye.magic = NET_MAGIC;
            bye.version = NET_PROTOCOL_VERSION;
            bye.pktType = NET_PKT_DISCONNECT;
            bye.senderRole = (uint16_t)g_netRole;
            bye.sequence = ++g_netSeqOut;
            sendto(g_netSocket, (const char*)&bye, sizeof(bye), 0,
                   (struct sockaddr*)&g_netRemoteAddr, sizeof(g_netRemoteAddr));
        }
        closesocket(g_netSocket);
        g_netSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

static void Net_PollIncoming(void) {
    if (!g_netEnabled || g_netSocket == INVALID_SOCKET) return;

    char recvBuf[256];
    struct sockaddr_in senderAddr;
    int senderLen = sizeof(senderAddr);

    while (1) {
        int bytes = recvfrom(g_netSocket, recvBuf, sizeof(recvBuf), 0,
                             (struct sockaddr*)&senderAddr, &senderLen);
        if (bytes <= 0) break; // Non-blocking: no more packets ready

        if ((size_t)bytes < sizeof(NetHeader)) continue;

        NetHeader* hdr = (NetHeader*)recvBuf;
        if (hdr->magic != NET_MAGIC || hdr->version != NET_PROTOCOL_VERSION) continue;

        g_netPacketsRecv++;
        g_netLastRecvTick = GetTickCount();

        // 1. Handshake Packet (Client -> Host)
        if (hdr->pktType == NET_PKT_HANDSHAKE) {
            if (g_netRole == 1) { // Host receives handshake
                g_netRemoteAddr = senderAddr; // Lock in client address
                g_netState = NET_STATE_CONNECTED;
                LogMsg("[Net] Handshake received from %s:%d! Sending ACK...\n",
                       inet_ntoa(senderAddr.sin_addr), ntohs(senderAddr.sin_port));

                NetHandshakeAckPacket ack;
                ack.header.magic = NET_MAGIC;
                ack.header.version = NET_PROTOCOL_VERSION;
                ack.header.pktType = NET_PKT_HANDSHAKE_ACK;
                ack.header.senderRole = 1;
                ack.header.sequence = ++g_netSeqOut;
                ack.status = 1;

                sendto(g_netSocket, (const char*)&ack, sizeof(ack), 0,
                       (struct sockaddr*)&g_netRemoteAddr, sizeof(g_netRemoteAddr));
                g_netPacketsSent++;
            }
        }
        // 2. Handshake ACK Packet (Host -> Client)
        else if (hdr->pktType == NET_PKT_HANDSHAKE_ACK) {
            if (g_netRole == 2) { // Client receives ACK
                g_netState = NET_STATE_CONNECTED;
                LogMsg("[Net] Handshake ACK received! Connected to Host at %s:%d!\n",
                       inet_ntoa(senderAddr.sin_addr), ntohs(senderAddr.sin_port));
            }
        }
        // 3. Heartbeat Packet
        else if (hdr->pktType == NET_PKT_HEARTBEAT) {
            // Heartbeat received, peer is alive
        }
        // 4. Player State Sync Packet
        else if (hdr->pktType == NET_PKT_PLAYER_SYNC) {
            if ((size_t)bytes >= sizeof(NetPlayerSyncPacket)) {
                NetPlayerSyncPacket* sync = (NetPlayerSyncPacket*)recvBuf;
                // Discard stale out-of-order UDP packets
                if (sync->header.sequence > g_netLastSeqIn || (g_netLastSeqIn - sync->header.sequence > 10000)) {
                    g_netLastSeqIn = sync->header.sequence;
                    g_remoteSyncState = *sync;
                    g_hasRemoteSync = TRUE;
                    if (g_netState != NET_STATE_CONNECTED) {
                        g_netState = NET_STATE_CONNECTED;
                    }
                }
            }
        }
        // 5. Disconnect Packet
        else if (hdr->pktType == NET_PKT_DISCONNECT) {
            LogMsg("[Net] Peer sent disconnect signal.\n");
            g_netState = (g_netRole == 2) ? NET_STATE_CONNECTING : NET_STATE_DISCONNECTED;
            g_hasRemoteSync = FALSE;
        }
    }
}

static void Net_BroadcastLocalPlayer(void) {
    if (!g_netEnabled || g_netSocket == INVALID_SOCKET) return;

    DWORD now = GetTickCount();

    // Client connection handshake retry loop (every 500ms)
    if (g_netRole == 2 && g_netState != NET_STATE_CONNECTED) {
        if (now - g_netLastSendTick >= 500) {
            g_netLastSendTick = now;
            NetHandshakePacket pkt;
            pkt.header.magic = NET_MAGIC;
            pkt.header.version = NET_PROTOCOL_VERSION;
            pkt.header.pktType = NET_PKT_HANDSHAKE;
            pkt.header.senderRole = 2;
            pkt.header.sequence = ++g_netSeqOut;
            strcpy_s(pkt.tag, sizeof(pkt.tag), "CLIENT_JOIN");

            sendto(g_netSocket, (const char*)&pkt, sizeof(pkt), 0,
                   (struct sockaddr*)&g_netRemoteAddr, sizeof(g_netRemoteAddr));
            g_netPacketsSent++;
        }
        return;
    }

    if (g_netState != NET_STATE_CONNECTED) return;

    // Send local Player 1 state
    void* pP1 = *(void**)ADDR_PLAYER1_PTR;
    if (pP1 && IsReadable(pP1, 0x418)) {
        NetPlayerSyncPacket pkt;
        ZeroMemory(&pkt, sizeof(pkt));
        pkt.header.magic = NET_MAGIC;
        pkt.header.version = NET_PROTOCOL_VERSION;
        pkt.header.pktType = NET_PKT_PLAYER_SYNC;
        pkt.header.senderRole = (uint16_t)g_netRole;
        pkt.header.sequence = ++g_netSeqOut;

        pkt.pos_x = *(float*)((uintptr_t)pP1 + OFF_POS_X);
        pkt.pos_y = *(float*)((uintptr_t)pP1 + OFF_POS_Y);
        pkt.vel_x = *(float*)((uintptr_t)pP1 + OFF_VEL_X);
        pkt.vel_y = *(float*)((uintptr_t)pP1 + OFF_VEL_Y);

        void* pArrow = *(void**)((uintptr_t)pP1 + OFF_PLAYER_ARROW);
        if (pArrow && IsReadable(pArrow, 0xAC)) {
            pkt.arrow_x = *(float*)((uintptr_t)pArrow + OFF_POS_X);
            pkt.arrow_y = *(float*)((uintptr_t)pArrow + OFF_POS_Y);
            pkt.arrow_vel_x = *(float*)((uintptr_t)pArrow + OFF_VEL_X);
            pkt.arrow_vel_y = *(float*)((uintptr_t)pArrow + OFF_VEL_Y);
            pkt.arrow_angle = *(float*)((uintptr_t)pArrow + 0x24);

            float dx = pkt.arrow_x - pkt.pos_x;
            float dy = pkt.arrow_y - pkt.pos_y;
            pkt.has_arrow = (dx * dx + dy * dy < 25.0f) ? 1 : 0;
        }

        pkt.input_x = g_p1_axis_x;
        pkt.input_y = g_p1_axis_y;
        pkt.btn_roll = (uint8_t)g_p1_btn_roll;
        pkt.btn_shoot = (uint8_t)g_p1_btn_shoot;
        pkt.is_alive = 1;

        sendto(g_netSocket, (const char*)&pkt, sizeof(pkt), 0,
               (struct sockaddr*)&g_netRemoteAddr, sizeof(g_netRemoteAddr));
        g_netPacketsSent++;
    }

    // Heartbeat every 60 frames (1 second)
    if (g_frameCount % 60 == 0) {
        NetHeartbeatPacket hb;
        hb.header.magic = NET_MAGIC;
        hb.header.version = NET_PROTOCOL_VERSION;
        hb.header.pktType = NET_PKT_HEARTBEAT;
        hb.header.senderRole = (uint16_t)g_netRole;
        hb.header.sequence = ++g_netSeqOut;
        hb.timestamp = now;

        sendto(g_netSocket, (const char*)&hb, sizeof(hb), 0,
               (struct sockaddr*)&g_netRemoteAddr, sizeof(g_netRemoteAddr));
        g_netPacketsSent++;
    }
}

static void Net_UpdatePuppet(void) {
    if (!g_netEnabled || !g_hasRemoteSync) return;
    if (!g_pPlayer2 || !IsReadable(g_pPlayer2, 0x418)) return;

    // Apply received remote inputs
    g_p2_axis_x = g_remoteSyncState.input_x;
    g_p2_axis_y = g_remoteSyncState.input_y;
    g_p2_btn_roll = (char)g_remoteSyncState.btn_roll;
    g_p2_btn_shoot = (char)g_remoteSyncState.btn_shoot;

    // Apply position convergence / smoothing
    float curr_x = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
    float curr_y = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);

    float dx = g_remoteSyncState.pos_x - curr_x;
    float dy = g_remoteSyncState.pos_y - curr_y;
    float distSq = dx * dx + dy * dy;

    if (distSq > 40000.0f) { // Snap if > 200px apart (initial spawn / room transition)
        *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X) = g_remoteSyncState.pos_x;
        *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y) = g_remoteSyncState.pos_y;
    } else {
        // Smooth linear interpolation (0.4f convergence per frame)
        *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X) = curr_x + dx * 0.4f;
        *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y) = curr_y + dy * 0.4f;
    }

    *(float*)((uintptr_t)g_pPlayer2 + OFF_VEL_X) = g_remoteSyncState.vel_x;
    *(float*)((uintptr_t)g_pPlayer2 + OFF_VEL_Y) = g_remoteSyncState.vel_y;

    // Arrow synchronization
    void* pArrow = *(void**)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ARROW);
    if (pArrow && IsReadable(pArrow, 0xAC)) {
        if (!g_remoteSyncState.has_arrow) {
            float curr_ax = *(float*)((uintptr_t)pArrow + OFF_POS_X);
            float curr_ay = *(float*)((uintptr_t)pArrow + OFF_POS_Y);
            float adx = g_remoteSyncState.arrow_x - curr_ax;
            float ady = g_remoteSyncState.arrow_y - curr_ay;
            if (adx * adx + ady * ady > 40000.0f) {
                *(float*)((uintptr_t)pArrow + OFF_POS_X) = g_remoteSyncState.arrow_x;
                *(float*)((uintptr_t)pArrow + OFF_POS_Y) = g_remoteSyncState.arrow_y;
            } else {
                *(float*)((uintptr_t)pArrow + OFF_POS_X) = curr_ax + adx * 0.5f;
                *(float*)((uintptr_t)pArrow + OFF_POS_Y) = curr_ay + ady * 0.5f;
            }
            *(float*)((uintptr_t)pArrow + OFF_VEL_X) = g_remoteSyncState.arrow_vel_x;
            *(float*)((uintptr_t)pArrow + OFF_VEL_Y) = g_remoteSyncState.arrow_vel_y;
            *(float*)((uintptr_t)pArrow + 0x24) = g_remoteSyncState.arrow_angle;
        }
    }
}

// ============================================================================
// Configuration Loader
// ============================================================================
static void LoadConfig(void) {
    char iniPath[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, iniPath);
    strcat_s(iniPath, MAX_PATH, "\\coop_config.ini");

    g_quickStart       = GetPrivateProfileIntA("General", "QuickStart", 1, iniPath) != 0;
    g_runAutomatedTest = GetPrivateProfileIntA("General", "RunAutomatedTest", 0, iniPath) != 0;
    g_midpointCamera   = GetPrivateProfileIntA("General", "MidpointCamera", 1, iniPath) != 0;
    g_p2GamepadIndex   = GetPrivateProfileIntA("Input", "GamepadIndex", 1, iniPath);
    g_keyboard2Enabled = GetPrivateProfileIntA("Input", "Keyboard2Enabled", 1, iniPath) != 0;

    // Network settings
    g_netEnabled           = GetPrivateProfileIntA("Network", "Enabled", 0, iniPath) != 0;
    g_netRole              = GetPrivateProfileIntA("Network", "Role", 1, iniPath);
    g_netBindPort          = GetPrivateProfileIntA("Network", "BindPort", (g_netRole == 1 ? 7777 : 7778), iniPath);
    g_netRemotePort        = GetPrivateProfileIntA("Network", "RemotePort", (g_netRole == 1 ? 7778 : 7777), iniPath);
    g_netIndependentCamera = GetPrivateProfileIntA("Network", "IndependentCamera", 1, iniPath) != 0;
    GetPrivateProfileStringA("Network", "RemoteIP", "127.0.0.1", g_netRemoteIP, sizeof(g_netRemoteIP), iniPath);

    char buf[64];
    if (GetPrivateProfileStringA("Player2", "ColorR", "0.2", buf, sizeof(buf), iniPath)) g_p2ColorR = (float)atof(buf);
    if (GetPrivateProfileStringA("Player2", "ColorG", "0.8", buf, sizeof(buf), iniPath)) g_p2ColorG = (float)atof(buf);
    if (GetPrivateProfileStringA("Player2", "ColorB", "1.0", buf, sizeof(buf), iniPath)) g_p2ColorB = (float)atof(buf);
    if (GetPrivateProfileStringA("Player2", "ColorA", "1.0", buf, sizeof(buf), iniPath)) g_p2ColorA = (float)atof(buf);

    LogMsg("Configuration loaded from %s:\n", iniPath);
    LogMsg("  QuickStart: %d | RunAutomatedTest: %d | MidpointCamera: %d\n", g_quickStart, g_runAutomatedTest, g_midpointCamera);
    LogMsg("  Network: Enabled=%d | Role=%s | BindPort=%d | Remote=%s:%d | IndepCam=%d\n",
           g_netEnabled, (g_netRole == 1 ? "HOST" : "CLIENT"), g_netBindPort, g_netRemoteIP, g_netRemotePort, g_netIndependentCamera);
}

static void PollLocalPlayer2Inputs(void) {
    if (g_netEnabled) return; // In network mode, Player 2 is driven by Net_UpdatePuppet

    g_p2_axis_x = 0.0f;
    g_p2_axis_y = 0.0f;
    g_p2_btn_shoot = 0;
    g_p2_btn_roll = 0;

    // 1. Gamepad 2 via XInput
    if (g_pfnXInputGetState && g_p2GamepadIndex >= 0) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(state));
        if (g_pfnXInputGetState(g_p2GamepadIndex, &state) == ERROR_SUCCESS) {
            SHORT lx = state.Gamepad.sThumbLX;
            SHORT ly = state.Gamepad.sThumbLY;
            if (abs(lx) > 7849) g_p2_axis_x = (float)lx / 32767.0f;
            if (abs(ly) > 7849) g_p2_axis_y = -(float)ly / 32767.0f;
            if (state.Gamepad.wButtons & 0x0001) g_p2_axis_y = -1.0f;
            if (state.Gamepad.wButtons & 0x0002) g_p2_axis_y = 1.0f;
            if (state.Gamepad.wButtons & 0x0004) g_p2_axis_x = -1.0f;
            if (state.Gamepad.wButtons & 0x0008) g_p2_axis_x = 1.0f;
            if (state.Gamepad.wButtons & (0x1000 | 0x2000)) g_p2_btn_roll = 1;
            if ((state.Gamepad.wButtons & 0x4000) || state.Gamepad.bRightTrigger > 30) g_p2_btn_shoot = 1;
        }
    }

    // 2. Secondary Keyboard Controls (Arrow keys, J/RShift=shoot, K/RCtrl/Numpad0=roll)
    if (g_keyboard2Enabled && IsGameWindowActive()) {
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  g_p2_axis_x = -1.0f;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_p2_axis_x = 1.0f;
        if (GetAsyncKeyState(VK_UP) & 0x8000)    g_p2_axis_y = -1.0f;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  g_p2_axis_y = 1.0f;

        if ((GetAsyncKeyState('J') & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000)) {
            g_p2_btn_shoot = 1;
        }
        if ((GetAsyncKeyState('K') & 0x8000) || (GetAsyncKeyState(VK_RCONTROL) & 0x8000) || (GetAsyncKeyState(VK_NUMPAD0) & 0x8000)) {
            g_p2_btn_roll = 1;
        }
    }

    // 3. Optional Automated Bot Test (ONLY if RunAutomatedTest=1 is explicitly set)
    if (g_runAutomatedTest && g_pPlayer1 && g_pPlayer2) {
        g_inGameFrames++;
        if (g_inGameFrames >= 30 && g_inGameFrames <= 90)   g_p2_axis_x = 1.0f;
        if (g_inGameFrames >= 100 && g_inGameFrames <= 115) g_p2_btn_roll = 1;
        if (g_inGameFrames >= 130 && g_inGameFrames <= 180) { g_p2_axis_y = -1.0f; g_p2_btn_shoot = 1; }
        if (g_inGameFrames >= 200 && g_inGameFrames <= 240) g_p2_btn_shoot = 1;
    }
}

// ============================================================================
// Hook: InputManager::GetButton
// ============================================================================
static char __cdecl Hooked_GetButton(const void* pStr) {
    const char* name = GetStdStringCStr(pStr);

    // Fast-forward Title Screen & Menu if QuickStart is enabled
    if (g_quickStart && !g_pPlayer1 && g_frameCount >= 180) {
        void* pMenu = *(void**)0x007168BC;
        uintptr_t vtbl = (pMenu && IsReadable(pMenu, 4)) ? *(uintptr_t*)pMenu : 0;
        if (strcmp(name, "menu_a") == 0 || strcmp(name, "roll") == 0) {
            if (!pMenu || vtbl == 0x0070C908 || vtbl == 0x0070E628) return 1;
        }
    }

    // Input dispatch for Player 2
    if (g_pCurrentUpdatingPlayer && g_pCurrentUpdatingPlayer == g_pPlayer2) {
        if (strcmp(name, "shoot") == 0) return g_p2_btn_shoot;
        if (strcmp(name, "roll") == 0)  return g_p2_btn_roll;
        if (strcmp(name, "up") == 0)    return (g_p2_axis_y < -0.5f);
        if (strcmp(name, "down") == 0)  return (g_p2_axis_y > 0.5f);
        if (strcmp(name, "left") == 0)  return (g_p2_axis_x < -0.5f);
        if (strcmp(name, "right") == 0) return (g_p2_axis_x > 0.5f);
        return 0;
    }

    // Keyboard menu fallback
    if (!g_pPlayer1 && IsGameWindowActive()) {
        if (strcmp(name, "menu_a") == 0) {
            if ((GetAsyncKeyState(VK_RETURN) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState('E') & 0x8000)) return 1;
        }
        if (strcmp(name, "menu_b") == 0) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return 1;
        }
        if (strcmp(name, "menu_up") == 0 || strcmp(name, "up") == 0) {
            if ((GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState('W') & 0x8000)) return 1;
        }
        if (strcmp(name, "menu_down") == 0 || strcmp(name, "down") == 0) {
            if ((GetAsyncKeyState(VK_DOWN) & 0x8000) || (GetAsyncKeyState('S') & 0x8000)) return 1;
        }
    }

    // Capture Local Player 1 input
    char btn = g_orig_GetButton(pStr);
    if (g_pCurrentUpdatingPlayer && g_pCurrentUpdatingPlayer == g_pPlayer1) {
        if (g_runAutomatedTest && g_netRole == 2) {
            // Scripted Client verification sequence
            if (g_inGameFrames >= 100 && g_inGameFrames <= 115) {
                if (strcmp(name, "roll") == 0) btn = 1;
            }
            if ((g_inGameFrames >= 130 && g_inGameFrames <= 180) || (g_inGameFrames >= 200 && g_inGameFrames <= 240)) {
                if (strcmp(name, "shoot") == 0) btn = 1;
            }
        }
        if (strcmp(name, "roll") == 0)  g_p1_btn_roll = btn;
        if (strcmp(name, "shoot") == 0) g_p1_btn_shoot = btn;
    }

    return btn;
}

// ============================================================================
// Hook: InputManager::GetAxis
// ============================================================================
static float __cdecl Hooked_GetAxis(const void* pStr) {
    const char* name = GetStdStringCStr(pStr);

    // Input dispatch for Player 2
    if (g_pCurrentUpdatingPlayer && g_pCurrentUpdatingPlayer == g_pPlayer2) {
        if (strcmp(name, "move_x") == 0) return g_p2_axis_x;
        if (strcmp(name, "move_y") == 0) return g_p2_axis_y;
        return 0.0f;
    }

    // Capture Local Player 1 axes
    float val = g_orig_GetAxis(pStr);
    if (g_pCurrentUpdatingPlayer && g_pCurrentUpdatingPlayer == g_pPlayer1) {
        if (g_runAutomatedTest && g_netRole == 2) {
            // Scripted Client movement: walk right
            if (g_inGameFrames >= 30 && g_inGameFrames <= 90) {
                if (strcmp(name, "move_x") == 0) val = 1.0f;
            }
            if (g_inGameFrames >= 130 && g_inGameFrames <= 180) {
                if (strcmp(name, "move_y") == 0) val = -1.0f; // Aim UP
            }
        }
        if (strcmp(name, "move_x") == 0) g_p1_axis_x = val;
        if (strcmp(name, "move_y") == 0) g_p1_axis_y = val;
    }

    return val;
}

// ============================================================================
// Hook: Player::Update
// ============================================================================
static void __fastcall Hooked_Player_Update(void* pThis, void* edx) {
    void* pPrev = g_pCurrentUpdatingPlayer;
    g_pCurrentUpdatingPlayer = pThis;

    g_orig_Player_Update(pThis);

    g_pCurrentUpdatingPlayer = pPrev;
}

// ============================================================================
// Hook: Camera::Update
// ============================================================================
static void __fastcall Hooked_Camera_Update(void* pThis, void* edx) {
    // In Network mode, Client can enjoy an independent camera centered on their own character
    if (g_netEnabled && g_netRole == 2 && g_netIndependentCamera) {
        g_orig_Camera_Update(pThis);
        return;
    }

    // Register Player 2 as native camera target for dual-player midpoint framing
    if (g_midpointCamera && g_pPlayer2 && IsReadable(g_pPlayer2, 0x418)) {
        float p2_x = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
        float p2_y = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);
        g_fnCameraAddTarget(pThis, p2_x, p2_y);
    }

    g_orig_Camera_Update(pThis);

    // Midpoint Camera Centering & Boundary Clamping
    if (g_midpointCamera && g_pPlayer1 && g_pPlayer2 && IsReadable(g_pPlayer1, 0x418) && IsReadable(g_pPlayer2, 0x418)) {
        float p1_x = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_X);
        float p1_y = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_Y);
        float p2_x = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
        float p2_y = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);

        float mid_x = (p1_x + p2_x) * 0.5f;
        float mid_y = (p1_y + p2_y) * 0.5f;

        float half_w = *(float*)ADDR_HALF_W;
        float half_h = *(float*)ADDR_HALF_H;

        float target_cam_x = mid_x - half_w;
        float target_cam_y = mid_y - half_h;

        if (IsReadable(pThis, 0x90) && *(char*)((uintptr_t)pThis + 0x81) != 0) {
            int map_w = *(int*)((uintptr_t)pThis + 0x84);
            int map_h = *(int*)((uintptr_t)pThis + 0x88);

            float max_x = (float)map_w - half_w * 2.0f;
            float max_y = (float)map_h - half_h * 2.0f;
            if (max_x < 0.0f) max_x = 0.0f;
            if (max_y < 0.0f) max_y = 0.0f;

            if (target_cam_x < 0.0f) target_cam_x = 0.0f;
            if (target_cam_x > max_x) target_cam_x = max_x;
            if (target_cam_y < 0.0f) target_cam_y = 0.0f;
            if (target_cam_y > max_y) target_cam_y = max_y;
        }

        *(float*)ADDR_CAM_OFFSET_X = (float)((int)target_cam_x);
        *(float*)ADDR_CAM_OFFSET_Y = (float)((int)target_cam_y);
    }
}

// ============================================================================
// Spawn Player 2 Entity
// ============================================================================
static void TrySpawnPlayer2(void) {
    if (g_pPlayer2 != NULL) return;

    void* pPlayer1 = *(void**)ADDR_PLAYER1_PTR;
    if (!pPlayer1 || !IsReadable(pPlayer1, 0x418)) return;

    void* pGame = *(void**)ADDR_GAME_PTR;
    if (!pGame || !IsReadable(pGame, 0x40)) return;

    void* pGameWorld = *(void**)ADDR_GAMEWORLD_PTR;
    if (!pGameWorld || !IsReadable(pGameWorld, 0x30)) return;

    uintptr_t gwVtbl = *(uintptr_t*)pGameWorld;
    if (gwVtbl != 0x00711928) return;

    g_pPlayer1 = pPlayer1;

    float p1_x = *(float*)((uintptr_t)pPlayer1 + OFF_POS_X);
    float p1_y = *(float*)((uintptr_t)pPlayer1 + OFF_POS_Y);

    LogMsg("[Frame %d] Instantiating Player 2 entity near (%.2f, %.2f)...\n", g_frameCount, p1_x, p1_y);

    void* pMem = g_fnOperatorNew(0x418);
    if (!pMem) {
        LogMsg("[Frame %d] ERROR: Operator new failed for Player 2!\n", g_frameCount);
        return;
    }

    g_fnPlayerCtor(pMem, 0.0f, 0.0f);
    void* pPlayer2 = pMem;

    if (IsReadable(pPlayer1, 0x60)) {
        *(void**)((uintptr_t)pPlayer2 + 0x50) = *(void**)((uintptr_t)pPlayer1 + 0x50);
    }

    float spawn_x = p1_x + 24.0f;
    float spawn_y = p1_y;
    *(float*)((uintptr_t)pPlayer2 + OFF_POS_X) = spawn_x;
    *(float*)((uintptr_t)pPlayer2 + OFF_POS_Y) = spawn_y;
    *(float*)((uintptr_t)pPlayer2 + 0x94) = spawn_x;
    *(float*)((uintptr_t)pPlayer2 + 0x98) = spawn_y;

    *(float*)((uintptr_t)pPlayer2 + OFF_COLOR_R) = g_p2ColorR;
    *(float*)((uintptr_t)pPlayer2 + OFF_COLOR_G) = g_p2ColorG;
    *(float*)((uintptr_t)pPlayer2 + OFF_COLOR_B) = g_p2ColorB;
    *(float*)((uintptr_t)pPlayer2 + OFF_COLOR_A) = g_p2ColorA;

    g_fnWorldAddEntity(pGameWorld, pPlayer2);
    g_pPlayer2 = pPlayer2;

    void* pArrow2 = *(void**)((uintptr_t)pPlayer2 + OFF_PLAYER_ARROW);
    LogMsg("\n============================================================\n");
    LogMsg(">>> PLAYER 2 SUCCESSFULLY SPAWNED! <<<\n");
    LogMsg("   Player 1: 0x%p | Pos: (%.2f, %.2f)\n", g_pPlayer1, p1_x, p1_y);
    LogMsg("   Player 2: 0x%p | Pos: (%.2f, %.2f)\n", g_pPlayer2, spawn_x, spawn_y);
    LogMsg("   Player 2 Arrow: 0x%p | Color: RGBA(%.2f, %.2f, %.2f, %.2f)\n",
           pArrow2, g_p2ColorR, g_p2ColorG, g_p2ColorB, g_p2ColorA);
    LogMsg("============================================================\n\n");
}

// ============================================================================
// Hook: SDL_GL_SwapWindow (Main Frame Tick)
// ============================================================================
static void SDLCALL Hooked_SDL_GL_SwapWindow(SDL_Window* window) {
    g_frameCount++;

    // 1. Network Sync & Input Polling
    if (g_netEnabled) {
        Net_PollIncoming();
        Net_UpdatePuppet();
        Net_BroadcastLocalPlayer();
    } else {
        PollLocalPlayer2Inputs();
    }

    // 2. QuickStart Automation: Automatically load Slot 0 if enabled
    if (g_quickStart && !g_pPlayer1 && g_frameCount >= 200) {
        void* pMenu = *(void**)0x007168BC;
        if (pMenu && IsReadable(pMenu, 0xD0)) {
            uintptr_t vtbl = *(uintptr_t*)pMenu;
            if (vtbl == 0x0070C908) {
                *(char*)((uintptr_t)pMenu + 0xC4) = 1;
                void* pGPE = *(void**)0x00716658;
                if (pGPE && IsReadable(pGPE, 0x80)) {
                    void* pFade = *(void**)((uintptr_t)pGPE + 0x7C);
                    if (pFade && IsReadable(pFade, 0x70)) {
                        *(float*)((uintptr_t)pFade + 0x64) = 1.0f;
                    }
                }
            } else if (vtbl == 0x0070E628 && !g_quickStartTriggered) {
                g_quickStartTriggered = TRUE;
                LogMsg("[Frame %d] QuickStart: Loading Save Slot 0...\n", g_frameCount);
                void* edx = *(void**)((uintptr_t)pMenu + 0xB8);
                if (edx && IsReadable(edx, 0xC0)) {
                    *(char*)((uintptr_t)edx + 0x7D) = 1;
                    void* pGameObj = *(void**)((uintptr_t)edx + 0xB0);
                    if (pGameObj && IsReadable(pGameObj, 0x10)) {
                        *(short*)((uintptr_t)pGameObj + 6) = 0; // Slot 0
                        ((LoadGame_t)ADDR_LOAD_GAME)(pGameObj);
                        LogMsg("[Frame %d] QuickStart: LoadGame(0) triggered successfully!\n", g_frameCount);
                    }
                }
            }
        }
    }

    // 3. Track Player 1 and spawn Player 2
    void* pP1 = *(void**)ADDR_PLAYER1_PTR;
    void* pGame = *(void**)ADDR_GAME_PTR;
    if (pP1 && IsReadable(pP1, 0x418) && pGame && IsReadable(pGame, 0x40)) {
        g_pPlayer1 = pP1;
        g_inGameFrames++;
        if (!g_pPlayer2) {
            TrySpawnPlayer2();
        }
    } else {
        // shortcut: GameWorld frees entities on map teardown; zero pointer here to re-spawn in next room.
        if (g_pPlayer2 && !pP1) {
            g_pPlayer1 = NULL;
            g_pPlayer2 = NULL;
            g_inGameFrames = 0;
            LogMsg("[Frame %d] Map unloaded, resetting Player 2 reference.\n", g_frameCount);
        }
    }

    // 4. Periodic status logging (every 60 frames = once per second)
    if (g_frameCount % 60 == 0) {
        float cam_x = *(float*)ADDR_CAM_OFFSET_X;
        float cam_y = *(float*)ADDR_CAM_OFFSET_Y;

        if (g_pPlayer1 && g_pPlayer2) {
            float p1_x = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_X);
            float p1_y = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_Y);
            float p2_x = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
            float p2_y = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);

            if (g_netEnabled) {
                LogMsg("[F%d] Net:[%s state=%d TX=%u RX=%u] | Cam:(%.1f,%.1f) | P1:(%.1f,%.1f) | P2:(%.1f,%.1f)\n",
                       g_frameCount, (g_netRole == 1 ? "HOST" : "CLIENT"), g_netState,
                       g_netPacketsSent, g_netPacketsRecv, cam_x, cam_y, p1_x, p1_y, p2_x, p2_y);
            } else {
                LogMsg("[F%d] Local Co-op | Cam:(%.1f,%.1f) | P1:(%.1f,%.1f) | P2:(%.1f,%.1f) | P2_In:(x=%.1f,y=%.1f,roll=%d,shoot=%d)\n",
                       g_frameCount, cam_x, cam_y, p1_x, p1_y, p2_x, p2_y,
                       g_p2_axis_x, g_p2_axis_y, g_p2_btn_roll, g_p2_btn_shoot);
            }
        } else {
            LogMsg("[Frame %d] [MENU] P1: 0x%p | QuickStart: %d | NetEnabled: %d (Role: %d)\n",
                   g_frameCount, pP1, g_quickStart, g_netEnabled, g_netRole);
        }
    }

    g_orig_SwapWindow(window);
}

// ============================================================================
// DLL Initialization & Teardown
// ============================================================================
static void InitXInput(void) {
    const char* xinput_dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (int i = 0; i < 3; i++) {
        HMODULE hX = LoadLibraryA(xinput_dlls[i]);
        if (hX) {
            g_pfnXInputGetState = (XInputGetState_t)GetProcAddress(hX, "XInputGetState");
            if (g_pfnXInputGetState) {
                LogMsg("Bound %s successfully.\n", xinput_dlls[i]);
                break;
            }
        }
    }
}

void DLLP_OnAttach(void) {
    FILE* f = fopen("coop_log.txt", "w");
    if (f) {
        fprintf(f, "=== Titan Souls Co-op Mod Initializing ===\n");
        fclose(f);
    }

    LoadConfig();
    InitXInput();
    Net_Init();

    if (MH_Initialize() == MH_OK) {
        HMODULE hSDL = GetModuleHandleW(L"SDL2.dll");
        if (hSDL) {
            void* pSwap = (void*)GetProcAddress(hSDL, "SDL_GL_SwapWindow");
            if (pSwap) {
                MH_CreateHook(pSwap, (void*)Hooked_SDL_GL_SwapWindow, (void**)&g_orig_SwapWindow);
            }
        }

        MH_CreateHook((void*)ADDR_GET_BUTTON, (void*)Hooked_GetButton, (void**)&g_orig_GetButton);
        MH_CreateHook((void*)ADDR_GET_AXIS, (void*)Hooked_GetAxis, (void**)&g_orig_GetAxis);
        MH_CreateHook((void*)ADDR_PLAYER_UPDATE, (void*)Hooked_Player_Update, (void**)&g_orig_Player_Update);
        MH_CreateHook((void*)ADDR_CAMERA_UPDATE, (void*)Hooked_Camera_Update, (void**)&g_orig_Camera_Update);

        MH_EnableHook(MH_ALL_HOOKS);
        LogMsg("All hooks installed successfully (SwapWindow, GetButton, GetAxis, Player::Update, Camera::Update).\n");
    } else {
        LogMsg("ERROR: MinHook initialization failed!\n");
    }
}

void DLLP_OnDetach(void) {
    LogMsg("=== Titan Souls Co-op Mod Detaching ===\n");
    Net_Shutdown();
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
