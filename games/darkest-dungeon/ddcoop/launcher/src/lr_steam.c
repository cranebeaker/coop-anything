#include "lr_steam.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define APPID_SPACEWAR 480
#define LOBBY_KEY "ddcoop"
#define LOBBY_VALUE "2" /* requires the launcher with verified preparation and guest recovery */

/* Callback ids and send flags (Steamworks SDK). */
#define CB_GAME_LOBBY_JOIN_REQUESTED 333
#define CB_GAME_RICH_PRESENCE_JOIN_REQUESTED 337
#define CB_LOBBY_ENTER 504
#define CB_LOBBY_CHAT_UPDATE 506
#define CB_LOBBY_CREATED 513
#define CB_API_CALL_COMPLETED 703
#define CB_SESSION_REQUEST 1251
#define CB_SESSION_FAILED 1252
#define SEND_RELIABLE_NO_NAGLE (8 | 1)
#define RESULT_OK 1
#define RESULT_LIMIT_EXCEEDED 25
#define CONFIG_SEND_BUFFER_SIZE 9
#define IDENTITY_SIZE 160 /* SteamNetworkingIdentity is 136 bytes; extra room as KeeperFX does */

#pragma pack(push, 8)
typedef struct {
    int32_t user;
    int32_t callback;
    uint8_t *param;
    int32_t param_size;
} CallbackMsg;
typedef struct {
    uint64_t call;
    int32_t callback;
    uint32_t param_size;
} CallCompleted;
typedef struct {
    int32_t result;
    uint64_t lobby;
} LobbyCreated;
typedef struct {
    uint64_t lobby;
    uint32_t permissions;
    uint8_t locked;
    uint32_t response;
} LobbyEnter;
typedef struct {
    uint64_t lobby, user_changed, making_change;
    uint32_t state;
} LobbyChatUpdate;
typedef struct {
    uint64_t lobby, friend_id;
} JoinRequested;
typedef struct {
    uint64_t friend_id;
    char connect[256];
} RichJoinRequested;
typedef struct {
    uint64_t game_id;
    uint32_t game_ip;
    uint16_t game_port, query_port;
    uint64_t lobby;
} FriendGameInfo;
#pragma pack(pop)

/* Steamworks bool returns fill only AL (1 byte); declaring them int reads 3 bytes of leftover register junk,
 * so "false" can look true (GetNextCallback then never ends: the 1.0.0 hang). Every bool here is _Bool. */
typedef _Bool(__cdecl *fn_init)(void);
typedef void(__cdecl *fn_void)(void);
typedef int32_t(__cdecl *fn_pipe)(void);
typedef void(__cdecl *fn_run_frame)(int32_t);
typedef _Bool(__cdecl *fn_next_cb)(int32_t, CallbackMsg *);
typedef _Bool(__cdecl *fn_call_result)(int32_t, uint64_t, void *, int, int, uint8_t *);
typedef void *(__cdecl *fn_iface)(void);
typedef uint64_t(__cdecl *fn_u64_self)(void *);
typedef const char *(__cdecl *fn_str_self)(void *);
typedef int(__cdecl *fn_int_self_int)(void *, int);
typedef uint64_t(__cdecl *fn_u64_self_int_int)(void *, int, int);
typedef const char *(__cdecl *fn_str_self_u64)(void *, uint64_t);
typedef int(__cdecl *fn_int_self_u64)(void *, uint64_t);
typedef _Bool(__cdecl *fn_game_played)(void *, uint64_t, FriendGameInfo *);
typedef _Bool(__cdecl *fn_set_rp)(void *, const char *, const char *);
typedef void(__cdecl *fn_void_self)(void *);
typedef const char *(__cdecl *fn_get_rp)(void *, uint64_t, const char *);
typedef void(__cdecl *fn_void_self_u64)(void *, uint64_t);
typedef uint64_t(__cdecl *fn_create_lobby)(void *, int, int);
typedef uint64_t(__cdecl *fn_join_lobby)(void *, uint64_t);
typedef _Bool(__cdecl *fn_set_lobby_data)(void *, uint64_t, const char *, const char *);
typedef const char *(__cdecl *fn_get_lobby_data)(void *, uint64_t, const char *);
typedef uint64_t(__cdecl *fn_member)(void *, uint64_t, int);
typedef _Bool(__cdecl *fn_invite)(void *, uint64_t, uint64_t);
typedef int(__cdecl *fn_send_msg)(void *, const void *, const void *, uint32_t, int, int);
typedef int(__cdecl *fn_recv_msgs)(void *, int, void **, int);
typedef _Bool(__cdecl *fn_session)(void *, const void *);
typedef void(__cdecl *fn_id_set)(void *, uint64_t);
typedef uint64_t(__cdecl *fn_id_get)(const void *);
typedef void(__cdecl *fn_msg_release)(void *);
typedef _Bool(__cdecl *fn_set_cfg_int)(void *, int, int32_t);
typedef _Bool(__cdecl *fn_bool_self)(void *);
typedef uint32_t(__cdecl *fn_u32_self)(void *);
typedef _Bool(__cdecl *fn_call_done)(void *, uint64_t, uint8_t *);
typedef int(__cdecl *fn_call_reason)(void *, uint64_t);

