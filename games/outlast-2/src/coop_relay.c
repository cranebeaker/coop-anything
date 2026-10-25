#include "coop_relay.h"

static void default_header(CoopRelay* r, CoopPacketHeader* hdr, unsigned char type)
{
    hdr->magic = COOP_MAGIC;
    hdr->version = COOP_PROTO_VERSION;
    hdr->type = type;
    hdr->seq = r->seq++;
}

void coop_relay_init(CoopRelay* r, int is_host)
{
    r->is_host = is_host;
    r->started = 0;
    r->sock = COOP_NET_INVALID_SOCKET;
    r->host_addr.ip = 0;
    r->host_addr.port = 0;
    r->seq = 1;
    r->now_ms = 0;
    r->stats.rx_packets = 0;
    r->stats.rx_invalid = 0;
    r->stats.rx_unknown = 0;
    r->stats.rx_full = 0;
    r->stats.tx_relay = 0;
    r->stats.tx_snapshot = 0;
    r->stats.expired = 0;
    r->sink = 0;
    r->sink_user = 0;
    coop_session_init(&r->session, is_host);
}

int coop_relay_host_start(CoopRelay* r, unsigned short bind_port)
{
    if (!r->is_host) return -1;
    r->sock = coop_net_open(bind_port);
    if (r->sock == COOP_NET_INVALID_SOCKET)
        return -1;
    r->started = 1;
    return 0;
}

int coop_relay_client_start(CoopRelay* r, unsigned int host_ip, unsigned short host_port)
{
    if (r->is_host) return -1;
    r->sock = coop_net_open(0);
    if (r->sock == COOP_NET_INVALID_SOCKET)
        return -1;
    r->host_addr.ip = host_ip;
    r->host_addr.port = host_port;
    r->started = 1;
    return 0;
}

void coop_relay_stop(CoopRelay* r)
{
    if (r->started) {
        coop_net_close(r->sock);
        r->sock = COOP_NET_INVALID_SOCKET;
        r->started = 0;
    }
}

void coop_relay_set_sink(CoopRelay* r, CoopRelaySink sink, void* user)
{
    r->sink = sink;
    r->sink_user = user;
}

int coop_relay_join(CoopRelay* r)
{
    if (r->is_host || !r->started) return -1;
    CoopPacketHeader hdr;
    default_header(r, &hdr, COOP_PKT_JOIN);
    return coop_net_send(r->sock, &hdr, (int)sizeof(hdr), &r->host_addr) == (int)sizeof(hdr) ? 0 : -1;
}

// Host: forward one raw packet to every active player except the sender.
static int relay_to_others(CoopRelay* r, int sender_slot, const void* data, int len)
{
    CoopNetAddress targets[COOP_MAX_PLAYERS];
    int n = coop_session_relay_targets(&r->session, sender_slot, targets, COOP_MAX_PLAYERS);
    int sent = 0;
    for (int i = 0; i < n; i++) {
        if (coop_net_send(r->sock, data, len, &targets[i]) == len)
            sent++;
    }
    r->stats.tx_relay += (unsigned int)sent;
    return sent;
}

// Host: send the last known state of every other player to a joining address.
static void send_snapshot(CoopRelay* r, int joiner_slot, const CoopNetAddress* to)
{
    CoopPlayerState states[COOP_MAX_PLAYERS];
    int n = coop_session_snapshot(&r->session, joiner_slot, states, COOP_MAX_PLAYERS);
    for (int i = 0; i < n; i++) {
        CoopPlayerState pkt = states[i];
        pkt.hdr.magic = COOP_MAGIC;
        pkt.hdr.version = COOP_PROTO_VERSION;
        pkt.hdr.type = COOP_PKT_PLAYER_STATE;
        pkt.hdr.seq = r->seq++;
        if (coop_net_send(r->sock, &pkt, (int)sizeof(pkt), to) == (int)sizeof(pkt))
            r->stats.tx_snapshot++;
    }
}

