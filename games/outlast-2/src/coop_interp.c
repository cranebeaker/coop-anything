#include "coop_interp.h"

// MSVC emits a reference to _fltused when floating point is used and the CRT
// normally provides it; this DLL is freestanding (/NODEFAULTLIB).
int _fltused = 0;

// ================= small float helpers (no libm in a freestanding build) =================

static float wrap180(float deg)
{
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

static float lerp_f(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float dist_sq(const float a[3], const float b[3])
{
    float dx = b[0] - a[0];
    float dy = b[1] - a[1];
    float dz = b[2] - a[2];
    return dx * dx + dy * dy + dz * dz;
}

// ================= buffer =================

void coop_interp_init(CoopInterp* it, unsigned int delay_ms)
{
    it->delay_ms = delay_ms;
    coop_interp_reset(it);
}

void coop_interp_reset(CoopInterp* it)
{
    for (int i = 0; i < COOP_MAX_PLAYERS; i++) {
        it->players[i].active = 0;
        it->players[i].count = 0;
        it->players[i].last_seq = 0;
    }
}

void coop_interp_remove(CoopInterp* it, int slot)
{
    if (slot < 0 || slot >= COOP_MAX_PLAYERS) return;
    it->players[slot].active = 0;
    it->players[slot].count = 0;
}

int coop_interp_active_count(const CoopInterp* it)
{
    int n = 0;
    for (int i = 0; i < COOP_MAX_PLAYERS; i++) {
        if (it->players[i].active) n++;
    }
    return n;
}

static void push_sorted(CoopInterpPlayer* p, const CoopInterpSnapshot* snap)
{
    if (p->count == COOP_INTERP_BUFFER) {
        // Drop the oldest snapshot to make room.
        for (int i = 1; i < p->count; i++)
            p->buf[i - 1] = p->buf[i];
        p->count--;
    }

    // Insert in receive-time order (arrival is usually ordered; this keeps the
    // buffer correct if two packets land in the same tick out of order).
    int at = p->count;
    while (at > 0 && p->buf[at - 1].recv_ms > snap->recv_ms) {
        p->buf[at] = p->buf[at - 1];
        at--;
    }
    p->buf[at] = *snap;
    p->count++;
}

int coop_interp_push(CoopInterp* it, const CoopPlayerState* state, unsigned int recv_ms)
{
    if (!state || state->slot >= COOP_MAX_PLAYERS) return 0;

    CoopInterpPlayer* p = &it->players[state->slot];
    if (p->active && (int)(state->hdr.seq - p->last_seq) <= 0)
        return 0; // stale, duplicate or reordered older than what we already have

    CoopInterpSnapshot snap;
    snap.seq = state->hdr.seq;
    snap.recv_ms = recv_ms;
    snap.state = *state;

    p->active = 1;
    p->last_seq = state->hdr.seq;
    push_sorted(p, &snap);
    return 1;
}

// ================= sampling =================

static void copy_state(const CoopPlayerState* from, CoopPlayerState* to)
{
    *to = *from;
}

// Blends two snapshots into *out at fraction t (0 = a, 1 = b).
static void blend(const CoopInterpSnapshot* a, const CoopInterpSnapshot* b, float t,
                  CoopPlayerState* out)
{
    if (dist_sq(a->state.pos, b->state.pos) >
        COOP_INTERP_TELEPORT_DIST * COOP_INTERP_TELEPORT_DIST) {
        // Teleport / level stream: do not slide the puppet through the world.
        copy_state(&b->state, out);
        return;
    }

    copy_state(&a->state, out);
    out->pos[0] = lerp_f(a->state.pos[0], b->state.pos[0], t);
    out->pos[1] = lerp_f(a->state.pos[1], b->state.pos[1], t);
    out->pos[2] = lerp_f(a->state.pos[2], b->state.pos[2], t);
    out->vel[0] = lerp_f(a->state.vel[0], b->state.vel[0], t);
    out->vel[1] = lerp_f(a->state.vel[1], b->state.vel[1], t);
    out->vel[2] = lerp_f(a->state.vel[2], b->state.vel[2], t);
    out->yaw = wrap180(a->state.yaw + wrap180(b->state.yaw - a->state.yaw) * t);
    out->pitch = wrap180(a->state.pitch + wrap180(b->state.pitch - a->state.pitch) * t);

    // Discrete state comes from the older snapshot: switching flags mid-blend
    // would flicker (e.g. crouch toggling every frame).
    out->flags = a->state.flags;
    out->misc = a->state.misc;
    out->stamina = a->state.stamina;
}

int coop_interp_sample(const CoopInterp* it, int slot, unsigned int now_ms, CoopPlayerState* out)
{
    if (slot < 0 || slot >= COOP_MAX_PLAYERS || !out) return 0;

    const CoopInterpPlayer* p = &it->players[slot];
    if (!p->active || p->count <= 0) return 0;

    // Playback runs delay_ms behind the newest arrival.
    unsigned int play = now_ms > it->delay_ms ? now_ms - it->delay_ms : 0;

    const CoopInterpSnapshot* first = &p->buf[0];
    const CoopInterpSnapshot* last = &p->buf[p->count - 1];

    if (play <= first->recv_ms) {
        copy_state(&first->state, out);
        return 1;
    }

    if (play >= last->recv_ms) {
        // Nothing newer: extrapolate briefly on the last velocity, then hold.
        unsigned int dt_ms = play - last->recv_ms;
        if (dt_ms > COOP_INTERP_MAX_EXTRAP_MS)
            dt_ms = COOP_INTERP_MAX_EXTRAP_MS;

        copy_state(&last->state, out);
        float dt = (float)dt_ms / 1000.0f;
        out->pos[0] = last->state.pos[0] + last->state.vel[0] * dt;
        out->pos[1] = last->state.pos[1] + last->state.vel[1] * dt;
        out->pos[2] = last->state.pos[2] + last->state.vel[2] * dt;
        return 1;
    }

    for (int i = 0; i + 1 < p->count; i++) {
        const CoopInterpSnapshot* a = &p->buf[i];
        const CoopInterpSnapshot* b = &p->buf[i + 1];
        if (play < a->recv_ms || play > b->recv_ms)
            continue;

        unsigned int span = b->recv_ms - a->recv_ms;
        float t = span > 0 ? (float)(play - a->recv_ms) / (float)span : 1.0f;
        blend(a, b, t, out);
        return 1;
    }

    // Should not happen (play is bracketed), but hold the newest if it does.
    copy_state(&last->state, out);
    return 1;
}
