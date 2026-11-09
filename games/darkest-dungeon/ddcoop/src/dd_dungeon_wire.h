/* Proposed dungeon-only protocol. Not routed through live DDCP v3; see runtime/DUNGEON.md.
 * Framing/authentication stays with dd_session. Never trust identity from message contents.
 * All integers little endian; no native structs/pointers/padding go on the wire. */
#ifndef DD_DUNGEON_WIRE_H
#define DD_DUNGEON_WIRE_H
#include "dd_dungeon.h"
#define DD_DNG_WIRE_MAGIC 0x31474e44u /* DNG1 */
#define DD_DNG_WIRE_VERSION 1
#define DD_DNG_WIRE_HEADER 24
#define DD_DNG_WIRE_MAX 1024
typedef enum {
    DD_DNG_OFFER = 1,
    DD_DNG_READY,
    DD_DNG_LAUNCH,
    DD_DNG_CHECKPOINT,
    DD_DNG_COMMAND,
    DD_DNG_EVENT,
    DD_DNG_ACK,
    DD_DNG_RELEASE,
    DD_DNG_REJECT,
    DD_DNG_MEMBERS
} dd_dng_message_kind;
typedef struct {
    uint16_t type;
    uint32_t sequence;
    uint64_t expedition;
    union {
        dd_dng_departure offer;
        struct {
            uint8_t town[32], content[32];
        } ready;
        dd_dng_checkpoint checkpoint;
        dd_dng_command command;
        dd_dng_event event;
        uint8_t hash[32];
        struct {
            uint32_t request;
            uint8_t reason;
        } reject;
        uint8_t connected;
    } u;
} dd_dng_message;
/* Exact complete packets only: callers use the authenticated outer length framing.
 * Encode returns length or0; decode returns1 or0 and zeroes output on any failure. */
size_t dd_dng_encode(const dd_dng_message *, uint8_t *out, size_t capacity);
int dd_dng_decode(const uint8_t *, size_t length, dd_dng_message *out);
#endif
