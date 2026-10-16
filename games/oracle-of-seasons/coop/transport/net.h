#ifndef COOP_NET_H
#define COOP_NET_H
#include <stddef.h>
#include <stdint.h>

#define NET_PAYLOAD_MAX 4096u
#define NET_FRAME_MAX (12u + 20u + NET_PAYLOAD_MAX)
#define NET_CAP_TWO_PLAYERS 1u
#define NET_P1 1u
#define NET_P2 2u

enum net_kind { NET_READY = 1, NET_INPUT = 2, NET_VIEW = 3, NET_EVENT = 4,
                NET_CLOSED = 5, NET_ERROR = 6 };
struct net_config {
    uint32_t protocol;
    unsigned char build[32];
    unsigned char rom[32];
    uint32_t capabilities; /* Adapter must report actual native two-player mode. */
    uint64_t epoch; /* Host supplied; nonzero. Guest value is ignored. */
};
struct net_event {
    enum net_kind kind;
    unsigned player;
    uint64_t epoch, sequence;
    size_t length;
    unsigned char payload[NET_PAYLOAD_MAX];
};
struct net_host {
    int fd, guest_fd;
    uint16_t port;
    uint64_t epoch, input_sequence; /* Retained across P2 reconnects. */
};
/* Storage is public for stack allocation, not for application mutation/copying. */
struct net_peer {
    int fd, host_side, ready, terminal;
    struct net_host *owner;
    struct net_config config;
    uint64_t epoch, rx_sequence, tx_sequence;
    size_t in_used, in_need, out_used, out_sent;
    unsigned char in[NET_FRAME_MAX], out[NET_FRAME_MAX];
};
/* Return 0 on setup success, -1 on invalid config/address or OS error. */
int net_listen(struct net_host *, const char *address, uint16_t port);
void net_host_close(struct net_host *);
/* Accept: 1 connected, 0 would block, -1 error, -2 second guest refused. */
int net_accept(struct net_host *, struct net_peer *, const struct net_config *);
int net_connect(struct net_peer *, const char *address, uint16_t port,
                const struct net_config *);
/* Pump at most one event; 1 event, 0 pending, -1 closed/error (event set). */
int net_pump(struct net_peer *, struct net_event *);
/* Single bounded output slot: 1 queued, 0 busy, -1 invalid/not ready. */
int net_send(struct net_peer *, enum net_kind, uint64_t sequence,
             const void *, size_t);
void net_peer_close(struct net_peer *);
#endif
