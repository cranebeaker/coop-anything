#ifndef COOP_INTERP_H
#define COOP_INTERP_H

// Remote-puppet interpolation for the Outlast 2 co-op layer.
//
// Each remote player's `CoopPlayerState` packets land in a small jitter buffer
// and are played back a fixed delay behind real time, so a late or missing
// packet is hidden by the buffer instead of stuttering the puppet.
//
//   push(state, recv_ms)   -> buffer one snapshot (stale/duplicate seq dropped)
//   sample(slot, now_ms)   -> position/rotation to draw this frame
//
// Behaviour:
//   - playback time = now - delay; snapshots are lerped between the two that
//     bracket it (positions linear, yaw/pitch along the shortest arc);
//   - before the oldest snapshot the oldest is held;
//   - after the newest snapshot the last velocity is extrapolated for at most
//     COOP_INTERP_MAX_EXTRAP_MS, then the puppet holds still;
//   - a jump larger than COOP_INTERP_TELEPORT_DIST between two snapshots snaps
//     to the newer one (streaming load, ladder, scripted move) instead of
//     sliding the puppet across the level.
//
// No game code and no CRT. Time is injected, so behaviour is deterministic.

#include "coop_protocol.h"

#define COOP_INTERP_BUFFER        8     // snapshots kept per remote player
#define COOP_INTERP_DELAY_MS      100   // jitter-buffer playback delay
#define COOP_INTERP_MAX_EXTRAP_MS 150   // stop extrapolating after this
#define COOP_INTERP_TELEPORT_DIST 800.0f // UE3 units (~8 m); larger = teleport

typedef struct CoopInterpSnapshot {
    unsigned int    seq;
    unsigned int    recv_ms;
    CoopPlayerState state;
} CoopInterpSnapshot;

typedef struct CoopInterpPlayer {
    int               active;
    int               count;
    unsigned int      last_seq;
    CoopInterpSnapshot buf[COOP_INTERP_BUFFER];
} CoopInterpPlayer;

typedef struct CoopInterp {
    CoopInterpPlayer players[COOP_MAX_PLAYERS];
    unsigned int     delay_ms;
} CoopInterp;

void coop_interp_init(CoopInterp* it, unsigned int delay_ms);

// Drops every buffered player (level change, session restart).
void coop_interp_reset(CoopInterp* it);

// Buffers one received state for its host-assigned slot.
// Returns 1 when accepted, 0 when rejected (unknown slot, stale seq).
int coop_interp_push(CoopInterp* it, const CoopPlayerState* state, unsigned int recv_ms);

// Writes the state to draw for `slot` at `now_ms` into *out.
// Returns 1 when a state was produced, 0 when the player has no data.
int coop_interp_sample(const CoopInterp* it, int slot, unsigned int now_ms, CoopPlayerState* out);

// Forgets a player (disconnect / slot reuse).
void coop_interp_remove(CoopInterp* it, int slot);

int coop_interp_active_count(const CoopInterp* it);

#endif // COOP_INTERP_H
