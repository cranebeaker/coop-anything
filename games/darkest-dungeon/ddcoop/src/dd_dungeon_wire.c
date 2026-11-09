#include "dd_dungeon_wire.h"
#include <string.h>

typedef struct {
    uint8_t *p;
    size_t at, cap;
    int ok;
} writer;
typedef struct {
    const uint8_t *p;
    size_t at, len;
    int ok;
} reader;
static void put(writer *w, uint64_t v, unsigned n) {
    if (!w->ok || n > w->cap - w->at) {
        w->ok = 0;
        return;
    }
    for (unsigned i = 0; i < n; i++) {
        w->p[w->at++] = (uint8_t)v;
        v >>= 8;
    }
}
static uint64_t get(reader *r, unsigned n) {
    if (!r->ok || n > r->len - r->at) {
        r->ok = 0;
        return 0;
    }
    uint64_t v = 0;
    for (unsigned i = 0; i < n; i++)
        v |= (uint64_t)r->p[r->at++] << (8 * i);
    return v;
}
static void bytes_out(writer *w, const uint8_t *p, size_t n) {
    if (!w->ok || n > w->cap - w->at) {
        w->ok = 0;
        return;
    }
    memcpy(w->p + w->at, p, n);
    w->at += n;
}
static void bytes_in(reader *r, uint8_t *p, size_t n) {
    if (!r->ok || n > r->len - r->at) {
        r->ok = 0;
        return;
    }
    memcpy(p, r->p + r->at, n);
    r->at += n;
}
static int hash_valid(const uint8_t *p) {
    unsigned v = 0;
    for (unsigned i = 0; i < 32; i++)
        v |= p[i];
    return v != 0;
}
static void entity_out(writer *w, dd_dng_entity e) {
    put(w, e.kind, 1);
    put(w, e.id, 4);
}
static dd_dng_entity entity_in(reader *r) {
    dd_dng_entity e;
    e.kind = (uint8_t)get(r, 1);
    e.id = (uint32_t)get(r, 4);
    return e;
}
static void offer_out(writer *w, const dd_dng_departure *d) {
    put(w, d->quest, 4);
    put(w, d->dungeon, 4);
    put(w, d->week, 4);
    put(w, d->owner_revision, 4);
    put(w, d->players, 1);
    put(w, d->connected, 1);
    for (unsigned i = 0; i < 4; i++) {
        put(w, d->heroes[i], 4);
        put(w, d->owners[i], 1);
    }
    put(w, d->map_seed, 4);
    put(w, d->rules_seed, 4);
    put(w, d->battle_seed, 4);
    put(w, d->quest_seed, 4);
    bytes_out(w, d->content_hash, 32);
    bytes_out(w, d->town_hash, 32);
    put(w, d->item_count, 1);
    for (unsigned i = 0; i < d->item_count; i++) {
        put(w, d->items[i].type, 4);
        put(w, d->items[i].id, 4);
        put(w, d->items[i].count, 4);
    }
}
static void offer_in(reader *r, dd_dng_departure *d) {
    d->quest = (uint32_t)get(r, 4);
    d->dungeon = (uint32_t)get(r, 4);
    d->week = (uint32_t)get(r, 4);
    d->owner_revision = (uint32_t)get(r, 4);
    d->players = (uint8_t)get(r, 1);
    d->connected = (uint8_t)get(r, 1);
    for (unsigned i = 0; i < 4; i++) {
        d->heroes[i] = (uint32_t)get(r, 4);
        d->owners[i] = (uint8_t)get(r, 1);
    }
    d->map_seed = (uint32_t)get(r, 4);
    d->rules_seed = (uint32_t)get(r, 4);
    d->battle_seed = (uint32_t)get(r, 4);
    d->quest_seed = (uint32_t)get(r, 4);
    bytes_in(r, d->content_hash, 32);
    bytes_in(r, d->town_hash, 32);
    d->item_count = (uint8_t)get(r, 1);
    if (d->item_count > DD_DNG_PROVISIONS) {
        r->ok = 0;
        return;
    }
    for (unsigned i = 0; i < d->item_count; i++) {
        d->items[i].type = (uint32_t)get(r, 4);
        d->items[i].id = (uint32_t)get(r, 4);
        d->items[i].count = (uint32_t)get(r, 4);
    }
}
static void checkpoint_out(writer *w, const dd_dng_checkpoint *c) {
    put(w, c->scene, 1);
    put(w, c->location, 4);
    put(w, c->encounter, 4);
    put(w, c->turn, 4);
    entity_out(w, c->active);
    put(w, c->rules_seed, 4);
    put(w, c->battle_seed, 4);
    put(w, c->quest_seed, 4);
    bytes_out(w, c->hash, 32);
}
static void checkpoint_in(reader *r, dd_dng_checkpoint *c) {
    c->scene = (uint8_t)get(r, 1);
    c->location = (uint32_t)get(r, 4);
    c->encounter = (uint32_t)get(r, 4);
    c->turn = (uint32_t)get(r, 4);
    c->active = entity_in(r);
    c->rules_seed = (uint32_t)get(r, 4);
    c->battle_seed = (uint32_t)get(r, 4);
    c->quest_seed = (uint32_t)get(r, 4);
    bytes_in(r, c->hash, 32);
}
static void command_out(writer *w, const dd_dng_command *c) {
    put(w, c->request, 4);
    put(w, c->argument, 4);
    put(w, c->kind, 1);
    entity_out(w, c->actor);
    put(w, c->target_count, 1);
    for (unsigned i = 0; i < c->target_count; i++)
        entity_out(w, c->targets[i]);
    bytes_out(w, c->before_hash, 32);
}
static void command_in(reader *r, dd_dng_command *c) {
    c->request = (uint32_t)get(r, 4);
    c->argument = (uint32_t)get(r, 4);
    c->kind = (uint8_t)get(r, 1);
    c->actor = entity_in(r);
    c->target_count = (uint8_t)get(r, 1);
    if (c->target_count > DD_DNG_TARGETS) {
        r->ok = 0;
        return;
    }
    for (unsigned i = 0; i < c->target_count; i++)
        c->targets[i] = entity_in(r);
    bytes_in(r, c->before_hash, 32);
}
static int valid(const dd_dng_message *m) {
    if (!m->expedition) return 0;
    switch (m->type) {
    case DD_DNG_OFFER:
        return !m->sequence && dd_dng_departure_valid(&m->u.offer);
    case DD_DNG_READY:
        return !m->sequence && hash_valid(m->u.ready.town) && hash_valid(m->u.ready.content);
    case DD_DNG_LAUNCH:
        return !m->sequence;
    case DD_DNG_CHECKPOINT:
        return !m->sequence && dd_dng_checkpoint_valid(&m->u.checkpoint);
    case DD_DNG_COMMAND:
        return dd_dng_command_valid(&m->u.command);
    case DD_DNG_EVENT:
        return m->sequence && m->u.event.origin < DD_DNG_PLAYERS &&
               dd_dng_command_valid(&m->u.event.command) && dd_dng_checkpoint_valid(&m->u.event.after);
    case DD_DNG_ACK:
    case DD_DNG_RELEASE:
        return hash_valid(m->u.hash);
    case DD_DNG_REJECT:
        return m->u.reject.request && m->u.reject.reason >= DD_DNG_INVALID &&
               m->u.reject.reason <= DD_DNG_DUPLICATE;
    case DD_DNG_MEMBERS:
        return (m->u.connected & 1) && m->u.connected < 16;
    default:
        return 0;
    }
}
size_t dd_dng_encode(const dd_dng_message *m, uint8_t *out, size_t cap) {
    if (!m || !out || cap < DD_DNG_WIRE_HEADER || !valid(m)) return 0;
    writer w = {out, 0, cap < DD_DNG_WIRE_MAX ? cap : DD_DNG_WIRE_MAX, 1};
    put(&w, DD_DNG_WIRE_MAGIC, 4);
    put(&w, DD_DNG_WIRE_VERSION, 2);
    put(&w, m->type, 2);
    put(&w, 0, 4);
    put(&w, m->sequence, 4);
    put(&w, m->expedition, 8);
    switch (m->type) {
    case DD_DNG_OFFER:
        offer_out(&w, &m->u.offer);
        break;
    case DD_DNG_READY:
        bytes_out(&w, m->u.ready.town, 32);
        bytes_out(&w, m->u.ready.content, 32);
        break;
    case DD_DNG_LAUNCH:
        break;
    case DD_DNG_CHECKPOINT:
        checkpoint_out(&w, &m->u.checkpoint);
        break;
    case DD_DNG_COMMAND:
        command_out(&w, &m->u.command);
        break;
    case DD_DNG_EVENT:
        put(&w, m->u.event.origin, 1);
        command_out(&w, &m->u.event.command);
        checkpoint_out(&w, &m->u.event.after);
        break;
    case DD_DNG_ACK:
    case DD_DNG_RELEASE:
        bytes_out(&w, m->u.hash, 32);
        break;
    case DD_DNG_REJECT:
        put(&w, m->u.reject.request, 4);
        put(&w, m->u.reject.reason, 1);
        break;
    case DD_DNG_MEMBERS:
        put(&w, m->u.connected, 1);
        break;
    default:
        return 0;
    }
    if (!w.ok) return 0;
    size_t length = w.at;
    w.at = 8;
    put(&w, length - DD_DNG_WIRE_HEADER, 4);
    return length;
}
int dd_dng_decode(const uint8_t *data, size_t len, dd_dng_message *out) {
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (!data || len < DD_DNG_WIRE_HEADER || len > DD_DNG_WIRE_MAX) return 0;
    reader r = {data, 0, len, 1};
    dd_dng_message m = {0};
    if (get(&r, 4) != DD_DNG_WIRE_MAGIC || get(&r, 2) != DD_DNG_WIRE_VERSION) return 0;
    m.type = (uint16_t)get(&r, 2);
    if (get(&r, 4) != len - DD_DNG_WIRE_HEADER) return 0;
    m.sequence = (uint32_t)get(&r, 4);
    m.expedition = get(&r, 8);
    switch (m.type) {
    case DD_DNG_OFFER:
        offer_in(&r, &m.u.offer);
        break;
    case DD_DNG_READY:
        bytes_in(&r, m.u.ready.town, 32);
        bytes_in(&r, m.u.ready.content, 32);
        break;
    case DD_DNG_LAUNCH:
        break;
    case DD_DNG_CHECKPOINT:
        checkpoint_in(&r, &m.u.checkpoint);
        break;
    case DD_DNG_COMMAND:
        command_in(&r, &m.u.command);
        break;
    case DD_DNG_EVENT:
        m.u.event.origin = (uint8_t)get(&r, 1);
        command_in(&r, &m.u.event.command);
        checkpoint_in(&r, &m.u.event.after);
        break;
    case DD_DNG_ACK:
    case DD_DNG_RELEASE:
        bytes_in(&r, m.u.hash, 32);
        break;
    case DD_DNG_REJECT:
        m.u.reject.request = (uint32_t)get(&r, 4);
        m.u.reject.reason = (uint8_t)get(&r, 1);
        break;
    case DD_DNG_MEMBERS:
        m.u.connected = (uint8_t)get(&r, 1);
        break;
    default:
        return 0;
    }
    if (!r.ok || r.at != len || !valid(&m)) return 0;
    *out = m;
    return 1;
}
