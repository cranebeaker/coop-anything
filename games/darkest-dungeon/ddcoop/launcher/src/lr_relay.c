#include "lr_relay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETRY_MS 500

static void event(lr_relay *r, const char *text, uint32_t id) {
    snprintf(r->last_event, sizeof r->last_event, "%s (stream %u)", text, id);
}

int lr_relay_init(lr_relay *r, lr_role role, int port, lr_send_fn send, void *ctx) {
    memset(r, 0, sizeof *r);
    r->role = role;
    r->send = send;
    r->ctx = ctx;
    r->listener = LR_NOSOCK;
    r->next_id = 1;
    for (int i = 0; i < LR_MAX_STREAMS; i++) r->streams[i].sock = LR_NOSOCK;
    if (role == LR_ROLE_GUEST) {
        r->listener = lr_listen_local(port, &r->port);
        return r->listener != LR_NOSOCK;
    }
    r->port = port;
    return port > 0;
}

static void stream_free(lr_stream *s) {
    lr_close(s->sock);
    free(s->to_local);
    memset(s, 0, sizeof *s);
    s->sock = LR_NOSOCK;
}

void lr_relay_free(lr_relay *r) {
    for (int i = 0; i < LR_MAX_STREAMS; i++)
        if (r->streams[i].used) stream_free(&r->streams[i]);
    lr_close(r->listener);
    r->listener = LR_NOSOCK;
}

static lr_stream *find(lr_relay *r, uint64_t peer, uint32_t id) {
    for (int i = 0; i < LR_MAX_STREAMS; i++)
        if (r->streams[i].used && r->streams[i].peer == peer && r->streams[i].id == id) return &r->streams[i];
    return NULL;
}

static lr_stream *new_stream(lr_relay *r, uint64_t peer, uint32_t id) {
    for (int i = 0; i < LR_MAX_STREAMS; i++)
        if (!r->streams[i].used) {
            lr_stream *s = &r->streams[i];
            memset(s, 0, sizeof *s);
            s->used = 1;
            s->peer = peer;
            s->id = id;
            s->sock = LR_NOSOCK;
            return s;
        }
    return NULL;
}

/* Sends a control-free stream message now, or parks it in s->pend. Returns 1 if nothing is pending. */
static int send_or_park(lr_relay *r, lr_stream *s, const lr_msg *m) {
    size_t n = lr_encode(m, s->pend);
    if (n == 0) return 1;
    s->pend_len = n;
    int sent = r->send(r->ctx, s->peer, LR_CH_STREAM, s->pend, s->pend_len);
    if (sent == 1 || sent < 0) s->pend_len = 0; /* a gone peer is dropped by the caller */
    return s->pend_len == 0;
}

static void send_simple(lr_relay *r, uint64_t peer, lr_type t, uint32_t id) {
    lr_msg m = {0};
    uint8_t buf[LR_MAX_MSG];
    m.type = t;
    m.id = id;
    size_t n = lr_encode(&m, buf);
    /* OPEN and CLOSE are tiny; Steam queues them even when data is backed up. Retry a few times. */
    for (int tries = 0; tries < 50 && r->send(r->ctx, peer, LR_CH_STREAM, buf, n) == 0; tries++) {
    }
}

static int append(lr_stream *s, const uint8_t *data, size_t len) {
    if (s->to_local_len + len > LR_MAX_BUFFERED) return 0;
    if (s->to_local_len + len > s->to_local_cap) {
        size_t cap = s->to_local_cap ? s->to_local_cap : 65536;
        while (cap < s->to_local_len + len) cap *= 2;
        uint8_t *p = realloc(s->to_local, cap);
        if (!p) return 0;
        s->to_local = p;
        s->to_local_cap = cap;
    }
    memcpy(s->to_local + s->to_local_len, data, len);
    s->to_local_len += len;
    return 1;
}

static void fail_stream(lr_relay *r, lr_stream *s, const char *why) {
    event(r, why, s->id);
    send_simple(r, s->peer, LR_STREAM_CLOSE, s->id);
    stream_free(s);
}

void lr_relay_on_message(lr_relay *r, uint64_t peer, const lr_msg *m, uint64_t now_ms) {
    lr_stream *s = find(r, peer, m->id);
    switch (m->type) {
    case LR_STREAM_OPEN:
        if (r->role != LR_ROLE_HOST || s) return;
        s = new_stream(r, peer, m->id);
        if (!s) {
            send_simple(r, peer, LR_STREAM_CLOSE, m->id);
            return;
        }
        s->give_up_at = now_ms + LR_CONNECT_PATIENCE_MS;
        s->next_try_at = now_ms;
        event(r, "vriend opent verbinding", m->id);
        break;
    case LR_STREAM_DATA:
        if (!s) return;
        r->bytes_from_peer += m->len;
        if (!append(s, m->data, m->len)) fail_stream(r, s, "te veel wachtende gegevens");
        break;
    case LR_STREAM_CLOSE:
        if (!s) return;
        s->peer_closed = 1;
        /* Our CLOSE may still be parked by transport backpressure. Deliver it before freeing. */
        if (s->local_closed && s->to_local_len == 0 && s->pend_len == 0) stream_free(s);
        break;
    default:
        break;
    }
}

