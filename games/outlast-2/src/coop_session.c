#include "coop_session.h"

static int same_addr(const CoopNetAddress* a, const CoopNetAddress* b)
{
    return a->ip == b->ip && a->port == b->port;
}

void coop_session_init(CoopSession* s, int is_host)
{
    s->is_host = is_host;
    for (int i = 0; i < COOP_SESSION_MAX; i++) {
        CoopSessionPlayer* p = &s->players[i];
        p->active = 0;
        p->ready = 0;
        p->has_state = 0;
        p->addr.ip = 0;
        p->addr.port = 0;
        p->last_seen_ms = 0;
    }
}

int coop_session_find(const CoopSession* s, const CoopNetAddress* addr)
{
    if (!addr) return -1;
    for (int i = 0; i < COOP_SESSION_MAX; i++) {
        if (s->players[i].active && same_addr(&s->players[i].addr, addr))
            return i;
    }
    return -1;
}

int coop_session_add(CoopSession* s, const CoopNetAddress* addr)
{
    if (!addr) return -1;

    int existing = coop_session_find(s, addr);
    if (existing >= 0)
        return existing;

    for (int i = 0; i < COOP_SESSION_MAX; i++) {
        if (!s->players[i].active) {
            CoopSessionPlayer* p = &s->players[i];
            p->active = 1;
            p->ready = 0;
            p->has_state = 0;
            p->addr = *addr;
            p->last_seen_ms = 0;
            return i;
        }
    }
    return -1; // full
}

int coop_session_remove(CoopSession* s, int slot)
{
    if (slot < 0 || slot >= COOP_SESSION_MAX) return -1;
    if (!s->players[slot].active) return -1;
    s->players[slot].active = 0;
    s->players[slot].ready = 0;
    s->players[slot].has_state = 0;
    return 0;
}

void coop_session_touch(CoopSession* s, int slot, unsigned int now_ms)
{
    if (slot < 0 || slot >= COOP_SESSION_MAX) return;
    if (!s->players[slot].active) return;
    s->players[slot].last_seen_ms = now_ms;
}

int coop_session_expire(CoopSession* s, unsigned int now_ms, unsigned int timeout_ms)
{
    int removed = 0;
    for (int i = 0; i < COOP_SESSION_MAX; i++) {
        CoopSessionPlayer* p = &s->players[i];
        if (!p->active) continue;
        if ((unsigned int)(now_ms - p->last_seen_ms) >= timeout_ms) {
            p->active = 0;
            p->ready = 0;
            p->has_state = 0;
            removed++;
        }
    }
    return removed;
}

int coop_session_relay_targets(const CoopSession* s, int sender_slot,
                               CoopNetAddress* out, int max)
{
    if (!out || max <= 0) return 0;
    int n = 0;
    for (int i = 0; i < COOP_SESSION_MAX && n < max; i++) {
        if (i == sender_slot) continue;
        if (!s->players[i].active) continue;
        out[n++] = s->players[i].addr;
    }
    return n;
}

int coop_session_update_state(CoopSession* s, int slot, const CoopPlayerState* state)
{
    if (slot < 0 || slot >= COOP_SESSION_MAX || !state) return -1;
    if (!s->players[slot].active) return -1;
    s->players[slot].state = *state;
    s->players[slot].has_state = 1;
    return 0;
}

int coop_session_snapshot(const CoopSession* s, int exclude_slot,
                          CoopPlayerState* out, int max)
{
    if (!out || max <= 0) return 0;
    int n = 0;
    for (int i = 0; i < COOP_SESSION_MAX && n < max; i++) {
        if (i == exclude_slot) continue;
        if (!s->players[i].active || !s->players[i].has_state) continue;
        out[n++] = s->players[i].state;
    }
    return n;
}

int coop_session_count(const CoopSession* s)
{
    int n = 0;
    for (int i = 0; i < COOP_SESSION_MAX; i++) {
        if (s->players[i].active) n++;
    }
    return n;
}