static struct {
    HMODULE dll;
    fn_init Init;
    fn_void Shutdown, DispatchInit;
    fn_pipe GetPipe;
    fn_run_frame RunFrame, FreeLastCallback;
    fn_next_cb GetNextCallback;
    fn_call_result GetCallResult;
    fn_iface User, Friends, Matchmaking, Messages, Utils;
    fn_u64_self GetSteamID;
    fn_str_self GetPersonaName;
    fn_int_self_int GetFriendCount;
    fn_u64_self_int_int GetFriendByIndex;
    fn_str_self_u64 GetFriendPersonaName;
    fn_int_self_u64 GetFriendPersonaState;
    fn_game_played GetFriendGamePlayed;
    fn_set_rp SetRichPresence;
    fn_void_self ClearRichPresence;
    fn_get_rp GetFriendRichPresence;
    fn_void_self_u64 RequestFriendRichPresence, ActivateInviteDialog;
    fn_create_lobby CreateLobby;
    fn_join_lobby JoinLobby;
    fn_void_self_u64 LeaveLobby;
    fn_set_lobby_data SetLobbyData;
    fn_get_lobby_data GetLobbyData;
    fn_join_lobby GetLobbyOwner; /* (self, lobby) -> owner: same shape as JoinLobby */
    fn_int_self_u64 GetNumLobbyMembers;
    fn_member GetLobbyMemberByIndex;
    fn_invite InviteUserToLobby;
    fn_send_msg SendMessageToUser;
    fn_recv_msgs ReceiveMessagesOnChannel;
    fn_session AcceptSession, CloseSession;
    fn_id_set IdentitySet;
    fn_id_get IdentityGet;
    fn_msg_release MessageRelease;
    fn_iface UtilsIface;
    fn_void_self InitRelay;
    fn_set_cfg_int SetConfigInt;
    fn_bool_self LoggedOn;
    fn_iface SteamUtils;
    fn_u32_self GetAppID;
    fn_call_done IsCallCompleted;
    fn_call_reason CallFailureReason;
    void *sutils;
    DWORD create_started, last_call_check, last_summary;
    int cb_counts[16];
    int cb_ids[16];
    int cb_logged;

    void *user, *friends, *mm, *msgs, *utils;
    int32_t pipe;
    int ready;
    lr_steam_callbacks cb;
    uint64_t me, lobby, create_call, join_call, entered;
    char my_name[LR_FRIEND_NAME];
    DWORD last_rp_request;
    /* Session requests from peers not (yet) in our lobby: Steam may announce a friend's session before
     * the lobby member list shows them, so a refusal is re-checked for a while instead of final. */
    struct {
        uint8_t identity[IDENTITY_SIZE];
        uint64_t peer;
        DWORD since;
    } pending[8];
} S;

static void slog(const char *fmt, ...) {
    if (!S.cb.log) return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    S.cb.log(S.cb.ctx, line);
}

static void emit(lr_steam_event_type t, uint64_t lobby, uint64_t user, const char *text) {
    lr_steam_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = t;
    ev.lobby = lobby;
    ev.user = user;
    if (text) snprintf(ev.text, sizeof ev.text, "%s", text);
    if (S.cb.on_event) S.cb.on_event(S.cb.ctx, &ev);
}

