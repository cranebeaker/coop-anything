/* Launcher session logic (platform independent; launcher/tests/test_lr_core.c).
 *
 * Host: snapshots its campaign when hosting starts, writes ddcoop.ini (role=host, a fresh password, a free
 * local port), and when a friend's launcher says HELLO it sends that snapshot, then relays the runtime's connection.
 * Guest: says HELLO to the host it joined, receives the password and the campaign, backs its own copy of
 * that campaign up, stores the host's, writes ddcoop.ini (role=guest, host=127.0.0.1, our relay port)
 * and asks the window to start the game. Steam (or the mock, or a test) only moves the messages.
 * Hero ownership (launcher/OWNERSHIP.md): the host reads the roster from that snapshot, sends the table to the
 * guest and decides on the guest's ticks; both keep ddcoop-heroes.ini up to date for the runtime. */
#ifndef LR_CORE_H
#define LR_CORE_H

#include "lr_files.h"
#include "lr_owners.h"
#include "lr_relay.h"

#define LR_KEEPALIVE_MS 1000
#define LR_PEER_TIMEOUT_MS 20000
#define LR_DEFAULT_GAME_PORT 27360
#define LR_HELLO_REPEAT_MS 2000 /* guest repeats HELLO until the host answers */
#define LR_MAX_CLAIMS 16        /* guest: ticks queued while Steam's send buffer is full */

typedef enum {
    LR_ST_IDLE = 0,
    LR_ST_HOST_WAITING,   /* host: game started, waiting for a friend */
    LR_ST_HOST_SENDING,   /* host: sending the campaign to the friend */
    LR_ST_HOST_PLAYING,   /* host: friend has the campaign; relay running */
    LR_ST_GUEST_HELLO,    /* guest: waiting for the host's answer */
    LR_ST_GUEST_RECEIVING,
    LR_ST_GUEST_LAUNCH,   /* guest: campaign stored, ini written, start the game now */
    LR_ST_GUEST_PLAYING,
    LR_ST_FAILED,
} lr_state;

typedef struct {
    lr_send_fn send;
    void *ctx;
    int (*allowed)(void *ctx, uint64_t peer);   /* host: is this peer in our lobby? NULL = everyone */
    int (*game_running)(void *ctx);             /* guest: is Darkest.exe open? NULL = never */
    int (*has_dlc)(void *ctx, const char *name); /* guest: is this DLC installed ("musketeer")? NULL = not checked */
    void (*random)(void *ctx, uint8_t *buf, size_t len);
    void (*log)(void *ctx, const char *line);   /* NULL = no log */
    uint32_t build;                              /* first 4 bytes of the Darkest.exe SHA-256 */
    uint32_t kit;                                /* first 4 bytes of the runtime's SHA-256: both sides must match */
    int coop_slot;                               /* campaign menu position, 1..9: the host's choice, for a guest what the host said */
    char ini_path[LR_PATH_MAX];                  /* mods\DDCoop\ddcoop.ini */
    char remote_dir[LR_PATH_MAX];                /* Steam\userdata\<id>\262060\remote */
    char backup_root[LR_PATH_MAX];               /* %USERPROFILE%\DDCoop-backups */
    char heroes_path[LR_PATH_MAX];               /* mods\DDCoop\ddcoop-heroes.ini; empty = not written */
    char restore_path[LR_PATH_MAX];              /* durable original guest snapshot; empty = legacy/test caller */
    uint64_t steam_user;                         /* owner of the local Steam save, checked by preparation helper */
} lr_config;

typedef struct {
    char name[LR_NAME_MAX + 1];
    uint32_t size, crc;
    uint8_t *data;
    uint32_t received;
} lr_file;

