/* Steam for the launcher (Windows only): the game's own steam_api64.dll, loaded dynamically through the
 * flat C API, running as AppID 480 "Spacewar" (playbook/networking-steam-spacewar.md in coop-anything).
 * Friends-only lobby, invites, a friends list, and SteamNetworkingMessages for the relay traffic, so no
 * IP address, port forward or firewall rule is ever needed. */
#ifndef LR_STEAM_H
#define LR_STEAM_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define LR_FRIEND_NAME 128

typedef struct {
    uint64_t id;
    char name[LR_FRIEND_NAME]; /* UTF-8 */
    int online;
    int hosting;      /* plays Spacewar in a lobby, or advertises a DDCoop lobby in rich presence */
    uint64_t lobby;   /* the lobby to join when hosting */
    int in_game;      /* plays any game (shown as "in een spel") */
} lr_friend;

typedef enum {
    LR_EV_LOBBY_CREATED,      /* lobby */
    LR_EV_LOBBY_FAILED,       /* create or join failed; text says why */
    LR_EV_LOBBY_ENTERED,      /* lobby, user = owner */
    LR_EV_MEMBER_JOINED,      /* user */
    LR_EV_MEMBER_LEFT,        /* user */
    LR_EV_JOIN_REQUESTED,     /* lobby: the player accepted an invite or clicked Join Game in Steam */
} lr_steam_event_type;

typedef struct {
    lr_steam_event_type type;
    uint64_t lobby, user;
    char text[160];
} lr_steam_event;

typedef struct {
    void *ctx;
    void (*on_message)(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len);
    void (*on_event)(void *ctx, const lr_steam_event *ev);
    int (*accept_session)(void *ctx, uint64_t peer); /* may this peer talk to us? */
    void (*log)(void *ctx, const char *line);
} lr_steam_callbacks;

/* Loads the DLL and starts Steam as AppID 480. err gets a Dutch explanation on failure. */
int lr_steam_start(const wchar_t *dll_path, const lr_steam_callbacks *cb, char *err, size_t cap);
void lr_steam_stop(void);
int lr_steam_ready(void);
int lr_steam_logged_on(void); /* 0 in Steam's offline mode: no lobbies or friends then */

uint64_t lr_steam_me(void);
const char *lr_steam_my_name(void);

/* 1 sent, 0 buffer full (try again), -1 failed. Reliable, no Nagle. */
int lr_steam_send(void *unused, uint64_t peer, int channel, const uint8_t *buf, size_t len);

void lr_steam_pump(void); /* callbacks and incoming messages; call every tick */

int lr_steam_host_lobby(void);            /* async: LR_EV_LOBBY_CREATED / FAILED */
int lr_steam_join_lobby(uint64_t lobby);  /* async: LR_EV_LOBBY_ENTERED / FAILED */
void lr_steam_leave_lobby(void);
uint64_t lr_steam_lobby(void);
uint64_t lr_steam_lobby_owner(void);
int lr_steam_is_member(uint64_t user);
int lr_steam_invite(uint64_t friend_id);
void lr_steam_close_session(uint64_t peer);

int lr_steam_friends(lr_friend *out, int max); /* online friends first, hosts at the top */

#endif
