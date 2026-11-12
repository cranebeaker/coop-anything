#ifndef COOP_RELAY_H
#define COOP_RELAY_H

// Host relay / client pump for the Outlast 2 co-op layer (Phase 3 prep).
// Ties the UDP transport (coop_net) to the session table (coop_session):
//
//   host   : receives packets, validates them, tracks players, relays player
//            state / world events to the other players, answers JOIN with a
//            late-join snapshot, and expires silent players.
//   client : sends its own state to the host, and hands every packet it
//            receives to the sink callback.
//
// No game code and no CRT. Time is injected into `pump` so behaviour is
// deterministic in tests.

#include "coop_protocol.h"
#include "coop_net.h"
#include "coop_session.h"

#define COOP_RELAY_MAX_PACKETS_PER_PUMP 32
#define COOP_RELAY_RECV_TIMEOUT_MS      1  // short, keeps a pump close to non-blocking
#define COOP_RELAY_TIMEOUT_MS           5000 // drop a player after 5 s of silence

typedef struct CoopRelayStats {
    unsigned int rx_packets;
    unsigned int rx_invalid;    // bad magic/version/type/length
    unsigned int rx_unknown;    // valid packet from an address that never joined
    unsigned int rx_full;       // JOIN rejected, session full
    unsigned int tx_relay;      // packets forwarded to other players
    unsigned int tx_snapshot;   // snapshot packets sent to a joining player
    unsigned int expired;       // players dropped for silence
} CoopRelayStats;

// Called for every accepted packet, with `slot` being the sender's slot
// (-1 on the client side, where the host owns slot assignment).
typedef void (*CoopRelaySink)(void* user, const void* packet, int packet_len,
                              const CoopNetAddress* from, int slot);

typedef struct CoopRelay {
    int              is_host;
    int              started;
    unsigned long long sock;
    CoopNetAddress   host_addr; // client only
    CoopSession      session;   // host only
    unsigned int     seq;       // outgoing sequence counter
    unsigned int     now_ms;    // last pumped time, host only
    CoopRelayStats   stats;
    CoopRelaySink    sink;
    void*            sink_user;
} CoopRelay;

void coop_relay_init(CoopRelay* r, int is_host);

// Host: bind a UDP socket (port 0 = ephemeral). Returns 0 on success.
int coop_relay_host_start(CoopRelay* r, unsigned short bind_port);

// Client: bind an ephemeral socket and record where the host is.
// Returns 0 on success.
int coop_relay_client_start(CoopRelay* r, unsigned int host_ip, unsigned short host_port);

void coop_relay_stop(CoopRelay* r);

void coop_relay_set_sink(CoopRelay* r, CoopRelaySink sink, void* user);

// Client: send JOIN to the host (host allocates a slot).
int coop_relay_join(CoopRelay* r);

// Send a player state. Client -> host; host -> all active players.
int coop_relay_send_state(CoopRelay* r, const CoopPlayerState* state);

// Send a world event. Client -> host; host -> all active players.
int coop_relay_send_event(CoopRelay* r, const CoopWorldEvent* ev);

// Processes up to COOP_RELAY_MAX_PACKETS_PER_PUMP received packets.
// `now_ms` drives join tracking and expiry on the host.
// Returns the number of packets processed.
int coop_relay_pump(CoopRelay* r, unsigned int now_ms);

// Host: number of active players.
int coop_relay_player_count(const CoopRelay* r);

#endif // COOP_RELAY_H
