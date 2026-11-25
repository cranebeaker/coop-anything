#ifndef COOP_SESSION_H
#define COOP_SESSION_H

// Relay session state for the Outlast 2 co-op layer (Phase 3 prep).
// Pure logic (no sockets, no game): host tracks connected players by address,
// keeps their last known state for late-join snapshots, and computes relay
// fan-out targets. Time is injected so the logic is deterministic in tests.

#include "coop_protocol.h"
#include "coop_net.h"

#define COOP_SESSION_MAX COOP_MAX_PLAYERS

typedef struct CoopSessionPlayer {
    int            active;
    int            ready;
    int            has_state;
    CoopNetAddress addr;
    unsigned int   last_seen_ms;
    CoopPlayerState state; // last known state, for late-join snapshots
} CoopSessionPlayer;

typedef struct CoopSession {
    int               is_host;
    CoopSessionPlayer players[COOP_SESSION_MAX];
} CoopSession;

void coop_session_init(CoopSession* s, int is_host);

// Returns slot (0..COOP_SESSION_MAX-1) or -1 when full.
// Re-adding a known address returns its existing slot.
int coop_session_add(CoopSession* s, const CoopNetAddress* addr);

// Returns slot or -1 when unknown.
int coop_session_find(const CoopSession* s, const CoopNetAddress* addr);

// Returns 0 on success, -1 for invalid/empty slot.
int coop_session_remove(CoopSession* s, int slot);

void coop_session_touch(CoopSession* s, int slot, unsigned int now_ms);

// Marks players not seen for >= timeout_ms as inactive. Returns removed count.
int coop_session_expire(CoopSession* s, unsigned int now_ms, unsigned int timeout_ms);

// Copies addresses of all active players except sender_slot into out.
// Returns number of targets written (<= max).
int coop_session_relay_targets(const CoopSession* s, int sender_slot,
                               CoopNetAddress* out, int max);

// Stores the latest player state for a slot. Returns 0 on success.
int coop_session_update_state(CoopSession* s, int slot, const CoopPlayerState* state);

// Copies states of active players (excluding exclude_slot) that have state.
// Returns number written (<= max). Used for late-join snapshots.
int coop_session_snapshot(const CoopSession* s, int exclude_slot,
                          CoopPlayerState* out, int max);

// Number of active players.
int coop_session_count(const CoopSession* s);

#endif // COOP_SESSION_H
