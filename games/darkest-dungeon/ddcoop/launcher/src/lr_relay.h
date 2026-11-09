/* Stream relay: carries the co-op runtime's TCP connection between two launchers (platform independent;
 * launcher/tests/test_lr_core.c).
 *
 * Guest: the runtime connects to 127.0.0.1:<listen port>; each accepted connection becomes a stream that
 * is opened, fed and closed at the host with STREAM_* messages.
 * Host: for each stream the relay connects to the runtime's own port on 127.0.0.1, retrying while the
 * game is still loading, and passes the bytes both ways. Nothing is read or changed in the bytes: the
 * runtime's own encrypted session runs end to end through the relay. */
#ifndef LR_RELAY_H
#define LR_RELAY_H

#include "lr_sock.h"
#include "lr_wire.h"

#define LR_MAX_STREAMS 8
#define LR_CONNECT_PATIENCE_MS (5 * 60 * 1000) /* host: how long a stream waits for the game to listen */
#define LR_MAX_BUFFERED (8u * 1024 * 1024)     /* bytes waiting for the local socket before the stream fails */

/* Sends one message to a peer. 1 = sent, 0 = try again later (send buffer full), -1 = peer is gone. */
typedef int (*lr_send_fn)(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len);

typedef struct {
    int used;
    uint64_t peer;
    uint32_t id;
    lr_sock sock;
    int connected;
    uint64_t give_up_at, next_try_at;
    uint8_t *to_local; /* bytes from the peer still to be written into the local socket */
    size_t to_local_len, to_local_cap;
    int peer_closed;   /* CLOSE received: shut down after the buffer is written */
    int local_closed;  /* the local socket ended: CLOSE sent (or pending) */
    uint8_t pend[LR_MAX_MSG]; /* one encoded message the transport could not take yet */
    size_t pend_len;
} lr_stream;

typedef struct {
    lr_role role;
    int port;             /* host: the runtime's port; guest: our listen port */
    lr_sock listener;     /* guest only */
    uint64_t host_peer;   /* guest only: where streams go (0 = not yet, connections wait) */
    lr_send_fn send;
    void *ctx;
    lr_stream streams[LR_MAX_STREAMS];
    uint32_t next_id;
    uint64_t bytes_to_peer, bytes_from_peer;
    int game_connected;   /* a local game connection is (or was) up */
    char last_event[160];
} lr_relay;

/* Guest: port 0 picks a free one (read it back from r->port). Returns 1 on success. */
int lr_relay_init(lr_relay *r, lr_role role, int port, lr_send_fn send, void *ctx);
void lr_relay_free(lr_relay *r);

/* A channel-0 message from peer (the caller has checked the peer is allowed). */
void lr_relay_on_message(lr_relay *r, uint64_t peer, const lr_msg *m, uint64_t now_ms);

/* Drops every stream of a peer (it left or timed out). */
void lr_relay_drop_peer(lr_relay *r, uint64_t peer);

/* Moves bytes; call often (every few ms). */
void lr_relay_tick(lr_relay *r, uint64_t now_ms);

int lr_relay_active_streams(const lr_relay *r);

#endif