typedef struct {
    lr_config cfg;
    lr_state state;
    lr_role role;
    uint64_t peer;            /* host: the friend; guest: the host */
    char password[LR_PASSWORD_LEN + 1];
    char save_folder[LR_NAME_MAX + 1];
    int game_port;
    lr_relay relay;
    int relay_up;
    uint64_t last_rx, last_tx;
    /* save transfer (both directions use the same table) */
    lr_file files[LR_MAX_FILES];
    uint32_t file_count, total, sent_file, sent_offset, sent_bytes, recv_bytes;
    int send_phase;           /* host: HELLO, SESSION, begin, file headers, chunks, end, done */
    uint64_t hello_at;        /* guest: when HELLO was last sent */
    uint8_t resend[LR_MAX_MSG];
    size_t resend_len;
    uint64_t resend_peer;
    char status[256];         /* Dutch, one line for the window */
    char error[256];          /* Dutch; empty when fine */
    char backup_path[LR_PATH_MAX];
    /* hero ownership */
    lr_owners owners;         /* host: the authority; guest: the host's last table */
    uint32_t owners_sent;     /* host: revision the guest has (0 = send it) */
    struct {
        uint32_t id;
        uint8_t claim;
    } claims[LR_MAX_CLAIMS];  /* guest: ticks waiting for room in Steam's send buffer */
    int claim_count;
    int host_spent;           /* host: this game's runtime had its one session and takes no other */
} lr_core;

void lr_core_init(lr_core *c, const lr_config *cfg);
void lr_core_free(lr_core *c);

/* Host: checks the campaign exists and is past the tutorial, picks a port, writes ddcoop.ini.
 * 1 = the window may start the game. */
int lr_core_host_start(lr_core *c, uint64_t now_ms);
/* Guest: starts our local relay port and says HELLO to the host. */
int lr_core_guest_join(lr_core *c, uint64_t host_peer, uint64_t now_ms);

void lr_core_on_message(lr_core *c, uint64_t peer, int channel, const uint8_t *buf, size_t len, uint64_t now_ms);
void lr_core_peer_left(lr_core *c, uint64_t peer); /* Steam says the peer left the lobby */
void lr_core_tick(lr_core *c, uint64_t now_ms);

/* Host: a newer copy of the campaign's persist.roster.json (the window re-reads it while the game runs).
 * Heroes keep their owners; new heroes are the host's; the guest gets the new table. */
void lr_core_host_roster(lr_core *c, const uint8_t *buf, size_t len);

/* This player ticks (1 = mine) or unticks (0) a hero. Host: applied at once. Guest: asks the host, and the
 * answer arrives as a new table. Returns 1 if applied or sent. */
int lr_core_claim(lr_core *c, uint32_t hero_id, int claim);

/* Host: the running game's session ended; its runtime takes no new guest until the game restarts. */
void lr_core_host_spent(lr_core *c);

/* Player index in the ownership table: LR_PLAYER_HOST or LR_PLAYER_GUEST. */
int lr_core_player(const lr_core *c);

/* Guest: returns 1 once when the game should be started now (state moves to PLAYING). */
int lr_core_take_launch(lr_core *c);

/* Ends the session: BYE to the peer, ddcoop.ini back to role=off so a normal game start stays single player. */
void lr_core_stop(lr_core *c);

/* Writes ddcoop.ini. role "off" / "host" / "guest". */
int lr_write_ini(const char *path, const char *role, int port, const char *password, int coop_slot);

/* Finds the campaign folder for a menu position under remote_dir ("profile_2" or "steam_profile_2").
 * Returns 1 and the folder name, 0 if none exists. */
int lr_find_save_folder(const char *remote_dir, int coop_slot, char *name, size_t cap);

/* Reads <remote_dir>\<folder>\persist.game.json. 1 = read and parsed. */
int lr_read_game_info(const char *remote_dir, const char *folder, lr_game_info *out);

/* All nine menu slots, in order. Returns how many hold a campaign that can be read. */
typedef struct {
    int slot;       /* Menu position, 1..9. */
    int empty;      /* No folder, or no persist.game.json in it. */
    int unreadable; /* A persist.game.json that cannot be read: not called empty, it may be someone's save. */
    int tutorial;   /* Cannot be hosted yet (LR_TUTORIAL_TEXT). */
    char estate[64];
} lr_campaign_entry;
int lr_list_campaigns(const char *remote_dir, lr_campaign_entry out[9]);

/* Dutch, for the window: why a campaign in this state cannot be hosted. */
#define LR_TUTORIAL_TEXT                                                                                     \
    "Slot %d is still at the start of the tutorial (never saved); your friend would receive an empty "     \
    "campaign. Start through Steam, complete the tutorial to reach the Hamlet, close the game and click "    \
    "Host again."

const char *lr_state_name(lr_state s);

#endif
