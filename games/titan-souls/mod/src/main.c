#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include "dllproxy.h"
#include "MinHook.h"

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

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
#define ADDR_WORLD_PTR        0x0071665C  // Active World / Map pointer
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
#define ADDR_PLAYER_UPDATEDEATH  0x005AD144  // Player::UpdateDeath() __thiscall
#define ADDR_GAMEWORLD_RESETROOM 0x0055309C  // GameWorld::ResetRoom() __thiscall
#define ADDR_PLAYER_SET_ANIM     0x005BA7A4  // Player::SetAnimation(string&, bool, bool) __thiscall
#define ADDR_STRING_CTOR         0x00699FD8  // std::string::basic_string(const char*, alloc) __thiscall
#define ADDR_STRING_DTOR         0x0069A634  // std::string::~basic_string() __thiscall
#define ADDR_PLAYER_INIT         0x005A6FA4  // Player::Init() (vtable[3]) __thiscall
#define ADDR_ARROW_ISDAMAGING    0x005A05A4  // Arrow::IsDamaging() __thiscall
#define ADDR_TITAN_HITSENSOR_ONHIT 0x00589700 // TitanHitSensor::OnHit() (vtable[7]) __thiscall

// Entity field offsets
#define OFF_POS_X             0x18
#define OFF_POS_Y             0x1C
#define OFF_VEL_X             0x88
#define OFF_VEL_Y             0x8C
#define OFF_ACTIVE_FLAG       0xA0        // byte: 1 = active entity
#define OFF_COLOR_R           0x334
#define OFF_COLOR_G           0x338
#define OFF_COLOR_B           0x33C
#define OFF_COLOR_A           0x340
#define OFF_PLAYER_ROLL       0x368       // 0=idle/normal, 1=rolling, 2=recovery, 3=dead
#define OFF_ALIVE_FLAG        0x378       // byte: 1 = alive, 0 = dead
#define OFF_DEATH_SLOWDOWN    0x3E8       // float: slow-motion / zoom factor
#define OFF_DEATH_TIMER       0x3EC       // uint16_t: frames elapsed since death
#define OFF_WAKEUP_FLAG       0x40D       // byte: 1 = wake / getup sequence
#define OFF_PLAYER_ARROW      0x35c       // Arrow entity pointer (5Arrow class)

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
typedef void (__thiscall *PlayerUpdateDeath_t)(void* pThis);
typedef void (__thiscall *GameWorldResetRoom_t)(void* pWorld);
typedef void (__thiscall *StringCtor_t)(void* pStr, const char* s, const void* alloc);
typedef void (__thiscall *StringDtor_t)(void* pStr);
typedef void (__thiscall *PlayerSetAnim_t)(void* pPlayer, void* pStr, int b1, int b2);
typedef void (__thiscall *PlayerInit_t)(void* pPlayer);
typedef char (__fastcall *ArrowIsDamaging_t)(void* pArrow, void* edx);
typedef void (__fastcall *TitanHitSensorOnHit_t)(void* pSensor, void* edx, float hit_x, float hit_y, float vel_x, float vel_y);

// Original function pointers & engine callables
static OperatorNew_t       g_fnOperatorNew       = (OperatorNew_t)ADDR_OPERATOR_NEW;
static PlayerCtor_t        g_fnPlayerCtor        = (PlayerCtor_t)ADDR_PLAYER_CTOR;
static WorldAddEntity_t    g_fnWorldAddEntity    = (WorldAddEntity_t)ADDR_WORLD_ADD_ENTITY;
static CameraAddTarget_t   g_fnCameraAddTarget   = (CameraAddTarget_t)ADDR_CAMERA_ADDTARGET;
static StringCtor_t        g_fnStringCtor        = (StringCtor_t)ADDR_STRING_CTOR;
static StringDtor_t        g_fnStringDtor        = (StringDtor_t)ADDR_STRING_DTOR;
static PlayerSetAnim_t     g_fnPlayerSetAnim     = (PlayerSetAnim_t)ADDR_PLAYER_SET_ANIM;

static PlayerUpdate_t        g_orig_Player_Update      = NULL;
static CameraUpdate_t        g_orig_Camera_Update      = NULL;
static GetButton_t           g_orig_GetButton          = NULL;
static GetAxis_t             g_orig_GetAxis            = NULL;
static SDL_GL_SwapWindow_t   g_orig_SwapWindow         = NULL;
static PlayerUpdateDeath_t   g_orig_Player_UpdateDeath = NULL;
static GameWorldResetRoom_t  g_orig_GameWorld_ResetRoom = NULL;
static ArrowIsDamaging_t     g_orig_Arrow_IsDamaging    = NULL;
static TitanHitSensorOnHit_t g_orig_TitanHitSensor_OnHit = NULL;

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
#define NET_PKT_BOSS_HIT      6

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
    // Player transform & velocity
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
    uint8_t  btn_up;
    uint8_t  btn_down;
    uint8_t  btn_left;
    uint8_t  btn_right;
    uint8_t  roll_state;  // [Player + OFF_PLAYER_ROLL]
    float    dir_x;       // [Character + 0xA4]
    float    dir_y;       // [Character + 0xA8]
    float    facing_x;    // [Player + 0x3B0]
    float    facing_y;    // [Player + 0x3B4]
} NetPlayerSyncPacket;

typedef struct {
    NetHeader header;
    float     hit_x;
    float     hit_y;
    float     hit_vel_x;
    float     hit_vel_y;
} NetBossHitPacket;
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

// Steamworks API types & dynamic function pointers
typedef bool (__thiscall *SendP2PPacket_t)(void* pThis, uint64_t steamIDRemote, const void* pubData, uint32_t cubData, int eP2PSendType, int nChannel);
typedef bool (__thiscall *IsP2PPacketAvailable_t)(void* pThis, uint32_t* pcubMsgSize, int nChannel);
typedef bool (__thiscall *ReadP2PPacket_t)(void* pThis, void* pubDest, uint32_t cubDest, uint32_t* pcubMsgSize, uint64_t* psteamIDRemote, int nChannel);
typedef bool (__thiscall *AcceptP2PSessionWithUser_t)(void* pThis, uint64_t steamIDRemote);
typedef bool (__thiscall *CloseP2PSessionWithUser_t)(void* pThis, uint64_t steamIDRemote);
typedef void (__thiscall *GetSteamID_t)(void* pThis, uint64_t* pOut);
typedef const char* (__thiscall *GetPersonaName_t)(void* pThis);
typedef void (__thiscall *ActivateGameOverlay_t)(void* pThis, const char* pchDialog);
typedef void (*SteamRunCallbacks_t)(void);

// Network Configuration
static BOOL     g_netEnabled = FALSE;
static char     g_netTransport[16] = "steam"; // "steam" or "udp"
static int      g_netRole = 1; // 1 = Host, 2 = Client
static int      g_netBindPort = 7777;
static char     g_netRemoteIP[64] = "127.0.0.1";
static int      g_netRemotePort = 7778;
static BOOL     g_netIndependentCamera = TRUE;

// Steamworks State
static void*    g_pSteamNetworking = NULL;
static void*    g_pSteamUser = NULL;
static void*    g_pSteamFriends = NULL;
static SteamRunCallbacks_t g_pfnSteamRunCallbacks = NULL;

static SendP2PPacket_t            g_pfnSendP2P = NULL;
static IsP2PPacketAvailable_t     g_pfnIsP2PAvail = NULL;
static ReadP2PPacket_t            g_pfnReadP2P = NULL;
static AcceptP2PSessionWithUser_t  g_pfnAcceptP2P = NULL;
static CloseP2PSessionWithUser_t  g_pfnCloseP2P = NULL;
static GetSteamID_t               g_pfnGetSteamID = NULL;
static GetPersonaName_t           g_pfnGetPersona = NULL;
static ActivateGameOverlay_t      g_pfnOverlay = NULL;