static int handle_host_packet(CoopRelay* r, const void* data, int len,
                              const CoopNetAddress* from)
{
    const CoopPacketHeader* hdr = (const CoopPacketHeader*)data;
    int slot = coop_session_find(&r->session, from);

    if (hdr->type == COOP_PKT_JOIN) {
        if (slot < 0) {
            slot = coop_session_add(&r->session, from);
            if (slot < 0) {
                r->stats.rx_full++;
                return 0;
            }
        } else {
            // Duplicate JOIN (e.g. the JOIN packet was retried): refresh only.
            coop_session_touch(&r->session, slot, r->now_ms);
        }
        coop_session_touch(&r->session, slot, r->now_ms);
        send_snapshot(r, slot, from);
        if (r->sink)
            r->sink(r->sink_user, data, len, from, slot);
        return 1;
    }

    // Everything else must come from a player that has joined.
    if (slot < 0) {
        r->stats.rx_unknown++;
        return 0;
    }

    coop_session_touch(&r->session, slot, r->now_ms);

    if (hdr->type == COOP_PKT_PLAYER_STATE) {
        const CoopPlayerState* st = (const CoopPlayerState*)data;
        // The host owns slot assignment: never trust the sender's own slot byte.
        CoopPlayerState fixed = *st;
        fixed.slot = (unsigned char)slot;
        coop_session_update_state(&r->session, slot, &fixed);
        relay_to_others(r, slot, &fixed, (int)sizeof(fixed));
        if (r->sink)
            r->sink(r->sink_user, &fixed, (int)sizeof(fixed), from, slot);
        return 1;
    }

    relay_to_others(r, slot, data, len);
    if (r->sink)
        r->sink(r->sink_user, data, len, from, slot);
    return 1;
}

int coop_relay_pump(CoopRelay* r, unsigned int now_ms)
{
    if (!r->started) return 0;

    r->now_ms = now_ms;
    int processed = 0;

    while (processed < COOP_RELAY_MAX_PACKETS_PER_PUMP) {
        unsigned char buf[128];
        CoopNetAddress from;
        int n = coop_net_recv(r->sock, buf, (int)sizeof(buf), &from,
                              COOP_RELAY_RECV_TIMEOUT_MS);
        if (n < 0)
            break;
        processed++;
        r->stats.rx_packets++;

        if (!coop_packet_validate(buf, n)) {
            r->stats.rx_invalid++;
            continue;
        }

        if (r->is_host) {
            handle_host_packet(r, buf, n, &from);
        } else {
            if (r->sink)
                r->sink(r->sink_user, buf, n, &from, -1);
        }
    }

    if (r->is_host) {
        int removed = coop_session_expire(&r->session, now_ms, COOP_RELAY_TIMEOUT_MS);
        r->stats.expired += (unsigned int)removed;
    }

    return processed;
}

int coop_relay_send_state(CoopRelay* r, const CoopPlayerState* state)
{
    if (!r->started || !state) return -1;

    CoopPlayerState pkt = *state;
    pkt.hdr.magic = COOP_MAGIC;
    pkt.hdr.version = COOP_PROTO_VERSION;
    pkt.hdr.type = COOP_PKT_PLAYER_STATE;
    pkt.hdr.seq = r->seq++;

    if (!r->is_host)
        return coop_net_send(r->sock, &pkt, (int)sizeof(pkt), &r->host_addr) == (int)sizeof(pkt) ? 0 : -1;

    // Host broadcasting its own state: everyone is a relay target.
    return relay_to_others(r, -1, &pkt, (int)sizeof(pkt)) > 0 ? 0 : -1;
}

int coop_relay_send_event(CoopRelay* r, const CoopWorldEvent* ev)
{
    if (!r->started || !ev) return -1;

    CoopWorldEvent pkt = *ev;
    pkt.hdr.magic = COOP_MAGIC;
    pkt.hdr.version = COOP_PROTO_VERSION;
    pkt.hdr.type = COOP_PKT_WORLD_EVENT;
    pkt.hdr.seq = r->seq++;

    if (!r->is_host)
        return coop_net_send(r->sock, &pkt, (int)sizeof(pkt), &r->host_addr) == (int)sizeof(pkt) ? 0 : -1;

    return relay_to_others(r, -1, &pkt, (int)sizeof(pkt)) > 0 ? 0 : -1;
}

int coop_relay_player_count(const CoopRelay* r)
{
    if (!r->is_host) return 0;
    return coop_session_count(&r->session);
}
