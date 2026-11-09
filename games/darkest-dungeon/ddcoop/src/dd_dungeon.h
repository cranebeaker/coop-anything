/* Portable dungeon coordinator/proposed wire data. NOT wired to live runtime or player kit yet.
 * Native validation/execution, canonical hashes, RNG restoration and transport are adapter duties.
 * Caller supplies peer identity from its authenticated connection, never from an untrusted packet. */
#ifndef DD_DUNGEON_H
#define DD_DUNGEON_H
#include <stddef.h>
#include <stdint.h>
#define DD_DNG_PLAYERS 4 /* Core capacity; current live launcher/transport still supports only two. */
#define DD_DNG_HEROES 4
#define DD_DNG_TARGETS 8
#define DD_DNG_PROVISIONS 32
#define DD_DNG_HASH 32
#define DD_DNG_TIMEOUT_MS 30000

typedef enum {
    DD_DNG_TOWN,
    DD_DNG_EXPLORE,
    DD_DNG_BATTLE,
    DD_DNG_CAMP,
    DD_DNG_LOOT,
    DD_DNG_RETURNING
} dd_dng_scene;
typedef enum {
    DD_DNG_IDLE,
    DD_DNG_PREPARING,
    DD_DNG_LOADING,
    DD_DNG_ACTIVE,
    DD_DNG_NATIVE_PENDING,
    DD_DNG_WAIT_ACK,
    DD_DNG_RESYNC
} dd_dng_control;
typedef enum { DD_DNG_NONE, DD_DNG_HERO, DD_DNG_MONSTER } dd_dng_entity_kind;
typedef struct {
    uint8_t kind;
    uint32_t id;
} dd_dng_entity;
typedef struct {
    /* Native identity is the pair(type,id); id=0 is valid (food). Count must be positive.
     * Native adapter validates existence; portable/wire validation only checks shape. */
    uint32_t type, id, count;
} dd_dng_item;
typedef struct {
    uint32_t quest, dungeon, week, owner_revision;
    uint8_t players, connected;
    uint32_t heroes[DD_DNG_HEROES];
    uint8_t owners[DD_DNG_HEROES];
    uint32_t map_seed, rules_seed, battle_seed, quest_seed;
    uint8_t content_hash[DD_DNG_HASH], town_hash[DD_DNG_HASH];
    uint8_t item_count;
    dd_dng_item items[DD_DNG_PROVISIONS];
} dd_dng_departure;
typedef struct {
    uint8_t scene;
    uint32_t location, encounter, turn;
    dd_dng_entity active;
    uint32_t rules_seed, battle_seed, quest_seed;
    uint8_t hash[DD_DNG_HASH];
} dd_dng_checkpoint;
typedef enum {
    DD_DNG_SKILL = 1,
    DD_DNG_MONSTER_SKILL,
    DD_DNG_PASS,
    DD_DNG_USE_ITEM,
    DD_DNG_CAMP_SKILL,
    DD_DNG_CURIO,
    DD_DNG_TRAP,
    DD_DNG_MOVE,
    DD_DNG_ENTER_ROOM,
    DD_DNG_TAKE_LOOT,
    DD_DNG_START_CAMP,
    DD_DNG_RETREAT,
    DD_DNG_RETURN_TOWN,
    DD_DNG_END_CAMP,
    DD_DNG_FINISH_QUEST
} dd_dng_action;
typedef struct {
    uint32_t request, argument;
    uint8_t kind, target_count;
    dd_dng_entity actor, targets[DD_DNG_TARGETS];
    uint8_t before_hash[DD_DNG_HASH];
} dd_dng_command;
typedef struct {
    uint8_t origin;
    dd_dng_command command;
    dd_dng_checkpoint after;
} dd_dng_event;
typedef enum {
    DD_DNG_OK = 0,
    DD_DNG_INVALID,
    DD_DNG_BUSY,
    DD_DNG_STALE,
    DD_DNG_OWNER,
    DD_DNG_PHASE,
    DD_DNG_NATIVE_REFUSED,
    DD_DNG_DRIFT,
    DD_DNG_DUPLICATE
} dd_dng_result;
/* Read-only native rules check; a missing validator refuses commands. Execution is a separate step. */
typedef int (*dd_dng_validate_fn)(void *, const dd_dng_command *);
typedef struct {
    uint64_t expedition, previous_expedition, deadline;
    uint32_t sequence, last_request[DD_DNG_PLAYERS];
    uint8_t local, control, connected, ready, acknowledged, initial;
    dd_dng_departure departure;
    dd_dng_checkpoint checkpoint;
    dd_dng_event pending;
    dd_dng_validate_fn validate;
    void *context;
} dd_dungeon;