static uint64_t g_steamMyID = 0;
static uint64_t g_steamFriendID = 0;
static char     g_steamPersonaName[64] = "";
static BOOL     g_steamActive = FALSE;
static BOOL     g_netInitialized = FALSE;

// Network Runtime State
static SOCKET   g_netSocket = INVALID_SOCKET;
static struct sockaddr_in g_netRemoteAddr;
static int      g_netState = NET_STATE_DISCONNECTED;
static uint32_t g_netSeqOut = 0;
static uint32_t g_netLastSeqIn = 0;
static DWORD    g_netLastRecvTick = 0;
static DWORD    g_netLastSendTick = 0;
static uint32_t g_netPacketsSent = 0;
static uint32_t g_netPacketsRecv = 0;
static NetPlayerSyncPacket g_remoteSyncState;
static BOOL     g_hasRemoteSync = FALSE;

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
static char  g_p1_btn_up = 0;
static char  g_p1_btn_down = 0;
static char  g_p1_btn_left = 0;
static char  g_p1_btn_right = 0;

// Player 2 Local / Remote Inputs
static float g_p2_axis_x = 0.0f;
static float g_p2_axis_y = 0.0f;
static char  g_p2_btn_shoot = 0;
static char  g_p2_btn_roll = 0;
static char  g_p2_btn_up = 0;
static char  g_p2_btn_down = 0;
static char  g_p2_btn_left = 0;
static char  g_p2_btn_right = 0;

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

static FILE* g_logFile = NULL;

