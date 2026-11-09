#include "dd_dungeon.h"
#include <string.h>

static int nonzero(const uint8_t hash[32]) {
    uint8_t v = 0;
    for (unsigned i = 0; i < 32; i++)
        v |= hash[i];
    return v != 0;
}
static int seed(uint32_t v) { return v > 0 && v < 0x7fffffff; }
static int entity(dd_dng_entity e, int empty) {
    return (empty && !e.kind && !e.id) || (e.kind >= DD_DNG_HERO && e.kind <= DD_DNG_MONSTER && e.id);
}
static int same_entity(dd_dng_entity a, dd_dng_entity b) { return a.kind == b.kind && a.id == b.id; }
static int mask(unsigned players, unsigned m) {
    return players && players <= 4 && (m & 1) && !(m & ~((1u << players) - 1));
}
static dd_dng_result drift(dd_dungeon *s) {
    s->control = DD_DNG_RESYNC;
    return DD_DNG_DRIFT;
}
static int peer(const dd_dungeon *s, unsigned p) {
    return p < s->departure.players && (s->connected & (1u << p));
}
static int epoch(const dd_dungeon *s, uint64_t e) { return e && e == s->expedition; }
static void deadline(dd_dungeon *s, uint64_t now) {
    s->deadline = now > UINT64_MAX - DD_DNG_TIMEOUT_MS ? UINT64_MAX : now + DD_DNG_TIMEOUT_MS;
}
int dd_dng_departure_valid(const dd_dng_departure *d) {
    if (!d || !d->quest || !d->dungeon || !mask(d->players, d->connected) || !seed(d->rules_seed) ||
        !seed(d->battle_seed) || !seed(d->quest_seed) || !nonzero(d->content_hash) ||
        !nonzero(d->town_hash) || d->item_count > DD_DNG_PROVISIONS)
        return 0;
    unsigned count = 0;
    for (unsigned i = 0; i < 4; i++) {
        if (d->owners[i] >= d->players || (!d->heroes[i] && d->owners[i])) return 0;
        if (d->heroes[i]) {
            count++;
            for (unsigned j = 0; j < i; j++)
                if (d->heroes[j] == d->heroes[i]) return 0;
        }
    }
    for (unsigned i = 0; i < d->item_count; i++)
        /* Food uses (type=provision,id=empty-string). Native adapter still verifies the pair
         * against actual item definitions; the portable shape validator must preserve zero ID. */
        if (!d->items[i].type || !d->items[i].count || d->items[i].count > INT32_MAX) return 0;
    return count != 0;
}
int dd_dng_checkpoint_valid(const dd_dng_checkpoint *c) {
    return c && c->scene <= DD_DNG_RETURNING && entity(c->active, 1) &&
           (c->scene == DD_DNG_BATTLE ? c->encounter && c->active.kind : !c->active.kind) &&
           seed(c->rules_seed) && seed(c->battle_seed) && seed(c->quest_seed) && nonzero(c->hash);
}
int dd_dng_checkpoint_equal(const dd_dng_checkpoint *a, const dd_dng_checkpoint *b) {
    return a->scene == b->scene && a->location == b->location && a->encounter == b->encounter &&
           a->turn == b->turn && same_entity(a->active, b->active) && a->rules_seed == b->rules_seed &&
           a->battle_seed == b->battle_seed && a->quest_seed == b->quest_seed &&
           !memcmp(a->hash, b->hash, 32);
}
int dd_dng_command_valid(const dd_dng_command *c) {
    if (!c || !c->request || c->kind < DD_DNG_SKILL || c->kind > DD_DNG_FINISH_QUEST ||
        c->target_count > DD_DNG_TARGETS || !entity(c->actor, 1) || !nonzero(c->before_hash))
        return 0;
    if (c->kind >= DD_DNG_MOVE) {
        if (c->actor.kind || c->target_count) return 0;
    } else if (c->kind == DD_DNG_MONSTER_SKILL) {
        if (c->actor.kind != DD_DNG_MONSTER || !c->argument) return 0;
    } else if (c->actor.kind != DD_DNG_HERO || (c->kind != DD_DNG_PASS && !c->argument))
        return 0;
    for (unsigned i = 0; i < c->target_count; i++) {
        if (!entity(c->targets[i], 0)) return 0;
        for (unsigned j = 0; j < i; j++)
            if (same_entity(c->targets[i], c->targets[j])) return 0;
    }
    return 1;
}
void dd_dungeon_init(dd_dungeon *s, unsigned local, dd_dng_validate_fn validate, void *context) {
    memset(s, 0, sizeof *s);
    s->local = (uint8_t)(local < DD_DNG_PLAYERS ? local : DD_DNG_PLAYERS);
    s->validate = validate;
    s->context = context;
    s->control = local < DD_DNG_PLAYERS ? DD_DNG_IDLE : DD_DNG_RESYNC;
}
int dd_dungeon_effective_owner(const dd_dungeon *s, uint32_t id) {
    for (unsigned i = 0; i < 4; i++)
        if (id && s->departure.heroes[i] == id) {
            unsigned p = s->departure.owners[i];
            return peer(s, p) ? (int)p : 0;
        }
    return -1;
}
static dd_dng_result allowed(const dd_dungeon *s, unsigned from, const dd_dng_command *c) {
    if (!peer(s, from) || !dd_dng_command_valid(c)) return DD_DNG_INVALID;
    if (c->actor.kind == DD_DNG_HERO && dd_dungeon_effective_owner(s, c->actor.id) != (int)from)
        return DD_DNG_OWNER;
    if (c->actor.kind != DD_DNG_HERO && from) return DD_DNG_OWNER;
    for (unsigned i = 0; i < c->target_count; i++)
        if (c->targets[i].kind == DD_DNG_HERO && dd_dungeon_effective_owner(s, c->targets[i].id) < 0)
            return DD_DNG_INVALID;
    unsigned scene = s->checkpoint.scene;
    switch (c->kind) {
    case DD_DNG_SKILL:
    case DD_DNG_MONSTER_SKILL:
    case DD_DNG_PASS:
        if (scene != DD_DNG_BATTLE || !same_entity(c->actor, s->checkpoint.active)) return DD_DNG_PHASE;
        break;
    case DD_DNG_USE_ITEM:
        if (scene != DD_DNG_EXPLORE && scene != DD_DNG_CAMP &&
            !(scene == DD_DNG_BATTLE && same_entity(c->actor, s->checkpoint.active)))
            return DD_DNG_PHASE;
        break;
    case DD_DNG_CAMP_SKILL:
        if (scene != DD_DNG_CAMP) return DD_DNG_PHASE;
        break;
    case DD_DNG_CURIO:
    case DD_DNG_TRAP:
    case DD_DNG_MOVE:
    case DD_DNG_ENTER_ROOM:
    case DD_DNG_START_CAMP:
        if (scene != DD_DNG_EXPLORE) return DD_DNG_PHASE;
        break;
    case DD_DNG_TAKE_LOOT:
        if (scene != DD_DNG_LOOT) return DD_DNG_PHASE;
        break;
    case DD_DNG_RETREAT:
        if (scene != DD_DNG_BATTLE && scene != DD_DNG_EXPLORE) return DD_DNG_PHASE;
        break;
    case DD_DNG_RETURN_TOWN:
        if (scene != DD_DNG_RETURNING) return DD_DNG_PHASE;
        break;
    case DD_DNG_END_CAMP:
        if (scene != DD_DNG_CAMP) return DD_DNG_PHASE;
        break;
    case DD_DNG_FINISH_QUEST:
        if (scene != DD_DNG_EXPLORE && scene != DD_DNG_LOOT) return DD_DNG_PHASE;
        break;
    default:
        return DD_DNG_INVALID;
    }
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_offer(dd_dungeon *s, unsigned sender, uint64_t run, const dd_dng_departure *d,
                               uint64_t now) {
    if (sender || !run || run == s->previous_expedition || !dd_dng_departure_valid(d) ||
        s->local >= d->players || !(d->connected & (1u << s->local)))
        return DD_DNG_INVALID;
    if (s->control != DD_DNG_IDLE) return DD_DNG_BUSY;
    s->expedition = run;
    s->departure = *d;
    s->connected = d->connected;
    s->ready = 0;
    s->sequence = 0;
    memset(s->last_request, 0, sizeof s->last_request);
    s->acknowledged = 0;
    s->initial = 0;
    memset(&s->pending, 0, sizeof s->pending);
    s->control = DD_DNG_PREPARING;
    deadline(s, now);
    return DD_DNG_OK;
}
static dd_dng_result ready_check(dd_dungeon *s, unsigned p, uint64_t run, const uint8_t town[32],
                                 const uint8_t content[32]) {
    if (!epoch(s, run) || !peer(s, p)) return DD_DNG_INVALID;
    if (s->control != DD_DNG_PREPARING) return DD_DNG_BUSY;
    if (memcmp(town, s->departure.town_hash, 32) || memcmp(content, s->departure.content_hash, 32))
        return drift(s);
    s->ready |= (uint8_t)(1u << p);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_local_ready(dd_dungeon *s, uint64_t run, const uint8_t town[32],
                                     const uint8_t content[32]) {
    return ready_check(s, s->local, run, town, content);
}
dd_dng_result dd_dungeon_ready(dd_dungeon *s, unsigned p, uint64_t run, const uint8_t town[32],
                               const uint8_t content[32]) {
    if (s->local || !p) return DD_DNG_INVALID;
    return ready_check(s, p, run, town, content);
}
dd_dng_result dd_dungeon_launch(dd_dungeon *s, unsigned sender, uint64_t run, uint64_t now) {
    if (sender || !epoch(s, run)) return DD_DNG_INVALID;
    if (s->control != DD_DNG_PREPARING || !(s->ready & (1u << s->local)) ||
        (!s->local && s->ready != s->connected))
        return DD_DNG_BUSY;
    s->control = DD_DNG_LOADING;
    deadline(s, now);
    return DD_DNG_OK;
}
static int checkpoint_party(const dd_dungeon *s, const dd_dng_checkpoint *c) {
    return dd_dng_checkpoint_valid(c) &&
           (c->active.kind != DD_DNG_HERO || dd_dungeon_effective_owner(s, c->active.id) >= 0);
}
dd_dng_result dd_dungeon_host_checkpoint(dd_dungeon *s, const dd_dng_checkpoint *c, uint64_t now) {
    if (s->local || !checkpoint_party(s, c) || c->scene == DD_DNG_TOWN || c->scene == DD_DNG_RETURNING)
        return DD_DNG_INVALID;
    if (s->control != DD_DNG_LOADING) return DD_DNG_BUSY;
    s->checkpoint = *c;
    s->sequence = 0;
    s->acknowledged = 1;
    s->control = DD_DNG_WAIT_ACK;
    deadline(s, now);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_guest_checkpoint(dd_dungeon *s, unsigned sender, uint64_t run,
                                          const dd_dng_checkpoint *c, uint64_t now) {
    if (!s->local || sender || !epoch(s, run) || !checkpoint_party(s, c) || c->scene == DD_DNG_TOWN ||
        c->scene == DD_DNG_RETURNING)
        return DD_DNG_INVALID;
    if (s->control != DD_DNG_LOADING) return DD_DNG_BUSY;
    s->pending.after = *c;
    s->initial = 1;
    s->control = DD_DNG_NATIVE_PENDING;
    deadline(s, now);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_request(dd_dungeon *s, unsigned from, uint64_t run, uint32_t seq,
                                 const dd_dng_command *c, uint64_t now) {
    if (s->local || !epoch(s, run) || !peer(s, from) || !dd_dng_command_valid(c)) return DD_DNG_INVALID;
    if (s->control != DD_DNG_ACTIVE) return DD_DNG_BUSY;
    if (seq != s->sequence || memcmp(c->before_hash, s->checkpoint.hash, 32)) return DD_DNG_STALE;
    if (c->request <= s->last_request[from]) return DD_DNG_DUPLICATE;
    s->last_request[from] = c->request;
    dd_dng_result r = allowed(s, from, c);
    if (r != DD_DNG_OK) return r;
    if (!s->validate || !s->validate(s->context, c)) return DD_DNG_NATIVE_REFUSED;
    memset(&s->pending, 0, sizeof s->pending);
    s->pending.origin = (uint8_t)from;
    s->pending.command = *c;
    s->control = DD_DNG_NATIVE_PENDING;
    deadline(s, now);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_host_complete(dd_dungeon *s, const dd_dng_checkpoint *after, uint64_t now,
                                       dd_dng_event *out) {
    if (s->local || !out) return DD_DNG_INVALID;
    if (s->control != DD_DNG_NATIVE_PENDING) return DD_DNG_BUSY;
    if (s->sequence == UINT32_MAX || !checkpoint_party(s, after) ||
        (after->scene == DD_DNG_TOWN && s->checkpoint.scene != DD_DNG_RETURNING))
        return drift(s);
    s->pending.after = *after;
    *out = s->pending;
    s->checkpoint = *after;
    s->sequence++;
    s->control = DD_DNG_WAIT_ACK;
    s->acknowledged = 1;
    deadline(s, now);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_host_refused(dd_dungeon *s, int final_without_side_effects) {
    if (s->local || s->control != DD_DNG_NATIVE_PENDING) return DD_DNG_INVALID;
    if (!final_without_side_effects) return drift(s);
    s->control = DD_DNG_ACTIVE;
    memset(&s->pending, 0, sizeof s->pending);
    return DD_DNG_NATIVE_REFUSED;
}
dd_dng_result dd_dungeon_guest_event(dd_dungeon *s, unsigned sender, uint64_t run, uint32_t seq,
                                     const dd_dng_event *e, uint64_t now) {
    if (!s->local || sender || !epoch(s, run) || !e || !checkpoint_party(s, &e->after)) return DD_DNG_INVALID;
    if (s->control == DD_DNG_RESYNC) return DD_DNG_DRIFT;
    if (seq == s->sequence && dd_dng_checkpoint_equal(&s->checkpoint, &e->after)) return DD_DNG_DUPLICATE;
    if (s->control != DD_DNG_ACTIVE) return DD_DNG_BUSY;
    if (seq != s->sequence + 1 || s->sequence == UINT32_MAX ||
        memcmp(e->command.before_hash, s->checkpoint.hash, 32) ||
        allowed(s, e->origin, &e->command) != DD_DNG_OK ||
        (e->after.scene == DD_DNG_TOWN && s->checkpoint.scene != DD_DNG_RETURNING))
        return drift(s);
    if (!s->validate || !s->validate(s->context, &e->command)) return drift(s);
    s->pending = *e;
    s->initial = 0;
    s->control = DD_DNG_NATIVE_PENDING;
    deadline(s, now);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_guest_complete(dd_dungeon *s, const dd_dng_checkpoint *actual, uint64_t now) {
    if (!s->local || s->control != DD_DNG_NATIVE_PENDING) return DD_DNG_INVALID;
    if (!actual || !dd_dng_checkpoint_equal(actual, &s->pending.after)) return drift(s);
    s->checkpoint = *actual;
    s->sequence += !s->initial;
    s->initial = 0;
    s->control = DD_DNG_WAIT_ACK;
    deadline(s, now);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_ack(dd_dungeon *s, unsigned p, uint64_t run, uint32_t seq, const uint8_t hash[32]) {
    if (s->local || !epoch(s, run) || !peer(s, p)) return DD_DNG_INVALID;
    if (s->control != DD_DNG_WAIT_ACK) return DD_DNG_BUSY;
    if (seq != s->sequence) return DD_DNG_STALE;
    if (memcmp(hash, s->checkpoint.hash, 32)) return drift(s);
    s->acknowledged |= (uint8_t)(1u << p);
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_release(dd_dungeon *s, unsigned sender, uint64_t run, uint32_t seq,
                                 const uint8_t hash[32]) {
    if (sender || !epoch(s, run)) return DD_DNG_INVALID;
    if (s->control != DD_DNG_WAIT_ACK || (!s->local && s->acknowledged != s->connected)) return DD_DNG_BUSY;
    if (seq != s->sequence || memcmp(hash, s->checkpoint.hash, 32)) return drift(s);
    s->control = DD_DNG_ACTIVE;
    s->deadline = 0;
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_members(dd_dungeon *s, unsigned sender, uint64_t run, uint8_t connected) {
    if (sender || !epoch(s, run) || !mask(s->departure.players, connected) ||
        !(connected & (1u << s->local)) || (connected & ~s->connected))
        return DD_DNG_INVALID;
    if (s->control == DD_DNG_NATIVE_PENDING)
        return drift(s); /* Finish or resync an in-flight native operation first. */
    if (s->control == DD_DNG_RESYNC) return DD_DNG_DRIFT;
    s->connected = connected;
    s->ready &= connected;
    s->acknowledged &= connected;
    return DD_DNG_OK;
}
dd_dng_result dd_dungeon_finish_return(dd_dungeon *s) {
    if (s->control != DD_DNG_ACTIVE || s->checkpoint.scene != DD_DNG_TOWN) return DD_DNG_BUSY;
    s->previous_expedition = s->expedition;
    s->expedition = 0;
    s->control = DD_DNG_IDLE;
    return DD_DNG_OK;
}
void dd_dungeon_connection_lost(dd_dungeon *s) { s->control = DD_DNG_RESYNC; }
void dd_dungeon_tick(dd_dungeon *s, uint64_t now) {
    if (s->control != DD_DNG_IDLE && s->control != DD_DNG_ACTIVE && s->control != DD_DNG_RESYNC &&
        now >= s->deadline)
        s->control = DD_DNG_RESYNC;
}
int dd_dungeon_town_allowed(const dd_dungeon *s) { return s->control == DD_DNG_IDLE; }