int dd_dng_departure_valid(const dd_dng_departure *);
int dd_dng_checkpoint_valid(const dd_dng_checkpoint *);
int dd_dng_command_valid(const dd_dng_command *);
int dd_dng_checkpoint_equal(const dd_dng_checkpoint *, const dd_dng_checkpoint *);
/* Host v1 ownership contract: missing/non-host file => all host, malformed host file => -1.
 * Returns1 for trusted host table,0 for defaults/ignored. No file IO; bounded input only. */
int dd_dng_owners_parse(const char *, size_t, const uint32_t party[4], uint8_t owners[4], uint32_t *revision);

void dd_dungeon_init(dd_dungeon *, unsigned local, dd_dng_validate_fn, void *);
dd_dng_result dd_dungeon_offer(dd_dungeon *, unsigned sender, uint64_t, const dd_dng_departure *,
                               uint64_t now);
/* Each copy verifies its OWN loaded town/content before it emits READY or accepts LAUNCH. */
dd_dng_result dd_dungeon_local_ready(dd_dungeon *, uint64_t, const uint8_t town[32],
                                     const uint8_t content[32]);
dd_dng_result dd_dungeon_ready(dd_dungeon *, unsigned peer, uint64_t, const uint8_t town[32],
                               const uint8_t content[32]);
dd_dng_result dd_dungeon_launch(dd_dungeon *, unsigned sender, uint64_t, uint64_t now);
dd_dng_result dd_dungeon_host_checkpoint(dd_dungeon *, const dd_dng_checkpoint *, uint64_t now);
dd_dng_result dd_dungeon_guest_checkpoint(dd_dungeon *, unsigned sender, uint64_t, const dd_dng_checkpoint *,
                                          uint64_t now);
dd_dng_result dd_dungeon_request(dd_dungeon *, unsigned peer, uint64_t, uint32_t quoted_sequence,
                                 const dd_dng_command *, uint64_t now);
/* Call ONCE after native completion, on the game thread; not when an asynchronous event is merely queued. */
dd_dng_result dd_dungeon_host_complete(dd_dungeon *, const dd_dng_checkpoint *, uint64_t now, dd_dng_event *);
/* Only a final refusal with NO queued/partial side effects is safe to continue. Uncertainty => resync. */
dd_dng_result dd_dungeon_host_refused(dd_dungeon *, int final_without_side_effects);
dd_dng_result dd_dungeon_guest_event(dd_dungeon *, unsigned sender, uint64_t, uint32_t sequence,
                                     const dd_dng_event *, uint64_t now);
dd_dng_result dd_dungeon_guest_complete(dd_dungeon *, const dd_dng_checkpoint *actual, uint64_t now);
dd_dng_result dd_dungeon_ack(dd_dungeon *, unsigned peer, uint64_t, uint32_t sequence,
                             const uint8_t hash[32]);
dd_dng_result dd_dungeon_release(dd_dungeon *, unsigned sender, uint64_t, uint32_t sequence,
                                 const uint8_t hash[32]);
dd_dng_result dd_dungeon_members(dd_dungeon *, unsigned sender, uint64_t, uint8_t connected);
dd_dng_result dd_dungeon_finish_return(dd_dungeon *);
void dd_dungeon_connection_lost(dd_dungeon *);
void dd_dungeon_tick(dd_dungeon *, uint64_t now);
int dd_dungeon_effective_owner(const dd_dungeon *, uint32_t hero);
int dd_dungeon_town_allowed(const dd_dungeon *);
#endif