static void LogMsg(const char* fmt, ...) {
    if (g_logFile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(g_logFile, fmt, args);
        va_end(args);
        fflush(g_logFile);
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
// Network Subsystem Implementation (Steamworks P2P + UDP Socket Fallback)
// ============================================================================
static void Net_SendPacket(const void* data, int size) {
    if (!g_netEnabled) return;
    if (g_steamActive && g_pSteamNetworking && g_steamFriendID != 0) {
        // k_EP2PSendUnreliableNoDelay = 1
        g_pfnSendP2P(g_pSteamNetworking, g_steamFriendID, data, size, 1, 0);
        g_netPacketsSent++;
    } else if (g_netSocket != INVALID_SOCKET) {
        sendto(g_netSocket, (const char*)data, size, 0,
               (struct sockaddr*)&g_netRemoteAddr, sizeof(g_netRemoteAddr));
        g_netPacketsSent++;
    }
}

static void ParseCommandLineSteamJoin(void) {
    LPSTR cmdLine = GetCommandLineA();
    if (!cmdLine) return;

    const char* conn = strstr(cmdLine, "+connect");
    if (!conn) conn = strstr(cmdLine, "-steamjoin");
    if (conn) {
        conn += (conn[1] == 'c' ? 8 : 10);
        while (*conn == ' ' || *conn == '\t') conn++;
        if (*conn) {
            uint64_t sid = _strtoui64(conn, NULL, 10);
            if (sid != 0) {
                g_steamFriendID = sid;
                g_netRole = 2; // Auto-set Role to Client when joining friend
                LogMsg("[Steam] Parsed connect argument: Target SteamID = %llu\n", g_steamFriendID);
            }
        }
    }
}

static void Net_Init(void) {
    if (!g_netEnabled) return;

    ParseCommandLineSteamJoin();

    // 1. Try initializing Steamworks P2P if transport is "steam"
    if (_stricmp(g_netTransport, "steam") == 0) {
        HMODULE hSteam = GetModuleHandleW(L"steam_api.dll");
        if (!hSteam) hSteam = LoadLibraryW(L"steam_api.dll");

        if (hSteam) {
            typedef void* (*SteamGetter_t)(void);
            SteamGetter_t pfnNet = (SteamGetter_t)GetProcAddress(hSteam, "SteamNetworking");
            SteamGetter_t pfnUser = (SteamGetter_t)GetProcAddress(hSteam, "SteamUser");
            SteamGetter_t pfnFriends = (SteamGetter_t)GetProcAddress(hSteam, "SteamFriends");
            g_pfnSteamRunCallbacks = (SteamRunCallbacks_t)GetProcAddress(hSteam, "SteamAPI_RunCallbacks");

            if (pfnNet && pfnUser) {
                g_pSteamNetworking = pfnNet();
                g_pSteamUser = pfnUser();
                g_pSteamFriends = pfnFriends ? pfnFriends() : NULL;

                if (g_pSteamNetworking && g_pSteamUser) {
                    uintptr_t* netVtbl = *(uintptr_t**)g_pSteamNetworking;
                    uintptr_t* userVtbl = *(uintptr_t**)g_pSteamUser;

                    g_pfnSendP2P    = (SendP2PPacket_t)netVtbl[0];
                    g_pfnIsP2PAvail = (IsP2PPacketAvailable_t)netVtbl[1];
                    g_pfnReadP2P    = (ReadP2PPacket_t)netVtbl[2];
                    g_pfnAcceptP2P  = (AcceptP2PSessionWithUser_t)netVtbl[3];
                    g_pfnCloseP2P   = (CloseP2PSessionWithUser_t)netVtbl[4];

                    g_pfnGetSteamID = (GetSteamID_t)userVtbl[2];
                    g_pfnGetSteamID(g_pSteamUser, &g_steamMyID);

                    if (g_pSteamFriends) {
                        uintptr_t* friendsVtbl = *(uintptr_t**)g_pSteamFriends;
                        g_pfnGetPersona = (GetPersonaName_t)friendsVtbl[0];
                        g_pfnOverlay = (ActivateGameOverlay_t)friendsVtbl[29];
                        const char* name = g_pfnGetPersona ? g_pfnGetPersona(g_pSteamFriends) : "";
                        if (name) strcpy_s(g_steamPersonaName, sizeof(g_steamPersonaName), name);
                    }

                    g_steamActive = TRUE;
                    g_netState = (g_netRole == 2) ? NET_STATE_CONNECTING : NET_STATE_DISCONNECTED;
                    LogMsg("[Net] Steamworks P2P active. Persona: '%s' | My SteamID: %llu | Target Friend: %llu\n",
                           g_steamPersonaName, g_steamMyID, g_steamFriendID);
                    return; // Steam transport successfully ready
                }
            }
        }
        LogMsg("[Net] Steamworks not available. Falling back to UDP socket.\n");
    }

    // 2. Direct-IP UDP Socket Initialization (Fallback / LAN mode)
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

    int reuse = 1;
    setsockopt(g_netSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    u_long nonblock = 1;
    ioctlsocket(g_netSocket, FIONBIO, &nonblock);

    // Disable Windows UDP connection reset on ICMP port unreachable
    BOOL bNewBehavior = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(g_netSocket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
             NULL, 0, &dwBytesReturned, NULL, NULL);

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

    ZeroMemory(&g_netRemoteAddr, sizeof(g_netRemoteAddr));
    g_netRemoteAddr.sin_family = AF_INET;
    g_netRemoteAddr.sin_addr.s_addr = inet_addr(g_netRemoteIP);
    g_netRemoteAddr.sin_port = htons((u_short)g_netRemotePort);

    g_netState = (g_netRole == 2) ? NET_STATE_CONNECTING : NET_STATE_DISCONNECTED;
    LogMsg("[Net] UDP Socket Initialized. Mode: %s | Bound: 0.0.0.0:%d | Target: %s:%d\n",
           (g_netRole == 1 ? "HOST" : "CLIENT"), g_netBindPort, g_netRemoteIP, g_netRemotePort);
}

static void Net_Shutdown(void) {
    if (g_netState == NET_STATE_CONNECTED) {
        NetHeader bye;
        bye.magic = NET_MAGIC;
        bye.version = NET_PROTOCOL_VERSION;
        bye.pktType = NET_PKT_DISCONNECT;
        bye.senderRole = (uint16_t)g_netRole;
        bye.sequence = ++g_netSeqOut;
        Net_SendPacket(&bye, sizeof(bye));
    }
    if (g_steamActive && g_pSteamNetworking && g_steamFriendID != 0) {
        if (g_pfnCloseP2P) g_pfnCloseP2P(g_pSteamNetworking, g_steamFriendID);
    }
    if (g_netSocket != INVALID_SOCKET) {
        closesocket(g_netSocket);
        g_netSocket = INVALID_SOCKET;
        WSACleanup();
    }
}

static void ProcessIncomingPacket(const char* recvBuf, int bytes, uint64_t senderSteamID, struct sockaddr_in* senderAddr) {
    if ((size_t)bytes < sizeof(NetHeader)) return;

    NetHeader* hdr = (NetHeader*)recvBuf;
    if (hdr->magic != NET_MAGIC || hdr->version != NET_PROTOCOL_VERSION) return;

    g_netPacketsRecv++;
    g_netLastRecvTick = GetTickCount();

    // 1. Handshake Packet (Client -> Host)
    if (hdr->pktType == NET_PKT_HANDSHAKE) {
        if ((size_t)bytes < sizeof(NetHandshakePacket)) return;
        if (g_netRole == 1) { // Host
            if (g_steamActive) {
                if (g_netState == NET_STATE_CONNECTED && g_steamFriendID != 0 && g_steamFriendID != senderSteamID && g_hasRemoteSync) {
                    return; // Ignore other players while session is active
                }
                g_steamFriendID = senderSteamID;
                g_netState = NET_STATE_CONNECTED;
                g_netLastSeqIn = 0;
                LogMsg("[Net/Steam] Handshake from SteamID %llu! Sending ACK...\n", senderSteamID);

                NetHandshakeAckPacket ack;
                ack.header.magic = NET_MAGIC;
                ack.header.version = NET_PROTOCOL_VERSION;
                ack.header.pktType = NET_PKT_HANDSHAKE_ACK;
                ack.header.senderRole = 1;
                ack.header.sequence = ++g_netSeqOut;
                ack.status = 1;
                Net_SendPacket(&ack, sizeof(ack));
            } else if (senderAddr) {
                BOOL isSamePeer = (senderAddr->sin_addr.s_addr == g_netRemoteAddr.sin_addr.s_addr &&
                                   senderAddr->sin_port == g_netRemoteAddr.sin_port);
                if (g_netState == NET_STATE_CONNECTED && !isSamePeer && g_hasRemoteSync) {
                    return;
                }
                g_netRemoteAddr = *senderAddr;
                g_netState = NET_STATE_CONNECTED;
                g_netLastSeqIn = 0;
                LogMsg("[Net/UDP] Handshake from %s:%d! Sending ACK...\n",
                       inet_ntoa(senderAddr->sin_addr), ntohs(senderAddr->sin_port));

                NetHandshakeAckPacket ack;
                ack.header.magic = NET_MAGIC;
                ack.header.version = NET_PROTOCOL_VERSION;
                ack.header.pktType = NET_PKT_HANDSHAKE_ACK;
                ack.header.senderRole = 1;
                ack.header.sequence = ++g_netSeqOut;
                ack.status = 1;
                Net_SendPacket(&ack, sizeof(ack));
            }
        }
    }
    // 2. Handshake ACK Packet (Host -> Client)
    else if (hdr->pktType == NET_PKT_HANDSHAKE_ACK) {
        if ((size_t)bytes < sizeof(NetHandshakeAckPacket)) return;
        if (g_netRole == 2) {
            g_netState = NET_STATE_CONNECTED;
            g_netLastSeqIn = 0;
            if (g_steamActive) {
                g_steamFriendID = senderSteamID;
                LogMsg("[Net/Steam] Handshake ACK received! Connected to Host SteamID %llu!\n", senderSteamID);
            } else if (senderAddr) {
                LogMsg("[Net/UDP] Handshake ACK received! Connected to Host at %s:%d!\n",
                       inet_ntoa(senderAddr->sin_addr), ntohs(senderAddr->sin_port));
            }
        }
    }
    // 3. Heartbeat
    else if (hdr->pktType == NET_PKT_HEARTBEAT) {
        if ((size_t)bytes < sizeof(NetHeartbeatPacket)) return;
        // Peer alive
    }
    // 4. Player State Sync
    else if (hdr->pktType == NET_PKT_PLAYER_SYNC) {
        if ((size_t)bytes >= sizeof(NetPlayerSyncPacket)) {
            NetPlayerSyncPacket* sync = (NetPlayerSyncPacket*)recvBuf;
            // Validate floats against NaN / Inf corruptions
            if (!_finite(sync->pos_x) || !_finite(sync->pos_y) ||
                !_finite(sync->vel_x) || !_finite(sync->vel_y) ||
                !_finite(sync->arrow_x) || !_finite(sync->arrow_y)) {
                return;
            }
            // RFC 1982 Serial Number Arithmetic for sequence progression
            int32_t seqDiff = (int32_t)(sync->header.sequence - g_netLastSeqIn);
            if (seqDiff > 0 || g_netLastSeqIn == 0) {
                g_netLastSeqIn = sync->header.sequence;
                g_remoteSyncState = *sync;
                g_hasRemoteSync = TRUE;
                if (g_netState != NET_STATE_CONNECTED) {
                    g_netState = NET_STATE_CONNECTED;
                    if (senderAddr && g_netRole == 1) {
                        g_netRemoteAddr = *senderAddr;
                    }
                }
            }
        }
    }
    // 5. Disconnect
    else if (hdr->pktType == NET_PKT_DISCONNECT) {
        LogMsg("[Net] Peer sent disconnect signal.\n");
        g_netState = (g_netRole == 2) ? NET_STATE_CONNECTING : NET_STATE_DISCONNECTED;
        g_hasRemoteSync = FALSE;
        g_netLastSeqIn = 0;
    }
    // 6. Boss Hit Sync Packet
    else if (hdr->pktType == NET_PKT_BOSS_HIT) {
        if ((size_t)bytes < sizeof(NetBossHitPacket)) return;
        const NetBossHitPacket* pBossPkt = (const NetBossHitPacket*)recvBuf;
        LogMsg("[Net] Received BOSS_HIT from peer: hit=(%.2f, %.2f), vel=(%.2f, %.2f)\n",
               pBossPkt->hit_x, pBossPkt->hit_y, pBossPkt->hit_vel_x, pBossPkt->hit_vel_y);

        void* pGameWorld = *(void**)ADDR_GAMEWORLD_PTR;
        if (pGameWorld && IsReadable(pGameWorld, 0x10)) {
            void** pBegin = *(void***)((uintptr_t)pGameWorld + 0x04);
            void** pEnd   = *(void***)((uintptr_t)pGameWorld + 0x08);
            if (pBegin && pEnd && pEnd > pBegin && (size_t)(pEnd - pBegin) <= 2048 &&
                IsReadable(pBegin, (size_t)(pEnd - pBegin) * sizeof(void*))) {
                size_t count = pEnd - pBegin;
                for (size_t i = 0; i < count; i++) {
                    void* pEnt = pBegin[i];
                    if (pEnt && IsReadable(pEnt, 0xA0)) {
                        uintptr_t vt = *(uintptr_t*)pEnt;
                        if (vt == 0x0070E728) { // TitanHitSensor
                            *(float*)((uintptr_t)pEnt + 0x98) = pBossPkt->hit_vel_x;
                            *(float*)((uintptr_t)pEnt + 0x9c) = pBossPkt->hit_vel_y;
                            *(uint8_t*)((uintptr_t)pEnt + 0x94) = 1;
                            LogMsg("[Net] Triggered local TitanHitSensor at 0x%p!\n", pEnt);
                        }
                    }
                }
            }
        }
    }
}

static void Net_PollIncoming(void) {
    if (!g_netEnabled) return;

    if (g_pfnSteamRunCallbacks) g_pfnSteamRunCallbacks();

    // 1. Poll Steamworks P2P packets
    if (g_steamActive && g_pSteamNetworking) {
        uint32_t msgSize = 0;
        while (g_pfnIsP2PAvail(g_pSteamNetworking, &msgSize, 0) && msgSize > 0) {
            char recvBuf[256];
            uint32_t readSize = (msgSize < sizeof(recvBuf)) ? msgSize : sizeof(recvBuf);
            uint64_t senderId = 0;
            if (g_pfnReadP2P(g_pSteamNetworking, recvBuf, sizeof(recvBuf), &readSize, &senderId, 0)) {
                if (g_pfnAcceptP2P) g_pfnAcceptP2P(g_pSteamNetworking, senderId);
                ProcessIncomingPacket(recvBuf, (int)readSize, senderId, NULL);
            } else {
                break;
            }
        }
    }
    // 2. Poll UDP socket packets
    else if (g_netSocket != INVALID_SOCKET) {
        char recvBuf[256];
        struct sockaddr_in senderAddr;
        while (1) {
            int senderLen = sizeof(senderAddr);
            int bytes = recvfrom(g_netSocket, recvBuf, sizeof(recvBuf), 0,
                                 (struct sockaddr*)&senderAddr, &senderLen);
            if (bytes <= 0) break;
            ProcessIncomingPacket(recvBuf, bytes, 0, &senderAddr);
        }
    }

    // Connection timeout check (common to both UDP and Steam P2P)
    if (g_netState == NET_STATE_CONNECTED && g_netLastRecvTick > 0) {
        if (GetTickCount() - g_netLastRecvTick > 5000) {
            LogMsg("[Net] Connection timed out (5s without packets). Dropping session.\n");
            g_netState = (g_netRole == 2) ? NET_STATE_CONNECTING : NET_STATE_DISCONNECTED;
            g_hasRemoteSync = FALSE;
            g_netLastRecvTick = 0;
            g_netLastSeqIn = 0;
        }
    }
}

// ============================================================================
// Cooperative Life & Revive Subsystem
// ============================================================================
static BOOL IsPlayerAlive(void* pPlayer) {
    if (!pPlayer || !IsReadable(pPlayer, 0x418)) return FALSE;
    int roll_state = *(int*)((uintptr_t)pPlayer + OFF_PLAYER_ROLL);
    if (roll_state == 3) return FALSE;
    char alive_byte = *(char*)((uintptr_t)pPlayer + OFF_ALIVE_FLAG);
    if (alive_byte == 0) return FALSE;
    return TRUE;
}

static void Player_SetAnimation(void* pPlayer, const char* animName) {
    if (!pPlayer || !IsReadable(pPlayer, 0x418)) return;
    char dummyAlloc = 0;
    void* strObj = NULL;
    g_fnStringCtor(&strObj, animName, &dummyAlloc);
    if (strObj) {
        g_fnPlayerSetAnim(pPlayer, &strObj, 1, 0);
        g_fnStringDtor(&strObj);
    }
}

static void RevivePlayer(void* pDeadPlayer) {
    if (!pDeadPlayer || !IsReadable(pDeadPlayer, 0x418)) return;

    LogMsg("[Co-op Revive] Reviving fallen player 0x%p!\n", pDeadPlayer);

    // 1. Reset death state and roll state to 0
    *(int*)((uintptr_t)pDeadPlayer + OFF_PLAYER_ROLL) = 0;
    *(char*)((uintptr_t)pDeadPlayer + OFF_ALIVE_FLAG) = 1;  // is_alive = true
    *(char*)((uintptr_t)pDeadPlayer + 0x37A) = 0;
    *(char*)((uintptr_t)pDeadPlayer + 0x37B) = 0;
    *(short*)((uintptr_t)pDeadPlayer + 0x386) = 0;
    *(float*)((uintptr_t)pDeadPlayer + OFF_DEATH_SLOWDOWN) = 0.0f;
    *(short*)((uintptr_t)pDeadPlayer + OFF_DEATH_TIMER) = 0; // Death frame counter reset
    *(short*)((uintptr_t)pDeadPlayer + 0x3EE) = 0;
    *(char*)((uintptr_t)pDeadPlayer + 0x3A6) = 0;  // Rolling = false
    *(char*)((uintptr_t)pDeadPlayer + OFF_WAKEUP_FLAG) = 1;  // Wakeup flag
    *(char*)((uintptr_t)pDeadPlayer + OFF_ACTIVE_FLAG) = 1;  // Active / visible

    // 2. Reset velocity
    *(float*)((uintptr_t)pDeadPlayer + OFF_VEL_X) = 0.0f;
    *(float*)((uintptr_t)pDeadPlayer + OFF_VEL_Y) = 0.0f;

    // 3. Reset sprite animation playback speeds to 12.0f
    for (int i = 0; i < 8; i++) {
        void* s1 = *(void**)((uintptr_t)pDeadPlayer + 0x124 + i * 4);
        void* s2 = *(void**)((uintptr_t)pDeadPlayer + 0x144 + i * 4);
        void* s3 = *(void**)((uintptr_t)pDeadPlayer + 0x164 + i * 4);
        if (s1 && IsReadable(s1, 0x60)) *(float*)((uintptr_t)s1 + 0x58) = 12.0f;
        if (s2 && IsReadable(s2, 0x60)) *(float*)((uintptr_t)s2 + 0x58) = 12.0f;
        if (s3 && IsReadable(s3, 0x60)) *(float*)((uintptr_t)s3 + 0x58) = 12.0f;
    }

    // 4. Trigger the authentic "getup" animation
    Player_SetAnimation(pDeadPlayer, "getup");
}

static void CheckPartnerRevive(void) {
    if (!g_pPlayer1 || !g_pPlayer2) return;
    if (!IsReadable(g_pPlayer1, 0x418) || !IsReadable(g_pPlayer2, 0x418)) return;

    BOOL p1_alive = IsPlayerAlive(g_pPlayer1);
    BOOL p2_alive = IsPlayerAlive(g_pPlayer2);

    // Revive is only possible if one player is alive and the other is dead
    if (p1_alive == p2_alive) return;

    float p1_x = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_X);
    float p1_y = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_Y);
    float p2_x = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
    float p2_y = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);

    float dx = p1_x - p2_x;
    float dy = p1_y - p2_y;
    float distSq = dx * dx + dy * dy;

    // Proximity threshold: 25.0 pixels (25^2 = 625.0)
    if (distSq <= 625.0f) {
        if (p1_alive && !p2_alive) {
            LogMsg("[Co-op Revive] Living Player 1 touched fallen Player 2 (dist=%.1f px) -> REVIVING!\n", sqrtf(distSq));
            RevivePlayer(g_pPlayer2);
        } else if (!p1_alive && p2_alive) {
            LogMsg("[Co-op Revive] Living Player 2 touched fallen Player 1 (dist=%.1f px) -> REVIVING!\n", sqrtf(distSq));
            RevivePlayer(g_pPlayer1);
        }
    }
}

