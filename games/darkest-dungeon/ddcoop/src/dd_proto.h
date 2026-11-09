/* Wire format for co-op messages. Platform independent and unit tested (tests/test_proto.c).
 *
 * Every message: 16-byte header + body, little endian.
 *   u32 magic 'DDCP', u16 protocol version, u16 type, u32 sequence, u32 body length
 * The decoder checks everything it reads (magic, version, type, exact body length, value ranges) and
 * rejects anything else, so a damaged or hostile packet can never be applied to the game.
 *
 * Town actions follow the host-authority flow from NOTES.md: a guest sends DD_MSG_TOWN_REQUEST, the
 * host runs the game's own function and answers DD_MSG_TOWN_APPLY (to everyone) or DD_MSG_TOWN_REJECT. */
#ifndef DD_PROTO_H
#define DD_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include "dd_trinket.h"

#define DD_PROTO_MAGIC 0x50434444u /* "DDCP" */
#define DD_PROTO_VERSION 25 /* guest requests normal expedition departure */
#define DD_PROTO_HEADER 16
#define DD_PROTO_MAX_BODY 256

typedef enum {
    DD_MSG_HELLO = 1,        /* both directions, first message */
    DD_MSG_TOWN_REQUEST = 2, /* guest -> host */
    DD_MSG_TOWN_APPLY = 3,   /* host -> everyone: run this action */
    DD_MSG_TOWN_REJECT = 4,  /* host -> requesting guest */
    DD_MSG_PING = 5,
    DD_MSG_PONG = 6,
    DD_MSG_WALK = 7,         /* host -> guest: newest party position; no order, no answer */
    DD_MSG_WALK_INTENT = 8,  /* guest -> host: the direction its keys ask for; no order, no answer */
    DD_MSG_LOAD = 9,         /* both directions: this copy waits at the save screen to load menu slot u.load.slot */
} dd_msg_type;

typedef enum {
    DD_ACT_ASSIGN_HERO = 1,       /* stress building / sanitarium slot; commits on lock-in */
    DD_ACT_BUY_UPGRADE = 2,       /* blacksmith, guild, buildings */
    DD_ACT_RECRUIT = 3,           /* stagecoach */
    DD_ACT_DISMISS = 4,
    DD_ACT_PROVISION_BUY = 5,
    DD_ACT_PROVISION_RETURN = 6,
    DD_ACT_WAGON_BUY = 7,         /* Nomad Wagon */
    DD_ACT_WAGON_SELL = 8,
    DD_ACT_HEIRLOOM_EXCHANGE = 9,
    DD_ACT_RENAME = 10,
    DD_ACT_SKILL = 11,
    DD_ACT_TRINKET_MOVE = 12,
    DD_ACT_PARTY = 13,
    DD_ACT_SUPPLIES = 14,
    DD_ACT_DEPARTURE = 15,
    DD_ACT_TRAVEL = 16,
    DD_ACT_USE = 17,
    DD_ACT_BATTLE = 18,
    DD_ACT_ITEM = 19,
    DD_ACT_OWNERS = 20, /* host -> guest: live party ownership, ordered with choices */
    DD_ACT_PREPARE = 21, /* open normal provisions together, same selected mission */
    DD_ACT_EMBARK = 22, /* request normal departure; only the host supplies seeds via DEPARTURE */
    DD_ACT_COUNT_
} dd_action_kind;

/* Building activities, in the order of the class table (Building::Activity::*). */
typedef enum {
    DD_ACTIVITY_BAR = 0, DD_ACTIVITY_GAMBLING, DD_ACTIVITY_BROTHEL, DD_ACTIVITY_MEDITATION, DD_ACTIVITY_PRAYER,
    DD_ACTIVITY_FLAGELLATION, DD_ACTIVITY_QUIRK_TREATMENT, DD_ACTIVITY_DISEASE_TREATMENT, DD_ACTIVITY_COUNT_
} dd_activity;