void lr_relay_drop_peer(lr_relay *r, uint64_t peer) {
    for (int i = 0; i < LR_MAX_STREAMS; i++)
        if (r->streams[i].used && r->streams[i].peer == peer) stream_free(&r->streams[i]);
}

int lr_relay_active_streams(const lr_relay *r) {
    int n = 0;
    for (int i = 0; i < LR_MAX_STREAMS; i++) n += r->streams[i].used;
    return n;
}

static void accept_local(lr_relay *r) {
    if (r->listener == LR_NOSOCK || r->host_peer == 0) return; /* connections wait in the backlog */
    for (;;) {
        lr_sock c = lr_accept_nb(r->listener);
        if (c == LR_NOSOCK) return;
        lr_stream *s = new_stream(r, r->host_peer, r->next_id++);
        if (!s) {
            lr_close(c);
            return;
        }
        s->sock = c;
        s->connected = 1;
        r->game_connected = 1;
        send_simple(r, s->peer, LR_STREAM_OPEN, s->id);
        event(r, "spel verbonden met DDCoop", s->id);
    }
}

static void host_connect(lr_relay *r, lr_stream *s, uint64_t now_ms) {
    if (s->sock != LR_NOSOCK) {
        int c = lr_connect_check(s->sock);
        if (c == 1) {
            s->connected = 1;
            r->game_connected = 1;
            event(r, "verbonden met het spel", s->id);
            return;
        }
        if (c == 0) return;
        lr_close(s->sock); /* refused: the game is not listening yet */
        s->sock = LR_NOSOCK;
        s->next_try_at = now_ms + RETRY_MS;
    }
    if (now_ms >= s->give_up_at) {
        fail_stream(r, s, "het spel van de host luisterde niet");
        return;
    }
    if (now_ms >= s->next_try_at) {
        s->sock = lr_connect_local_nb(r->port);
        s->next_try_at = now_ms + RETRY_MS;
    }
}

static void pump(lr_relay *r, lr_stream *s) {
    /* peer -> local */
    while (s->to_local_len > 0) {
        int n = lr_send_nb(s->sock, s->to_local, s->to_local_len);
        if (n < 0) {
            fail_stream(r, s, "spelverbinding verbroken");
            return;
        }
        if (n == 0) break;
        memmove(s->to_local, s->to_local + n, s->to_local_len - (size_t)n);
        s->to_local_len -= (size_t)n;
    }
    if (s->peer_closed && s->to_local_len == 0) {
        lr_shutdown_write(s->sock);
        if (s->local_closed && s->pend_len == 0) {
            stream_free(s);
            return;
        }
    }
    /* local -> peer, one message at a time so a full Steam buffer holds the game back instead of dropping */
    if (s->pend_len) {
        int sent = r->send(r->ctx, s->peer, LR_CH_STREAM, s->pend, s->pend_len);
        if (sent == 0) return;
        s->pend_len = 0;
    }
    for (int rounds = 0; rounds < 16 && !s->local_closed; rounds++) {
        uint8_t buf[LR_CHUNK];
        int n = lr_recv_nb(s->sock, buf, sizeof buf);
        if (n == 0) break;
        lr_msg m = {0};
        if (n < 0) {
            s->local_closed = 1;
            m.type = LR_STREAM_CLOSE;
            m.id = s->id;
            event(r, "spel sloot de verbinding", s->id);
        } else {
            r->bytes_to_peer += (uint64_t)n;
            m.type = LR_STREAM_DATA;
            m.id = s->id;
            m.data = buf;
            m.len = (size_t)n;
        }
        if (!send_or_park(r, s, &m)) break;
    }
    if (s->local_closed && s->pend_len == 0 && (s->peer_closed || r->role == LR_ROLE_HOST)) {
        if (s->to_local_len == 0) stream_free(s);
    }
}

void lr_relay_tick(lr_relay *r, uint64_t now_ms) {
    if (r->role == LR_ROLE_GUEST) accept_local(r);
    for (int i = 0; i < LR_MAX_STREAMS; i++) {
        lr_stream *s = &r->streams[i];
        if (!s->used) continue;
        if (!s->connected) {
            host_connect(r, s, now_ms);
            if (!s->used || !s->connected) continue;
        }
        pump(r, s);
    }
}