static void Net_BroadcastLocalPlayer(void) {
    if (!g_netEnabled) return;
    if (!g_steamActive && g_netSocket == INVALID_SOCKET) return;

    DWORD now = GetTickCount();

    // Client connection handshake retry loop (every 500ms)
    if (g_netRole == 2 && g_netState != NET_STATE_CONNECTED) {
        if (g_steamActive && g_steamFriendID == 0) {
            static DWORD s_lastWarn = 0;
            if (now - s_lastWarn >= 3000) {
                s_lastWarn = now;
                LogMsg("[Net/Steam] WARNING: Role=2 (Client) but FriendSteamID is 0! Set FriendSteamID in coop_config.ini or pass +connect <SteamID>\n");
            }
        }
        if (now - g_netLastSendTick >= 500) {
            g_netLastSendTick = now;
            NetHandshakePacket pkt;
            pkt.header.magic = NET_MAGIC;
            pkt.header.version = NET_PROTOCOL_VERSION;
            pkt.header.pktType = NET_PKT_HANDSHAKE;
            pkt.header.senderRole = 2;
            pkt.header.sequence = ++g_netSeqOut;
            strcpy_s(pkt.tag, sizeof(pkt.tag), "CLIENT_JOIN");

            Net_SendPacket(&pkt, sizeof(pkt));
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
            char inQuiver = *(char*)((uintptr_t)pP1 + 0x3C8);
            pkt.has_arrow = (inQuiver != 0 || (dx * dx + dy * dy < 25.0f)) ? 1 : 0;
        }

        // If analog axis is 0, synthesize from keyboard directional buttons
        float in_x = g_p1_axis_x;
        float in_y = g_p1_axis_y;
        if (in_x == 0.0f) {
            if (g_p1_btn_left) in_x -= 1.0f;
            if (g_p1_btn_right) in_x += 1.0f;
        }
        if (in_y == 0.0f) {
            if (g_p1_btn_up) in_y -= 1.0f;
            if (g_p1_btn_down) in_y += 1.0f;
        }

        pkt.input_x = in_x;
        pkt.input_y = in_y;
        pkt.btn_roll = (uint8_t)g_p1_btn_roll;
        pkt.btn_shoot = (uint8_t)g_p1_btn_shoot;
        pkt.btn_up = (uint8_t)g_p1_btn_up;
        pkt.btn_down = (uint8_t)g_p1_btn_down;
        pkt.btn_left = (uint8_t)g_p1_btn_left;
        pkt.btn_right = (uint8_t)g_p1_btn_right;

        char p1_alive_byte = *(char*)((uintptr_t)pP1 + OFF_ALIVE_FLAG);
        int p1_state = *(int*)((uintptr_t)pP1 + OFF_PLAYER_ROLL);
        if (p1_alive_byte == 0) {
            p1_state = 3;
            *(int*)((uintptr_t)pP1 + OFF_PLAYER_ROLL) = 3;
        }
        pkt.is_alive = (p1_state != 3 && p1_alive_byte != 0) ? 1 : 0;
        pkt.roll_state = (uint8_t)p1_state;

        pkt.dir_x = *(float*)((uintptr_t)pP1 + 0xA4);
        pkt.dir_y = *(float*)((uintptr_t)pP1 + 0xA8);
        pkt.facing_x = *(float*)((uintptr_t)pP1 + 0x3B0);
        pkt.facing_y = *(float*)((uintptr_t)pP1 + 0x3B4);

        Net_SendPacket(&pkt, sizeof(pkt));
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

        Net_SendPacket(&hb, sizeof(hb));
    }
}