typedef struct {
    uint8_t kind; /* dd_action_kind */
    union {
        struct { uint8_t activity, slot; uint32_t hero_id; } assign;              /* activity: dd_activity */
        struct { uint32_t tree; uint8_t code; uint32_t hero_id; } upgrade;        /* tree = hash("class.item"), hero 0 = building */
        struct { uint32_t hero_id; } hero;                                         /* recruit, dismiss */
        struct { uint16_t slot, count; uint32_t type, id, cost; } items;             /* wagon buy/sell: physical slot, item identity, quoted gold; provision actions use slot/count */
        struct { int32_t src_type, src_amount, dst_type, dst_amount; } exchange;  /* heirloom exchange */
        struct {uint32_t hero_id;char before[64],after[64];} rename;
        struct {uint32_t hero_id;uint8_t index,camping,before,after;uint32_t selected_mask;} skill;
        struct {uint32_t src_hero,dst_hero;uint16_t src_slot,dst_slot;uint32_t src_type,src_id,dst_type,dst_id;} move;
        struct {uint32_t before[4],after[4];uint8_t show;} party; /* show: 0, or 1+rank when only that slot gets drawn */
        struct {uint8_t op;uint16_t slot;uint32_t type,id,gold,shards;uint8_t before[32];} supplies;
        struct {uint32_t seed,rules,quest,quest_index,heroes[4],supplies,revision;uint8_t owners;} departure; /* owners: bit i, heroes[i] belongs to the guest */
        struct {uint32_t dest[4],from_area,rules,torch,bag,ids[4],hp[4],stress[4],trinkets[4];uint8_t has_from,flip,extra;} travel; /* rules, torch, bag and the party by rank (id 0: none; hp and stress the bits of a float; trinkets a hash): the host's state as it leaves, rules 0 unknown */
        struct {uint32_t area,prop,prop_kind,pos,option,item_type,item_id;uint8_t hero,flag,door;} use; /* door: 0 use, 1 a trap, 4 a curio's item button, 2 a door, 3 the curio's button */
        struct {uint32_t round,turn,rules;int32_t value;uint8_t op,state,hero,flag,side,place;} battle; /* value: skill slot or -1, or a rank; side and place: the target */
        struct {uint32_t area,pos,round,turn,rules,amount,type,id;int32_t slot;uint8_t op,state,hero,inv,flag;} item; /* state 0: outside a fight; hero 255: none; inv 0 the bag, 1 the loot */
        struct {uint32_t heroes[4],revision;uint8_t mask;} owners;
        struct {uint32_t index,key;} prepare; /* PREPARE or EMBARK: shared mission identity */
    } u;
} dd_action;
enum { DD_SUPPLY_BUY=1, DD_SUPPLY_RETURN=2, DD_SUPPLY_CANCEL=3 };
enum { DD_BATTLE_OP_SKILL = 0, DD_BATTLE_OP_ENEMY = 1, DD_BATTLE_OP_FRIEND = 2, DD_BATTLE_OP_MOVE = 3 };
enum { DD_ITEM_OP_USE = 0, DD_ITEM_OP_TORCH = 1, DD_ITEM_OP_LOOT_ALL = 2, DD_ITEM_OP_LOOT_CLOSE = 3, DD_ITEM_OP_LOOT_ITEM = 4, DD_ITEM_OP_CURIO = 5, DD_ITEM_OP_SELECT = 6, DD_ITEM_OP_TUTORIAL = 7, DD_ITEM_OP_RETREAT = 8, DD_ITEM_OP_CAMP_MEAL = 9, DD_ITEM_OP_CAMP_SKILL = 10, DD_ITEM_OP_CAMP_LEAVE = 11, DD_ITEM_OP_QUEST_ONE = 12, DD_ITEM_OP_QUEST_TWO = 13, DD_ITEM_OP_RULES = 14, DD_ITEM_OP_RESULTS_NEXT = 15, DD_ITEM_OP_BAG_SLOT = 16, DD_ITEM_OP_BAG_ASK = 17, DD_ITEM_OP_TOWN = 18, DD_ITEM_OP_HUNGER = 19, DD_ITEM_OP_QUEST_RETURN = 20 }; /* TOWN: host -> guest after a raid; type currencies, id heroes, round count (hashes) */

typedef struct {
    uint32_t protocol;
    char mod_version[16];
    uint8_t build[8];   /* first 8 bytes of the exe's sha256: both sides must run the same game build */
    char name[32];      /* display name */
} dd_hello;

typedef enum {
    DD_REJECT_NOT_ALLOWED = 1, /* the game refused it (not enough gold, slot taken, ...) */
    DD_REJECT_BUSY = 2,        /* host cannot apply actions right now (loading, not in town) */
    DD_REJECT_INVALID = 3,     /* values out of range */
} dd_reject_reason;

typedef struct {
    uint16_t type;
    uint32_t seq;
    union {
        dd_hello hello;
        struct { uint32_t request_id; dd_action action; } town;   /* REQUEST and APPLY */
        struct { uint32_t request_id; uint8_t reason; } reject;
        struct { uint64_t time_ms; } ping;                        /* PING and PONG */
        struct { uint32_t area, pos; } walk;                      /* pos: the bits of a float */
        struct { int32_t direction; } intent;                     /* -1, 0 or +1 */
        struct { uint32_t slot; } load;                           /* 1..9 */
    } u;
} dd_msg;

typedef enum {
    DD_DECODE_OK = 0,
    DD_DECODE_SHORT,        /* not enough bytes yet (stream: wait for more) */
    DD_DECODE_BAD_MAGIC,
    DD_DECODE_BAD_VERSION,
    DD_DECODE_BAD_TYPE,
    DD_DECODE_BAD_LENGTH,
    DD_DECODE_BAD_VALUE,
} dd_decode_result;

/* Encodes msg into buf. Returns the number of bytes written, or 0 if buf is too small or msg invalid. */
size_t dd_encode(const dd_msg *msg, uint8_t *buf, size_t cap);

/* Decodes one message from the start of buf. On OK, *used is the message's size. */
dd_decode_result dd_decode(const uint8_t *buf, size_t len, dd_msg *msg, size_t *used);

/* 1 if the action's values are in range (kind known, slot/activity bounds, counts > 0, amounts > 0). */
int dd_action_valid(const dd_action *a);

const char *dd_decode_result_name(dd_decode_result r);

#endif
