/* Launcher wire format: the messages two DDCoop launchers send each other over Steam (platform independent;
 * launcher/tests/test_lr_wire.c).
 *
 * The launcher runs next to the game, never inside it. It carries the co-op runtime's own TCP connection
 * between the PCs (stream messages, channel 0) and sets the session up (control messages, channel 1):
 * the host hands the guest a fresh password and its co-op campaign, so nobody types an IP, a port or a
 * password. Every message starts with "DL", a version byte and a type byte; the decoder refuses anything
 * that is not exactly one well-formed message. */
#ifndef LR_WIRE_H
#define LR_WIRE_H

#include <stddef.h>
#include <stdint.h>

#define LR_VERSION 2 /* slot-3 preparation and automatic guest recovery require both launchers updated */
#define LR_HEADER 4                /* 'D' 'L' version type */
#define LR_CHUNK 16000             /* stream data and save chunks per message */
#define LR_MAX_MSG (LR_HEADER + 16 + LR_CHUNK)
#define LR_PASSWORD_LEN 32         /* letters and digits only, so it is safe in ddcoop.ini */
#define LR_NAME_MAX 64             /* save file name, without folder */
#define LR_MAX_FILES 128
#define LR_MAX_FILE_SIZE (16u * 1024 * 1024)
#define LR_MAX_SAVE_TOTAL (64u * 1024 * 1024)

#define LR_CH_STREAM 0
#define LR_CH_CONTROL 1

typedef enum {
    /* channel 0 */
    LR_STREAM_OPEN = 1,  /* id */
    LR_STREAM_DATA = 2,  /* id, bytes */
    LR_STREAM_CLOSE = 3, /* id */
    /* channel 1 */
    LR_HELLO = 10,      /* role, game build hash prefix, kit id (runtime hash prefix) in id */
    LR_KEEPALIVE = 11,
    LR_BYE = 12,        /* reason */
    LR_SESSION = 13,    /* host -> guest: password, co-op menu slot */
    LR_SAVE_BEGIN = 14, /* host -> guest: file count, total bytes, save folder name */
    LR_SAVE_FILE = 15,  /* index, size, crc32, name */
    LR_SAVE_CHUNK = 16, /* index, offset, bytes */
    LR_SAVE_END = 17,
    LR_SAVE_RESULT = 18, /* guest -> host: 0 = stored, else lr_bye_reason */
    LR_ROSTER = 19,      /* host -> guest: hero ownership table (revision in id, lr_owners body in data) */
    LR_CLAIM = 20,       /* guest -> host: hero id in id, a = 1 tick (mine) / 0 untick */
} lr_type;

typedef enum {
    LR_ROLE_HOST = 1,
    LR_ROLE_GUEST = 2,
} lr_role;

typedef enum {
    LR_BYE_QUIT = 1,         /* the player closed the session */
    LR_BYE_FULL = 2,         /* the host already has a guest (the runtime takes one) */
    LR_BYE_VERSION = 3,      /* different launcher version */
    LR_BYE_BUILD = 4,        /* different Darkest.exe build */
    LR_BYE_SAVE = 5,         /* the campaign could not be sent or stored */
    LR_BYE_GAME_RUNNING = 6, /* the guest's game was already open, so its save could not be replaced */
    LR_BYE_PROTOCOL = 7,     /* a malformed or unexpected message */
    LR_BYE_DLC = 8,          /* the guest lacks DLC the host's campaign uses */
    LR_BYE_RESTART = 9,      /* the host's game had its one session; it must restart first */
} lr_bye_reason;

typedef struct {
    lr_type type;
    uint32_t id;      /* stream id, file index */
    uint32_t a;       /* role / reason / slot / count / size / offset / result */
    uint32_t b;       /* build / total / crc32 */
    char text[LR_NAME_MAX + 1]; /* password or file name, NUL terminated */
    const uint8_t *data;        /* stream or chunk bytes (points into the decoded buffer) */
    size_t len;
} lr_msg;

/* Channel a message type belongs on. */
int lr_channel_of(lr_type t);

/* Encodes into out (LR_MAX_MSG bytes); returns the length, 0 if the message is not valid. */
size_t lr_encode(const lr_msg *m, uint8_t *out);

/* Decodes exactly one message; 1 = ok, 0 = refused. channel is where it arrived: a type on the wrong
 * channel is refused too. */
int lr_decode(const uint8_t *buf, size_t len, int channel, lr_msg *out);

/* Password characters and file names the launcher accepts (also used when reading the save folder). */
int lr_password_ok(const char *p);
int lr_file_name_ok(const char *name);

uint32_t lr_crc32(uint32_t crc, const uint8_t *p, size_t len); /* start with 0 */

const char *lr_bye_text(uint32_t reason); /* Dutch, for the window */

#endif