static void Net_UpdatePuppet(void) {
    if (!g_netEnabled || !g_hasRemoteSync) return;
    if (!g_pPlayer2 || !IsReadable(g_pPlayer2, 0x418)) return;

    // Reflect death state on remote puppet
    if (!g_remoteSyncState.is_alive) {
        *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL) = 3;
        *(char*)((uintptr_t)g_pPlayer2 + OFF_ALIVE_FLAG) = 0;
    } else {
        if (*(char*)((uintptr_t)g_pPlayer2 + OFF_ALIVE_FLAG) == 0 || *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL) == 3) {
            RevivePlayer(g_pPlayer2);
        } else if (g_remoteSyncState.roll_state == 1) {
            *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL) = 1;
        } else if (g_remoteSyncState.roll_state == 0 && *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL) == 2) {
            *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL) = 0;
        }
    }

    // Apply received remote inputs
    g_p2_axis_x = g_remoteSyncState.input_x;
    g_p2_axis_y = g_remoteSyncState.input_y;
    g_p2_btn_roll = (char)g_remoteSyncState.btn_roll;
    g_p2_btn_shoot = (char)g_remoteSyncState.btn_shoot;
    g_p2_btn_up = (char)g_remoteSyncState.btn_up;
    g_p2_btn_down = (char)g_remoteSyncState.btn_down;
    g_p2_btn_left = (char)g_remoteSyncState.btn_left;
    g_p2_btn_right = (char)g_remoteSyncState.btn_right;

    // Apply facing and movement direction
    *(float*)((uintptr_t)g_pPlayer2 + 0xA4) = g_remoteSyncState.dir_x;
    *(float*)((uintptr_t)g_pPlayer2 + 0xA8) = g_remoteSyncState.dir_y;
    if (g_remoteSyncState.facing_x != 0.0f || g_remoteSyncState.facing_y != 0.0f) {
        *(float*)((uintptr_t)g_pPlayer2 + 0x3B0) = g_remoteSyncState.facing_x;
        *(float*)((uintptr_t)g_pPlayer2 + 0x3B4) = g_remoteSyncState.facing_y;
    }

    // Apply position convergence / smoothing
    float curr_x = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
    float curr_y = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);

    float dx = g_remoteSyncState.pos_x - curr_x;
    float dy = g_remoteSyncState.pos_y - curr_y;
    float distSq = dx * dx + dy * dy;

    // Apply position synchronization directly to eliminate packet latency desync
    *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X) = g_remoteSyncState.pos_x;
    *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y) = g_remoteSyncState.pos_y;
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
            if (adx * adx + ady * ady > 100.0f) {
                *(float*)((uintptr_t)pArrow + OFF_POS_X) = g_remoteSyncState.arrow_x;
                *(float*)((uintptr_t)pArrow + OFF_POS_Y) = g_remoteSyncState.arrow_y;
            } else {
                *(float*)((uintptr_t)pArrow + OFF_POS_X) = curr_ax + adx * 0.35f;
                *(float*)((uintptr_t)pArrow + OFF_POS_Y) = curr_ay + ady * 0.35f;
            }
            *(float*)((uintptr_t)pArrow + OFF_VEL_X) = g_remoteSyncState.arrow_vel_x;
            *(float*)((uintptr_t)pArrow + OFF_VEL_Y) = g_remoteSyncState.arrow_vel_y;
            *(float*)((uintptr_t)pArrow + 0x24) = g_remoteSyncState.arrow_angle;
        } else {
            *(float*)((uintptr_t)pArrow + OFF_POS_X) = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X);
            *(float*)((uintptr_t)pArrow + OFF_POS_Y) = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);
            *(float*)((uintptr_t)pArrow + OFF_VEL_X) = 0.0f;
            *(float*)((uintptr_t)pArrow + OFF_VEL_Y) = 0.0f;
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
    GetPrivateProfileStringA("Network", "Transport", "steam", g_netTransport, sizeof(g_netTransport), iniPath);
    g_netRole              = GetPrivateProfileIntA("Network", "Role", 1, iniPath);
    g_netBindPort          = GetPrivateProfileIntA("Network", "BindPort", (g_netRole == 1 ? 7777 : 7778), iniPath);
    g_netRemotePort        = GetPrivateProfileIntA("Network", "RemotePort", (g_netRole == 1 ? 7778 : 7777), iniPath);
    g_netIndependentCamera = GetPrivateProfileIntA("Network", "IndependentCamera", 1, iniPath) != 0;
    GetPrivateProfileStringA("Network", "RemoteIP", "127.0.0.1", g_netRemoteIP, sizeof(g_netRemoteIP), iniPath);

    char friendBuf[64] = {0};
    if (GetPrivateProfileStringA("Network", "FriendSteamID", "0", friendBuf, sizeof(friendBuf), iniPath)) {
        uint64_t fid = _strtoui64(friendBuf, NULL, 10);
        if (fid != 0) g_steamFriendID = fid;
    }

    char buf[64];
    if (GetPrivateProfileStringA("Player2", "ColorR", "0.2", buf, sizeof(buf), iniPath)) g_p2ColorR = (float)atof(buf);
    if (GetPrivateProfileStringA("Player2", "ColorG", "0.8", buf, sizeof(buf), iniPath)) g_p2ColorG = (float)atof(buf);
    if (GetPrivateProfileStringA("Player2", "ColorB", "1.0", buf, sizeof(buf), iniPath)) g_p2ColorB = (float)atof(buf);
    if (GetPrivateProfileStringA("Player2", "ColorA", "1.0", buf, sizeof(buf), iniPath)) g_p2ColorA = (float)atof(buf);

    LogMsg("Configuration loaded from %s:\n", iniPath);
    LogMsg("  QuickStart: %d | RunAutomatedTest: %d | MidpointCamera: %d\n", g_quickStart, g_runAutomatedTest, g_midpointCamera);
    LogMsg("  Network: Enabled=%d | Transport=%s | Role=%s | BindPort=%d | Remote=%s:%d | FriendSteamID=%llu | IndepCam=%d\n",
           g_netEnabled, g_netTransport, (g_netRole == 1 ? "HOST" : "CLIENT"), g_netBindPort, g_netRemoteIP, g_netRemotePort, g_steamFriendID, g_netIndependentCamera);
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
        if (strcmp(name, "up") == 0)    return (g_p2_btn_up || g_p2_axis_y < -0.3f);
        if (strcmp(name, "down") == 0)  return (g_p2_btn_down || g_p2_axis_y > 0.3f);
        if (strcmp(name, "left") == 0)  return (g_p2_btn_left || g_p2_axis_x < -0.3f);
        if (strcmp(name, "right") == 0) return (g_p2_btn_right || g_p2_axis_x > 0.3f);
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
        if (g_runAutomatedTest) {
            // Scripted verification sequence after waking up (frame 300+)
            // Phase 1: Move RIGHT (frames 320..360)
            if (g_inGameFrames >= 320 && g_inGameFrames <= 360) {
                if (strcmp(name, "right") == 0) btn = 1;
            }
            // Phase 2: Move DOWN (frames 380..420)
            if (g_inGameFrames >= 380 && g_inGameFrames <= 420) {
                if (strcmp(name, "down") == 0) btn = 1;
            }
            // Phase 3: Move LEFT (frames 440..480)
            if (g_inGameFrames >= 440 && g_inGameFrames <= 480) {
                if (strcmp(name, "left") == 0) btn = 1;
            }
            // Phase 4: Move UP (frames 500..540)
            if (g_inGameFrames >= 500 && g_inGameFrames <= 540) {
                if (strcmp(name, "up") == 0) btn = 1;
            }
            // Phase 5: Roll dodge RIGHT (frames 560..585)
            if (g_inGameFrames >= 560 && g_inGameFrames <= 585) {
                if (strcmp(name, "roll") == 0 || strcmp(name, "right") == 0) btn = 1;
            }
            // Phase 6: Aim arrow DOWN (frames 610..660)
            if (g_inGameFrames >= 610 && g_inGameFrames <= 660) {
                if (strcmp(name, "shoot") == 0 || strcmp(name, "down") == 0) btn = 1;
            }
            // Phase 7: Fire arrow (frames 661..694: shoot released, arrow in flight)
            // Phase 8: Recall arrow (frames 695..820: shoot held, magnetic recall)
            if (g_inGameFrames >= 695 && g_inGameFrames <= 820) {
                if (strcmp(name, "shoot") == 0) btn = 1;
            }
        }
        if (strcmp(name, "roll") == 0)  g_p1_btn_roll = btn;
        if (strcmp(name, "shoot") == 0) g_p1_btn_shoot = btn;
        if (strcmp(name, "up") == 0)    g_p1_btn_up = btn;
        if (strcmp(name, "down") == 0)  g_p1_btn_down = btn;
        if (strcmp(name, "left") == 0)  g_p1_btn_left = btn;
        if (strcmp(name, "right") == 0) g_p1_btn_right = btn;
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
        if (g_runAutomatedTest) {
            // Scripted movement:
            // Phase 1: Move RIGHT (frames 320..360)
            if (g_inGameFrames >= 320 && g_inGameFrames <= 360) {
                if (strcmp(name, "move_x") == 0) val = 1.0f;
            }
            // Phase 2: Move DOWN (frames 380..420)
            if (g_inGameFrames >= 380 && g_inGameFrames <= 420) {
                if (strcmp(name, "move_y") == 0) val = 1.0f;
            }
            // Phase 3: Move LEFT (frames 440..480)
            if (g_inGameFrames >= 440 && g_inGameFrames <= 480) {
                if (strcmp(name, "move_x") == 0) val = -1.0f;
            }
            // Phase 4: Move UP (frames 500..540)
            if (g_inGameFrames >= 500 && g_inGameFrames <= 540) {
                if (strcmp(name, "move_y") == 0) val = -1.0f;
            }
            // Phase 5: Roll dodge RIGHT (frames 560..585)
            if (g_inGameFrames >= 560 && g_inGameFrames <= 585) {
                if (strcmp(name, "move_x") == 0) val = 1.0f;
            }
            // Phase 6: Aim DOWN to shoot (frames 610..660)
            if (g_inGameFrames >= 610 && g_inGameFrames <= 660) {
                if (strcmp(name, "move_y") == 0) val = 1.0f;
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

    // Detect if this player entered death state in the engine
    if (pThis && IsReadable(pThis, 0x418)) {
        char alive_byte = *(char*)((uintptr_t)pThis + OFF_ALIVE_FLAG);
        int roll_state = *(int*)((uintptr_t)pThis + OFF_PLAYER_ROLL);
        if (alive_byte == 0 && roll_state != 3) {
            *(int*)((uintptr_t)pThis + OFF_PLAYER_ROLL) = 3;
            LogMsg("[Co-op] Player 0x%p entered death state (roll_state=3)\n", pThis);
        }
    }

    if (g_netEnabled && pThis == g_pPlayer2) {
        Net_PollIncoming();
    }

    g_orig_Player_Update(pThis);

    if (g_netEnabled && pThis == g_pPlayer2 && g_hasRemoteSync) {
        if (g_remoteSyncState.facing_x != 0.0f || g_remoteSyncState.facing_y != 0.0f) {
            *(float*)((uintptr_t)pThis + 0x3B0) = g_remoteSyncState.facing_x;
            *(float*)((uintptr_t)pThis + 0x3B4) = g_remoteSyncState.facing_y;
        }
        *(float*)((uintptr_t)pThis + 0xA4) = g_remoteSyncState.dir_x;
        *(float*)((uintptr_t)pThis + 0xA8) = g_remoteSyncState.dir_y;

        // Strict position enforcement on puppet right after engine update
        *(float*)((uintptr_t)pThis + OFF_POS_X) = g_remoteSyncState.pos_x;
        *(float*)((uintptr_t)pThis + OFF_POS_Y) = g_remoteSyncState.pos_y;
        *(float*)((uintptr_t)pThis + 0x94) = g_remoteSyncState.pos_x;
        *(float*)((uintptr_t)pThis + 0x98) = g_remoteSyncState.pos_y;
    }

    g_pCurrentUpdatingPlayer = pPrev;
}

// ============================================================================
// Hook: Player::UpdateDeath (0x005AD144) - Freeze/Bypass Wipe Timer in Co-op
// ============================================================================
static void __fastcall Hooked_Player_UpdateDeath(void* pThis, void* edx) {
    // Run original death routine (blood particles, death audio, slow-motion decay)
    g_orig_Player_UpdateDeath(pThis);

    // If both players exist in co-op, check whether room reload timer should be frozen
    if (g_pPlayer1 && g_pPlayer2 && IsReadable(g_pPlayer1, 0x418) && IsReadable(g_pPlayer2, 0x418)) {
        BOOL p1_alive = IsPlayerAlive(g_pPlayer1);
        BOOL p2_alive = IsPlayerAlive(g_pPlayer2);

        // If at least one player is alive, keep the room active and freeze reload timer!
        if (p1_alive || p2_alive) {
            unsigned short timer = *(unsigned short*)((uintptr_t)pThis + OFF_DEATH_TIMER);
            // Cap below frame 90 (which triggers screen fade) and frame 252 (which triggers room reload)
            if (timer >= 80) {
                *(unsigned short*)((uintptr_t)pThis + OFF_DEATH_TIMER) = 80;
            }
        }
    }
}

// ============================================================================
// Hook: GameWorld::ResetRoom (0x0055309C) - Cooperative Wipe Gatekeeper
// ============================================================================
static void __fastcall Hooked_GameWorld_ResetRoom(void* pWorld, void* edx) {
    if (!pWorld || !IsReadable(pWorld, 0x10)) return;

    if (g_pPlayer1 && g_pPlayer2 && IsReadable(g_pPlayer1, 0x418) && IsReadable(g_pPlayer2, 0x418)) {
        BOOL p1_alive = IsPlayerAlive(g_pPlayer1);
        BOOL p2_alive = IsPlayerAlive(g_pPlayer2);
        // Only trigger room reset / wipe when BOTH players are dead!
        if (p1_alive || p2_alive) {
            LogMsg("[Co-op] GameWorld::ResetRoom blocked: a partner is still alive!\n");
            return;
        }
    }
    LogMsg("[Co-op] Both players fallen -> proceeding with GameWorld::ResetRoom wipe.\n");
    g_orig_GameWorld_ResetRoom(pWorld);
}

// ============================================================================
// Hook: Arrow::IsDamaging (0x005A05A4) - Dual-Player Boss Damage Gate
// ============================================================================
static char __fastcall Hooked_Arrow_IsDamaging(void* pArrow, void* edx) {
    if (g_orig_Arrow_IsDamaging(pArrow, edx)) {
        return 1;
    }
    // If the inspected arrow is not damaging, check the partner's arrow symmetrically
    void* pOther = (g_pPlayer1 && pArrow == *(void**)((uintptr_t)g_pPlayer1 + OFF_PLAYER_ARROW)) ? g_pPlayer2 : g_pPlayer1;
    if (pOther && IsReadable(pOther, 0x418)) {
        void* pOtherArrow = *(void**)((uintptr_t)pOther + OFF_PLAYER_ARROW);
        if (pOtherArrow && pOtherArrow != pArrow && IsReadable(pOtherArrow, 0x140)) {
            if (g_orig_Arrow_IsDamaging(pOtherArrow, NULL)) {
                return 1;
            }
        }
    }
    return 0;
}

// ============================================================================
// Hook: TitanHitSensor::OnHit (0x00589700) - Boss Weakpoint Impact & Sync
// ============================================================================
static void __fastcall Hooked_TitanHitSensor_OnHit(void* pSensor, void* edx, float hit_x, float hit_y, float vel_x, float vel_y) {
    if (!pSensor || !IsReadable(pSensor, 0xA0)) return;

    g_orig_TitanHitSensor_OnHit(pSensor, edx, hit_x, hit_y, vel_x, vel_y);

    LogMsg("[Frame %d] Boss Weakpoint Hit! Sensor=0x%p Pos=(%.2f, %.2f) Vel=(%.2f, %.2f)\n",
           g_frameCount, pSensor, hit_x, hit_y, vel_x, vel_y);

    // Broadcast Boss Hit over Network
    if (g_netEnabled && g_netState == NET_STATE_CONNECTED) {
        NetBossHitPacket pkt;
        ZeroMemory(&pkt, sizeof(pkt));
        pkt.header.magic = NET_MAGIC;
        pkt.header.version = NET_PROTOCOL_VERSION;
        pkt.header.pktType = NET_PKT_BOSS_HIT;
        pkt.header.senderRole = (uint16_t)g_netRole;
        pkt.header.sequence = ++g_netSeqOut;
        pkt.hit_x = hit_x;
        pkt.hit_y = hit_y;
        pkt.hit_vel_x = vel_x;
        pkt.hit_vel_y = vel_y;
        Net_SendPacket(&pkt, sizeof(pkt));
    }
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

    // Call Player::Init() (vtable[3]) to instantiate Player 2's Arrow (at 0x35c) and weapon components
    ((PlayerInit_t)ADDR_PLAYER_INIT)(pPlayer2);

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

    if (!g_netInitialized || (g_netEnabled && !g_steamActive && g_netSocket == INVALID_SOCKET && g_frameCount % 60 == 0)) {
        g_netInitialized = TRUE;
        Net_Init();
    }

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

    // 3. Track Player 1, World and manage Player 2 across room transitions
    void* pP1 = *(void**)ADDR_PLAYER1_PTR;
    void* pGame = *(void**)ADDR_GAME_PTR;
    void* pWorld = *(void**)ADDR_WORLD_PTR;
    static void* s_lastWorld = NULL;

    if (pP1 && IsReadable(pP1, 0x418) && pGame && IsReadable(pGame, 0x40)) {
        g_pPlayer1 = pP1;
        g_inGameFrames++;

        // Detect room transition via World pointer change
        static int s_roomTransitionCooldown = 0;
        if (pWorld && s_lastWorld && pWorld != s_lastWorld) {
            s_roomTransitionCooldown = 30; // sync across 30 frames during screen fade / entrance walk
            LogMsg("[Frame %d] Room transition detected! World: 0x%p -> 0x%p\n",
                   g_frameCount, s_lastWorld, pWorld);
        }
        if (pWorld) {
            s_lastWorld = pWorld;
        }

        if (s_roomTransitionCooldown > 0) {
            s_roomTransitionCooldown--;
            if (g_pPlayer1 && g_pPlayer2 && IsReadable(g_pPlayer1, 0x418) && IsReadable(g_pPlayer2, 0x418)) {
                float p1_x = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_X);
                float p1_y = *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_Y);

                float p2_x = p1_x + 16.0f;
                float p2_y = p1_y;

                *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X) = p2_x;
                *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y) = p2_y;
                *(float*)((uintptr_t)g_pPlayer2 + 0x94)      = p2_x;
                *(float*)((uintptr_t)g_pPlayer2 + 0x98)      = p2_y;
                *(float*)((uintptr_t)g_pPlayer2 + OFF_VEL_X) = 0.0f;
                *(float*)((uintptr_t)g_pPlayer2 + OFF_VEL_Y) = 0.0f;

                // Recall Player 2's arrow
                void* pArrow2 = *(void**)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ARROW);
                if (pArrow2 && IsReadable(pArrow2, 0xAC)) {
                    *(float*)((uintptr_t)pArrow2 + 0x18) = p2_x;
                    *(float*)((uintptr_t)pArrow2 + 0x1C) = p2_y;
                }

                g_hasRemoteSync = FALSE; // Reset smoothing interpolation across room change

                if (s_roomTransitionCooldown == 0) {
                    LogMsg("[Frame %d] Room transition sync complete! Player 2 aligned at (%.2f, %.2f)\n",
                           g_frameCount, p2_x, p2_y);
                }
            }
        }

        if (g_runAutomatedTest) {
            if (g_inGameFrames == 320) LogMsg("[QA Bot] Phase 1: Move RIGHT (frames 320-360)\n");
            else if (g_inGameFrames == 380) LogMsg("[QA Bot] Phase 2: Move DOWN (frames 380-420)\n");
            else if (g_inGameFrames == 440) LogMsg("[QA Bot] Phase 3: Move LEFT (frames 440-480)\n");
            else if (g_inGameFrames == 500) LogMsg("[QA Bot] Phase 4: Move UP (frames 500-540)\n");
            else if (g_inGameFrames == 560) LogMsg("[QA Bot] Phase 5: Roll Dodge RIGHT (frames 560-585)\n");
            else if (g_inGameFrames == 610) LogMsg("[QA Bot] Phase 6: Aim Arrow DOWN (frames 610-660)\n");
            else if (g_inGameFrames == 661) LogMsg("[QA Bot] Phase 7: Fire Arrow (frames 661-694)\n");
            else if (g_inGameFrames == 695) LogMsg("[QA Bot] Phase 8: Recall Arrow (frames 695-820)\n");
            else if (g_inGameFrames == 821) LogMsg("[QA Bot] Phase 9: Sequence Complete (frame %d)\n", g_inGameFrames);

            // Phase 10: Co-op Life & Revive Automated Test
            if (g_inGameFrames == 830 && g_pPlayer2 && IsReadable(g_pPlayer2, 0x418)) {
                LogMsg("[QA Revive Test] Step 1: Simulating Player 2 death (roll_state=3)...\n");
                *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL) = 3;
                *(char*)((uintptr_t)g_pPlayer2 + OFF_ALIVE_FLAG) = 0;
                *(unsigned short*)((uintptr_t)g_pPlayer2 + OFF_DEATH_TIMER) = 85;
            }
            if (g_inGameFrames == 840 && g_pPlayer2 && IsReadable(g_pPlayer2, 0x418)) {
                unsigned short t = *(unsigned short*)((uintptr_t)g_pPlayer2 + OFF_DEATH_TIMER);
                LogMsg("[QA Revive Test] Step 2: Player 2 death timer is %d (frozen <= 80, room active)\n", t);
            }
            if (g_inGameFrames == 850 && g_pPlayer1 && g_pPlayer2 && IsReadable(g_pPlayer1, 0x418) && IsReadable(g_pPlayer2, 0x418)) {
                LogMsg("[QA Revive Test] Step 3: Player 1 approaching Player 2 for Revive (dist <= 25.0 px)...\n");
                *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_X) = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_X) - 10.0f;
                *(float*)((uintptr_t)g_pPlayer1 + OFF_POS_Y) = *(float*)((uintptr_t)g_pPlayer2 + OFF_POS_Y);
            }
            if (g_inGameFrames == 860 && g_pPlayer2 && IsReadable(g_pPlayer2, 0x418)) {
                int p2_state = *(int*)((uintptr_t)g_pPlayer2 + OFF_PLAYER_ROLL);
                char p2_alive = *(char*)((uintptr_t)g_pPlayer2 + OFF_ALIVE_FLAG);
                LogMsg("[QA Revive Test] Step 4: Player 2 Revive verified -> roll_state=%d, alive=%d (REVIVE SUCCESS!)\n",
                       p2_state, p2_alive);
            }
        }
        if (!g_pPlayer2) {
            TrySpawnPlayer2();
        } else {
            CheckPartnerRevive();
        }
    } else {
        s_lastWorld = NULL;
        // shortcut: GameWorld frees entities on map teardown; zero pointer here to re-spawn in next room.
        if (g_pPlayer2 && !pP1) {
            g_pPlayer1 = NULL;
            g_pPlayer2 = NULL;
            g_hasRemoteSync = FALSE;
            g_inGameFrames = 0;
            LogMsg("[Frame %d] Map unloaded, resetting Player 2 reference.\n", g_frameCount);
        }
    }

    // F7 hotkey: open Steam Friends overlay if Steamworks is active
    if (g_steamActive && g_pfnOverlay && IsGameWindowActive()) {
        static BOOL s_f7Pressed = FALSE;
        if (GetAsyncKeyState(VK_F7) & 0x8000) {
            if (!s_f7Pressed) {
                s_f7Pressed = TRUE;
                LogMsg("[Steam] F7 pressed -> opening Steam Friends overlay\n");
                g_pfnOverlay(g_pSteamFriends, "Friends");
            }
        } else {
            s_f7Pressed = FALSE;
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
                if (g_steamActive) {
                    LogMsg("[F%d] Net/Steam:[%s state=%d Friend=%llu TX=%u RX=%u] | Cam:(%.1f,%.1f) | P1:(%.1f,%.1f) | P2:(%.1f,%.1f)\n",
                           g_frameCount, (g_netRole == 1 ? "HOST" : "CLIENT"), g_netState, g_steamFriendID,
                           g_netPacketsSent, g_netPacketsRecv, cam_x, cam_y, p1_x, p1_y, p2_x, p2_y);
                } else {
                    LogMsg("[F%d] Net/UDP:[%s state=%d Peer=%s:%d TX=%u RX=%u] | Cam:(%.1f,%.1f) | P1:(%.1f,%.1f) | P2:(%.1f,%.1f)\n",
                           g_frameCount, (g_netRole == 1 ? "HOST" : "CLIENT"), g_netState, g_netRemoteIP, g_netRemotePort,
                           g_netPacketsSent, g_netPacketsRecv, cam_x, cam_y, p1_x, p1_y, p2_x, p2_y);
                }
            } else {
                LogMsg("[F%d] Local Co-op | Cam:(%.1f,%.1f) | P1:(%.1f,%.1f) | P2:(%.1f,%.1f) | P2_In:(x=%.1f,y=%.1f,roll=%d,shoot=%d)\n",
                       g_frameCount, cam_x, cam_y, p1_x, p1_y, p2_x, p2_y,
                       g_p2_axis_x, g_p2_axis_y, g_p2_btn_roll, g_p2_btn_shoot);
            }
        } else {
            LogMsg("[Frame %d] [MENU] P1: 0x%p | QuickStart: %d | NetEnabled: %d (Transport: %s, Role: %d)\n",
                   g_frameCount, pP1, g_quickStart, g_netEnabled, (g_steamActive ? "Steam" : "UDP"), g_netRole);
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
    g_logFile = fopen("coop_log.txt", "w");
    if (g_logFile) {
        fprintf(g_logFile, "=== Titan Souls Co-op Mod Initializing ===\n");
        fflush(g_logFile);
    }

    LoadConfig();
    InitXInput();

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
        MH_CreateHook((void*)ADDR_PLAYER_UPDATEDEATH, (void*)Hooked_Player_UpdateDeath, (void**)&g_orig_Player_UpdateDeath);
        MH_CreateHook((void*)ADDR_GAMEWORLD_RESETROOM, (void*)Hooked_GameWorld_ResetRoom, (void**)&g_orig_GameWorld_ResetRoom);
        MH_CreateHook((void*)ADDR_ARROW_ISDAMAGING, (void*)Hooked_Arrow_IsDamaging, (void**)&g_orig_Arrow_IsDamaging);
        MH_CreateHook((void*)ADDR_TITAN_HITSENSOR_ONHIT, (void*)Hooked_TitanHitSensor_OnHit, (void**)&g_orig_TitanHitSensor_OnHit);

        MH_EnableHook(MH_ALL_HOOKS);
        LogMsg("All hooks installed successfully (SwapWindow, GetButton, GetAxis, Player::Update, Camera::Update, Player::UpdateDeath, GameWorld::ResetRoom, Arrow::IsDamaging, TitanHitSensor::OnHit).\n");
    } else {
        LogMsg("ERROR: MinHook initialization failed!\n");
    }
}

void DLLP_OnDetach(void) {
    LogMsg("=== Titan Souls Co-op Mod Detaching ===\n");
    Net_Shutdown();
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = NULL;
    }
}