#define LOAD(field, name)                                                                                              \
    do {                                                                                                               \
        FARPROC proc_ = GetProcAddress(S.dll, name);                                                                   \
        memcpy(&S.field, &proc_, sizeof proc_);                                                                        \
        if (!S.field) {                                                                                                \
            snprintf(err, cap, "steam_api64.dll mist %s (te oud?). Draai Repair DDCoop.bat.", name);                    \
            return 0;                                                                                                  \
        }                                                                                                              \
    } while (0)

int lr_steam_start(const wchar_t *dll_path, const lr_steam_callbacks *cb, char *err, size_t cap) {
    memset(&S, 0, sizeof S);
    S.cb = *cb;
    /* Steam reads the AppID from this variable before steam_appid.txt; the game is started with it removed. */
    SetEnvironmentVariableW(L"SteamAppId", L"480");
    SetEnvironmentVariableW(L"SteamGameId", L"480");
    S.dll = LoadLibraryW(dll_path);
    if (!S.dll) {
        snprintf(err, cap, "steam_api64.dll niet gevonden naast DDCoop.exe. Draai Repair DDCoop.bat.");
        return 0;
    }
    LOAD(Init, "SteamAPI_Init");
    LOAD(Shutdown, "SteamAPI_Shutdown");
    LOAD(DispatchInit, "SteamAPI_ManualDispatch_Init");
    LOAD(GetPipe, "SteamAPI_GetHSteamPipe");
    LOAD(RunFrame, "SteamAPI_ManualDispatch_RunFrame");
    LOAD(GetNextCallback, "SteamAPI_ManualDispatch_GetNextCallback");
    LOAD(FreeLastCallback, "SteamAPI_ManualDispatch_FreeLastCallback");
    LOAD(GetCallResult, "SteamAPI_ManualDispatch_GetAPICallResult");
    LOAD(User, "SteamAPI_SteamUser_v021");
    LOAD(Friends, "SteamAPI_SteamFriends_v017");
    LOAD(Matchmaking, "SteamAPI_SteamMatchmaking_v009");
    LOAD(Messages, "SteamAPI_SteamNetworkingMessages_SteamAPI_v002");
    LOAD(UtilsIface, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
    LOAD(GetSteamID, "SteamAPI_ISteamUser_GetSteamID");
    LOAD(GetPersonaName, "SteamAPI_ISteamFriends_GetPersonaName");
    LOAD(GetFriendCount, "SteamAPI_ISteamFriends_GetFriendCount");
    LOAD(GetFriendByIndex, "SteamAPI_ISteamFriends_GetFriendByIndex");
    LOAD(GetFriendPersonaName, "SteamAPI_ISteamFriends_GetFriendPersonaName");
    LOAD(GetFriendPersonaState, "SteamAPI_ISteamFriends_GetFriendPersonaState");
    LOAD(GetFriendGamePlayed, "SteamAPI_ISteamFriends_GetFriendGamePlayed");
    LOAD(SetRichPresence, "SteamAPI_ISteamFriends_SetRichPresence");
    LOAD(ClearRichPresence, "SteamAPI_ISteamFriends_ClearRichPresence");
    LOAD(GetFriendRichPresence, "SteamAPI_ISteamFriends_GetFriendRichPresence");
    LOAD(RequestFriendRichPresence, "SteamAPI_ISteamFriends_RequestFriendRichPresence");
    LOAD(ActivateInviteDialog, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog");
    LOAD(CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
    LOAD(JoinLobby, "SteamAPI_ISteamMatchmaking_JoinLobby");
    LOAD(LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");
    LOAD(SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    LOAD(GetLobbyData, "SteamAPI_ISteamMatchmaking_GetLobbyData");
    LOAD(GetLobbyOwner, "SteamAPI_ISteamMatchmaking_GetLobbyOwner");
    LOAD(GetNumLobbyMembers, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");
    LOAD(GetLobbyMemberByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
    LOAD(InviteUserToLobby, "SteamAPI_ISteamMatchmaking_InviteUserToLobby");
    LOAD(SendMessageToUser, "SteamAPI_ISteamNetworkingMessages_SendMessageToUser");
    LOAD(ReceiveMessagesOnChannel, "SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
    LOAD(AcceptSession, "SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser");
    LOAD(CloseSession, "SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser");
    LOAD(IdentitySet, "SteamAPI_SteamNetworkingIdentity_SetSteamID64");
    LOAD(IdentityGet, "SteamAPI_SteamNetworkingIdentity_GetSteamID64");
    LOAD(MessageRelease, "SteamAPI_SteamNetworkingMessage_t_Release");
    LOAD(InitRelay, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess");
    LOAD(SetConfigInt, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueInt32");
    LOAD(LoggedOn, "SteamAPI_ISteamUser_BLoggedOn");
    LOAD(SteamUtils, "SteamAPI_SteamUtils_v010");
    LOAD(GetAppID, "SteamAPI_ISteamUtils_GetAppID");
    LOAD(IsCallCompleted, "SteamAPI_ISteamUtils_IsAPICallCompleted");
    LOAD(CallFailureReason, "SteamAPI_ISteamUtils_GetAPICallFailureReason");

    if (!S.Init()) {
        snprintf(err, cap, "Steam reageert niet. Start Steam, log in, en open DDCoop opnieuw.");
        return 0;
    }
    /* steam_api.h: "must be called after SteamAPI_Init". Before it (1.0.0-1.0.2) Init reset the mode and not one
     * callback or call result ever reached us: the lobby was made but never seen. */
    S.DispatchInit();
    S.pipe = S.GetPipe();
    S.user = S.User();
    S.friends = S.Friends();
    S.mm = S.Matchmaking();
    S.msgs = S.Messages();
    S.utils = S.UtilsIface();
    if (!S.user || !S.friends || !S.mm || !S.msgs || !S.utils) {
        snprintf(err, cap, "Steam gaf niet alle onderdelen (versie?). Draai Repair DDCoop.bat.");
        S.Shutdown();
        return 0;
    }
    S.sutils = S.SteamUtils();
    S.me = S.GetSteamID(S.user);
    snprintf(S.my_name, sizeof S.my_name, "%s", S.GetPersonaName(S.friends));
    S.InitRelay(S.utils);
    S.SetConfigInt(S.utils, CONFIG_SEND_BUFFER_SIZE, 8 * 1024 * 1024);
    S.ready = 1;
    slog("steam: ready as %llu, AppID %u, logged on: %s", (unsigned long long)S.me, S.sutils ? S.GetAppID(S.sutils) : 0,
         S.LoggedOn(S.user) ? "yes" : "NO (offline mode?)");
    return 1;
}

void lr_steam_stop(void) {
    if (!S.ready) return;
    lr_steam_leave_lobby();
    S.ClearRichPresence(S.friends);
    S.Shutdown();
    S.ready = 0;
}

int lr_steam_ready(void) { return S.ready; }
int lr_steam_logged_on(void) { return S.ready && S.LoggedOn(S.user); }
uint64_t lr_steam_me(void) { return S.me; }
const char *lr_steam_my_name(void) { return S.my_name; }
uint64_t lr_steam_lobby(void) { return S.lobby; }

uint64_t lr_steam_lobby_owner(void) { return S.lobby ? S.GetLobbyOwner(S.mm, S.lobby) : 0; }

int lr_steam_is_member(uint64_t user) {
    if (!S.lobby) return 0;
    int n = S.GetNumLobbyMembers(S.mm, S.lobby);
    for (int i = 0; i < n; i++)
        if (S.GetLobbyMemberByIndex(S.mm, S.lobby, i) == user) return 1;
    return 0;
}

int lr_steam_send(void *unused, uint64_t peer, int channel, const uint8_t *buf, size_t len) {
    (void)unused;
    if (!S.ready) return -1;
    uint8_t id[IDENTITY_SIZE] = {0};
    S.IdentitySet(id, peer);
    int r = S.SendMessageToUser(S.msgs, id, buf, (uint32_t)len, SEND_RELIABLE_NO_NAGLE, channel);
    if (r == RESULT_OK) return 1;
    if (r == RESULT_LIMIT_EXCEEDED) return 0;
    slog("steam: send to %llu failed, result %d; resetting the session", (unsigned long long)peer, r);
    S.CloseSession(S.msgs, id);
    return 0; /* try again: the next send opens a new session */
}

void lr_steam_close_session(uint64_t peer) {
    if (!S.ready) return;
    uint8_t id[IDENTITY_SIZE] = {0};
    S.IdentitySet(id, peer);
    S.CloseSession(S.msgs, id);
}

static void set_presence(void) {
    char id[32];
    snprintf(id, sizeof id, "%llu", (unsigned long long)S.lobby);
    S.SetRichPresence(S.friends, "ddcoop_lobby", id);
    char connect[64];
    snprintf(connect, sizeof connect, "+connect_lobby %s", id);
    S.SetRichPresence(S.friends, "connect", connect);
    S.SetRichPresence(S.friends, "status", "DDCoop: Darkest Dungeon co-op host");
}

static void lobby_created(const LobbyCreated *lc) {
    if (lc->result != RESULT_OK || lc->lobby == 0) {
        char t[160];
        snprintf(t, sizeof t, "Steam kon geen lobby maken (code %d). Probeer het zo opnieuw.", lc->result);
        emit(LR_EV_LOBBY_FAILED, 0, 0, t);
        return;
    }
    if (S.lobby == lc->lobby) return; /* arrives twice on real Steam: call result and broadcast */
    if (S.create_call == 0) {        /* we gave up on it (20 s timeout or Stop): do not keep it open */
        S.LeaveLobby(S.mm, lc->lobby);
        slog("steam: late lobby %llu left again", (unsigned long long)lc->lobby);
        return;
    }
    S.lobby = lc->lobby;
    S.create_call = 0; /* done: stop asking Steam about it */
    S.SetLobbyData(S.mm, S.lobby, LOBBY_KEY, LOBBY_VALUE);
    S.SetLobbyData(S.mm, S.lobby, "ddcoop_host", S.my_name);
    set_presence();
    slog("steam: lobby %llu created", (unsigned long long)S.lobby);
    emit(LR_EV_LOBBY_CREATED, S.lobby, S.me, NULL);
}

static void lobby_entered(const LobbyEnter *le) {
    if (le->lobby != S.lobby && S.join_call == 0) return;
    if (S.GetLobbyOwner(S.mm, le->lobby) == S.me) return; /* our own lobby (host side) */
    if (S.entered == le->lobby) return;                   /* call result and broadcast: handle once */
    S.join_call = 0;
    if (le->response != 1) {
        char t[160];
        snprintf(t, sizeof t, "Kon de lobby niet binnen (code %u): vol, gesloten of de host is weg.", le->response);
        emit(LR_EV_LOBBY_FAILED, le->lobby, 0, t);
        S.lobby = 0;
        return;
    }
    const char *v = S.GetLobbyData(S.mm, le->lobby, LOBBY_KEY);
    if (!v || strcmp(v, LOBBY_VALUE) != 0) {
        S.LeaveLobby(S.mm, le->lobby);
        S.lobby = 0;
        emit(LR_EV_LOBBY_FAILED, le->lobby, 0, "Dat is geen DDCoop-spel (iemand speelt gewoon Spacewar).");
        return;
    }
    S.lobby = le->lobby;
    S.entered = le->lobby;
    uint64_t owner = S.GetLobbyOwner(S.mm, S.lobby);
    slog("steam: entered lobby %llu, owner %llu", (unsigned long long)S.lobby, (unsigned long long)owner);
    emit(LR_EV_LOBBY_ENTERED, S.lobby, owner, NULL);
}

static void handle(int id, const uint8_t *param) {
    switch (id) {
    case CB_LOBBY_CREATED:
        lobby_created((const LobbyCreated *)param);
        break;
    case CB_LOBBY_ENTER:
        lobby_entered((const LobbyEnter *)param);
        break;
    case CB_LOBBY_CHAT_UPDATE: {
        const LobbyChatUpdate *u = (const LobbyChatUpdate *)param;
        if (u->lobby != S.lobby) break;
        if (u->state & 0x1) emit(LR_EV_MEMBER_JOINED, u->lobby, u->user_changed, NULL);
        if (u->state & (0x2 | 0x4 | 0x8 | 0x10)) emit(LR_EV_MEMBER_LEFT, u->lobby, u->user_changed, NULL);
        break;
    }
    case CB_GAME_LOBBY_JOIN_REQUESTED: {
        const JoinRequested *j = (const JoinRequested *)param;
        emit(LR_EV_JOIN_REQUESTED, j->lobby, j->friend_id, NULL);
        break;
    }
    case CB_GAME_RICH_PRESENCE_JOIN_REQUESTED: {
        const RichJoinRequested *j = (const RichJoinRequested *)param;
        const char *p = strstr(j->connect, "+connect_lobby ");
        if (p) emit(LR_EV_JOIN_REQUESTED, _strtoui64(p + 15, NULL, 10), j->friend_id, NULL);
        break;
    }
    case CB_SESSION_REQUEST: {
        uint64_t peer = S.IdentityGet(param);
        if (S.cb.accept_session && S.cb.accept_session(S.cb.ctx, peer)) {
            S.AcceptSession(S.msgs, param);
            slog("steam: accepted session from %llu", (unsigned long long)peer);
            break;
        }
        for (int i = 0; i < 8; i++)
            if (!S.pending[i].peer || S.pending[i].peer == peer) {
                memcpy(S.pending[i].identity, param, 136);
                S.pending[i].peer = peer;
                S.pending[i].since = GetTickCount();
                break;
            }
        slog("steam: session from %llu waits (not in our lobby yet)", (unsigned long long)peer);
        break;
    }
    case CB_SESSION_FAILED:
        slog("steam: a session failed (peer %llu); closing it so the next message opens a new one",
             (unsigned long long)S.IdentityGet(param));
        S.CloseSession(S.msgs, param);
        break;
    default:
        break;
    }
}

void lr_steam_pump(void) {
    if (!S.ready) return;
    S.RunFrame(S.pipe);
    CallbackMsg msg;
    while (S.GetNextCallback(S.pipe, &msg)) {
        if (msg.callback == CB_API_CALL_COMPLETED) {
            const CallCompleted *done = (const CallCompleted *)msg.param;
            void *buf = calloc(1, done->param_size ? done->param_size : 1);
            uint8_t failed = 0;
            int got = buf && S.GetCallResult(S.pipe, done->call, buf, (int)done->param_size, done->callback, &failed);
            if (done->call == S.create_call || done->call == S.join_call) /* the others are Steam's own */
                slog("steam: call result %llu for callback %d (%u bytes): %s", (unsigned long long)done->call,
                     done->callback, done->param_size, !got ? "could not be read" : failed ? "FAILED" : "ok");
            if (got) {
                if (!failed) handle(done->callback, buf);
                else if (done->call == S.create_call) emit(LR_EV_LOBBY_FAILED, 0, 0, "Steam kon geen lobby maken.");
                else if (done->call == S.join_call) emit(LR_EV_LOBBY_FAILED, 0, 0, "Kon de lobby niet binnen.");
            }
            free(buf);
        } else {
            if (msg.callback == CB_LOBBY_CREATED || msg.callback == CB_LOBBY_ENTER || msg.callback == CB_LOBBY_CHAT_UPDATE ||
                msg.callback == CB_GAME_LOBBY_JOIN_REQUESTED || msg.callback == CB_SESSION_REQUEST)
                slog("steam: callback %d (%d bytes)", msg.callback, msg.param_size);
            handle(msg.callback, msg.param);
        }
        for (int i = 0; i < 16; i++)
            if (S.cb_ids[i] == msg.callback || S.cb_ids[i] == 0) {
                S.cb_ids[i] = msg.callback;
                S.cb_counts[i]++;
                break;
            }
        S.FreeLastCallback(S.pipe);
    }
    DWORD now = GetTickCount();
    if (S.create_call && !S.lobby && now - S.last_call_check > 1000) {
        S.last_call_check = now;
        uint8_t failed = 0;
        int done = S.IsCallCompleted(S.sutils, S.create_call, &failed);
        slog("steam: lobby call %llu after %lu ms: %s%s (reason %d)", (unsigned long long)S.create_call,
             now - S.create_started, done ? "completed" : "pending", failed ? ", FAILED" : "",
             S.CallFailureReason(S.sutils, S.create_call));
    }
    if (now - S.last_summary > 10000) {
        S.last_summary = now;
        char line[400];
        int n = snprintf(line, sizeof line, "steam: callbacks so far:");
        for (int i = 0; i < 16 && S.cb_ids[i] && n < (int)sizeof line - 24; i++)
            n += snprintf(line + n, sizeof line - (size_t)n, " %d x%d", S.cb_ids[i], S.cb_counts[i]);
        if (S.cb_logged++ < 3) slog("%s", line);
    }
    for (int i = 0; i < 8; i++) {
        if (!S.pending[i].peer) continue;
        if (S.cb.accept_session && S.cb.accept_session(S.cb.ctx, S.pending[i].peer)) {
            S.AcceptSession(S.msgs, S.pending[i].identity);
            slog("steam: accepted session from %llu (now in our lobby)", (unsigned long long)S.pending[i].peer);
            S.pending[i].peer = 0;
        } else if (GetTickCount() - S.pending[i].since > 30000) {
            slog("steam: refused session from %llu (never joined our lobby)", (unsigned long long)S.pending[i].peer);
            S.pending[i].peer = 0;
        }
    }
    for (int ch = 0; ch < 2; ch++) {
        void *batch[64];
        int n = S.ReceiveMessagesOnChannel(S.msgs, ch, batch, 64);
        for (int i = 0; i < n; i++) {
            const uint8_t *m = batch[i];
            const uint8_t *data = *(const uint8_t *const *)m; /* x64 layout: data pointer at 0 */
            int32_t size = *(const int32_t *)(m + 8);          /* size at 8 */
            uint64_t sender = S.IdentityGet(m + 16);           /* sender identity at 16 */
            if (S.cb.on_message && size > 0) S.cb.on_message(S.cb.ctx, sender, ch, data, (size_t)size);
            S.MessageRelease(batch[i]);
        }
    }
}

int lr_steam_host_lobby(void) {
    if (!S.ready) return 0;
    lr_steam_leave_lobby();
    S.create_call = S.CreateLobby(S.mm, 1 /* friends only */, 4);
    S.create_started = GetTickCount();
    slog("steam: CreateLobby call %llu (logged on: %s)", (unsigned long long)S.create_call, S.LoggedOn(S.user) ? "yes" : "NO");
    return S.create_call != 0;
}

int lr_steam_join_lobby(uint64_t lobby) {
    if (!S.ready || lobby == 0) return 0;
    lr_steam_leave_lobby();
    S.lobby = lobby;
    S.join_call = S.JoinLobby(S.mm, lobby);
    return S.join_call != 0;
}

void lr_steam_leave_lobby(void) {
    if (!S.ready) return;
    if (S.lobby) S.LeaveLobby(S.mm, S.lobby);
    S.lobby = 0;
    S.entered = 0;
    S.create_call = 0;
    S.join_call = 0;
    S.ClearRichPresence(S.friends);
}

int lr_steam_invite(uint64_t friend_id) {
    if (!S.ready || !S.lobby) return 0;
    return S.InviteUserToLobby(S.mm, S.lobby, friend_id);
}

static int by_rank(const void *a, const void *b) {
    const lr_friend *x = a, *y = b;
    int rx = x->hosting * 4 + x->online * 2, ry = y->hosting * 4 + y->online * 2;
    if (rx != ry) return ry - rx;
    return _stricmp(x->name, y->name);
}

int lr_steam_friends(lr_friend *out, int max) {
    if (!S.ready) return 0;
    int n = S.GetFriendCount(S.friends, 4 /* immediate friends */), count = 0;
    DWORD now = GetTickCount();
    int request_rp = now - S.last_rp_request > 10000;
    if (request_rp) S.last_rp_request = now;
    for (int i = 0; i < n && count < max; i++) {
        uint64_t id = S.GetFriendByIndex(S.friends, i, 4);
        lr_friend *f = &out[count];
        memset(f, 0, sizeof *f);
        f->id = id;
        snprintf(f->name, sizeof f->name, "%s", S.GetFriendPersonaName(S.friends, id));
        f->online = S.GetFriendPersonaState(S.friends, id) != 0;
        if (!f->online) continue;
        if (request_rp) S.RequestFriendRichPresence(S.friends, id);
        FriendGameInfo gi;
        memset(&gi, 0, sizeof gi);
        if (S.GetFriendGamePlayed(S.friends, id, &gi)) {
            f->in_game = 1;
            if ((gi.game_id & 0xFFFFFF) == APPID_SPACEWAR && gi.lobby) {
                f->hosting = 1;
                f->lobby = gi.lobby;
            }
        }
        /* The host shows as "playing Darkest Dungeon" once the game runs; the rich presence still says DDCoop. */
        const char *rp = S.GetFriendRichPresence(S.friends, id, "ddcoop_lobby");
        if (rp && *rp) {
            uint64_t lobby = _strtoui64(rp, NULL, 10);
            if (lobby) {
                f->hosting = 1;
                f->lobby = lobby;
            }
        }
        count++;
    }
    qsort(out, (size_t)count, sizeof *out, by_rank);
    return count;
}
