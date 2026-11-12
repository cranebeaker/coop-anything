// End-to-end relay test for the co-op layer (no game needed).
// Runs a real host relay and real client relays over UDP loopback:
//   1. join + player-state relay, late-join snapshot of earlier state
//   2. host assigns slots (the sender's own slot byte is not trusted)
//   3. packets from an address that never joined are ignored
//   4. malformed packets (magic / version / type / length) are rejected
//   5. silent players expire from the session
//
// Freestanding (no CRT): built with /NODEFAULTLIB like the proxy DLL.
// Build + run: games/outlast-2/tests/run.bat

#include "win_min.h"
#include "coop_protocol.h"
#include "coop_net.h"
#include "coop_session.h"
#include "coop_relay.h"

int _fltused = 0;

#define LOOPBACK_IP 0x0100007Fu

static int g_failures = 0;

static unsigned long long str_len(const char* s)
{
    unsigned long long n = 0;
    while (s[n]) n++;
    return n;
}

static void out_str(const char* s)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    if (h && h != INVALID_HANDLE_VALUE)
        WriteFile(h, s, (DWORD)str_len(s), &written, NULL);
}

static void out_uint(unsigned long v)
{
    char buf[24];
    int i = 0;
    if (v == 0) {
        out_str("0");
        return;
    }
    while (v > 0 && i < 23) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    char out[24];
    int j = 0;
    while (i > 0) out[j++] = buf[--i];
    out[j] = '\0';
    out_str(out);
}

static void check_fail(int line)
{
    out_str("FAIL line ");
    out_uint((unsigned long)line);
    out_str("\n");
    g_failures++;
}

#define CHECK(cond)                        \
    do {                                   \
        if (!(cond)) check_fail(__LINE__); \
    } while (0)

static void copy_bytes(void* dst, const void* src, unsigned long long n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
}

static void zero_bytes(void* dst, unsigned long long n)
{
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = 0;
}

// ================= packet recorder (client sink) =================

#define REC_MAX 16

typedef struct Recorder {
    int count;
    int len[REC_MAX];
    unsigned char data[REC_MAX][128];
    int slot[REC_MAX];
    CoopNetAddress from[REC_MAX];
} Recorder;

static void recorder_sink(void* user, const void* packet, int packet_len,
                          const CoopNetAddress* from, int slot)
{
    Recorder* rec = (Recorder*)user;
    if (rec->count >= REC_MAX) return;
    int i = rec->count++;
    rec->len[i] = packet_len;
    rec->slot[i] = slot;
    rec->from[i] = *from;
    if (packet_len > (int)sizeof(rec->data[i]))
        packet_len = (int)sizeof(rec->data[i]);
    copy_bytes(rec->data[i], packet, (unsigned long long)packet_len);
}

static void recorder_init(Recorder* rec)
{
    zero_bytes(rec, sizeof(*rec));
}

static const CoopPlayerState* rec_state(const Recorder* rec, int i)
{
    return (const CoopPlayerState*)rec->data[i];
}

// ================= helpers =================

static void fill_state(CoopPlayerState* st, unsigned char flags, float x)
{
    zero_bytes(st, sizeof(*st));
    st->hdr.magic = COOP_MAGIC;
    st->hdr.version = COOP_PROTO_VERSION;
    st->hdr.type = COOP_PKT_PLAYER_STATE;
    st->slot = 0;
    st->flags = flags;
    st->stamina = 60;
    st->pos[0] = x;
    st->pos[1] = 2.0f;
    st->pos[2] = 3.0f;
    st->yaw = 45.0f;
}

typedef struct TestNet {
    CoopRelay host;
    CoopRelay a;
    CoopRelay b;
    Recorder ra;
    Recorder rb;
    unsigned short host_port;
} TestNet;

// Starts a host and two clients with recorders attached. Returns 0 on success.
static int net_start(TestNet* t)
{
    zero_bytes(t, sizeof(*t));
    coop_relay_init(&t->host, 1);
    coop_relay_init(&t->a, 0);
    coop_relay_init(&t->b, 0);
    recorder_init(&t->ra);
    recorder_init(&t->rb);

    if (coop_relay_host_start(&t->host, 0) != 0) return -1;
    if (coop_net_local_port(t->host.sock, &t->host_port) != 0) return -1;
    if (coop_relay_client_start(&t->a, LOOPBACK_IP, t->host_port) != 0) return -1;
    if (coop_relay_client_start(&t->b, LOOPBACK_IP, t->host_port) != 0) return -1;

    coop_relay_set_sink(&t->a, recorder_sink, &t->ra);
    coop_relay_set_sink(&t->b, recorder_sink, &t->rb);
    return 0;
}

static void net_stop(TestNet* t)
{
    coop_relay_stop(&t->a);
    coop_relay_stop(&t->b);
    coop_relay_stop(&t->host);
}

// Pumps `count` times so a datagram already queued gets processed.
static void pump_host(TestNet* t, int count, unsigned int now_ms)
{
    for (int i = 0; i < count; i++)
        coop_relay_pump(&t->host, now_ms + (unsigned int)i);
}

static void pump_client(CoopRelay* c, int count, unsigned int now_ms)
{
    for (int i = 0; i < count; i++)
        coop_relay_pump(c, now_ms + (unsigned int)i);
}

// ================= 1. join + relay + late-join snapshot =================

static void test_join_relay_snapshot(void)
{
    static TestNet t;
    CHECK(net_start(&t) == 0);
    if (t.host.started != 1 || t.a.started != 1 || t.b.started != 1) {
        net_stop(&t);
        return;
    }

    // A joins, then sends its state.
    CHECK(coop_relay_join(&t.a) == 0);
    pump_host(&t, 3, 1000);
    CHECK(coop_relay_player_count(&t.host) == 1);

    CoopPlayerState a_state;
    fill_state(&a_state, COOP_FLAG_CROUCH, 11.5f);
    CHECK(coop_relay_send_state(&t.a, &a_state) == 0);
    pump_host(&t, 3, 1010);
    CHECK(t.host.session.players[0].has_state == 1);
    CHECK(t.host.session.players[0].state.pos[0] == 11.5f);

    // B joins later: the host must send A's last state as a snapshot.
    CHECK(coop_relay_join(&t.b) == 0);
    pump_host(&t, 3, 1020);
    CHECK(coop_relay_player_count(&t.host) == 2);
    CHECK(t.host.stats.tx_snapshot == 1);

    pump_client(&t.b, 3, 1030);
    CHECK(t.rb.count == 1);
    if (t.rb.count == 1) {
        const CoopPlayerState* snap = rec_state(&t.rb, 0);
        CHECK(t.rb.len[0] == (int)sizeof(CoopPlayerState));
        CHECK(snap->hdr.type == COOP_PKT_PLAYER_STATE);
        CHECK(snap->slot == 0);
        CHECK(snap->flags == COOP_FLAG_CROUCH);
        CHECK(snap->pos[0] == 11.5f);
    }

    // B broadcasts its state: the host relays it to A.
    CoopPlayerState b_state;
    fill_state(&b_state, COOP_FLAG_SPRINT, -4.25f);
    CHECK(coop_relay_send_state(&t.b, &b_state) == 0);
    pump_host(&t, 3, 1040);
    CHECK(t.host.stats.tx_relay >= 1);

    pump_client(&t.a, 3, 1050);
    CHECK(t.ra.count == 1);
    if (t.ra.count == 1) {
        const CoopPlayerState* got = rec_state(&t.ra, 0);
        CHECK(t.ra.len[0] == (int)sizeof(CoopPlayerState));
        CHECK(got->slot == 1); // B's host-assigned slot
        CHECK(got->flags == COOP_FLAG_SPRINT);
        CHECK(got->pos[0] == -4.25f);
    }

    net_stop(&t);
}

// ================= 2. host owns slot assignment =================

static void test_host_assigns_slots(void)
{
    static TestNet t;
    CHECK(net_start(&t) == 0);
    if (t.host.started != 1 || t.a.started != 1 || t.b.started != 1) {
        net_stop(&t);
        return;
    }

    CHECK(coop_relay_join(&t.a) == 0);
    CHECK(coop_relay_join(&t.b) == 0);
    pump_host(&t, 3, 2000);

    // A lies about its slot; the host must rewrite it to A's real slot (0).
    CoopPlayerState st;
    fill_state(&st, COOP_FLAG_SWIM, 7.75f);
    st.slot = 7;
    CHECK(coop_relay_send_state(&t.a, &st) == 0);
    pump_host(&t, 3, 2010);

    CHECK(t.host.session.players[0].state.slot == 0);
    CHECK(t.host.session.players[0].state.pos[0] == 7.75f);

    pump_client(&t.b, 3, 2020);
    CHECK(t.rb.count == 1);
    if (t.rb.count == 1) {
        CHECK(rec_state(&t.rb, 0)->slot == 0);
        CHECK(rec_state(&t.rb, 0)->pos[0] == 7.75f);
    }

    net_stop(&t);
}

// ================= 3. unknown sender ignored =================

static void test_unknown_sender_ignored(void)
{
    static TestNet t;
    CHECK(net_start(&t) == 0);
    if (t.host.started != 1 || t.a.started != 1 || t.b.started != 1) {
        net_stop(&t);
        return;
    }

    // A sends state without ever joining.
    CoopPlayerState st;
    fill_state(&st, COOP_FLAG_CROUCH, 1.0f);
    CHECK(coop_relay_send_state(&t.a, &st) == 0);
    pump_host(&t, 3, 3000);

    CHECK(coop_relay_player_count(&t.host) == 0);
    CHECK(t.host.stats.rx_unknown == 1);
    CHECK(t.host.stats.rx_packets == 1);

    net_stop(&t);
}

// ================= 4. malformed packets rejected =================

static void test_malformed_packets(void)
{
    static TestNet t;
    CHECK(net_start(&t) == 0);
    if (t.host.started != 1) {
        net_stop(&t);
        return;
    }

    unsigned long long raw = coop_net_open(0);
    CHECK(raw != COOP_NET_INVALID_SOCKET);
    if (raw == COOP_NET_INVALID_SOCKET) {
        net_stop(&t);
        return;
    }

    CoopNetAddress to;
    to.ip = LOOPBACK_IP;
    to.port = t.host_port;

    CoopPlayerState st;

    // Wrong magic.
    fill_state(&st, 0, 0.0f);
    st.hdr.magic = 0xBEEF;
    coop_net_send(raw, &st, (int)sizeof(st), &to);

    // Wrong protocol version.
    fill_state(&st, 0, 0.0f);
    st.hdr.version = COOP_PROTO_VERSION + 1;
    coop_net_send(raw, &st, (int)sizeof(st), &to);

    // Unknown packet type.
    fill_state(&st, 0, 0.0f);
    st.hdr.type = 0x7F;
    coop_net_send(raw, &st, (int)sizeof(st), &to);

    // Truncated: header only, but declares PLAYER_STATE.
    fill_state(&st, 0, 0.0f);
    coop_net_send(raw, &st, (int)sizeof(CoopPacketHeader), &to);

    pump_host(&t, 6, 4000);

    CHECK(t.host.stats.rx_invalid == 4);
    CHECK(t.host.stats.rx_packets == 4);
    CHECK(coop_relay_player_count(&t.host) == 0);

    coop_net_close(raw);
    net_stop(&t);
}

// ================= 5. silent players expire =================

static void test_expiry(void)
{
    static TestNet t;
    CHECK(net_start(&t) == 0);
    if (t.host.started != 1 || t.a.started != 1) {
        net_stop(&t);
        return;
    }

    CHECK(coop_relay_join(&t.a) == 0);
    pump_host(&t, 3, 5000);
    CHECK(coop_relay_player_count(&t.host) == 1);

    // Still inside the timeout window (one pump, so the clock does not advance
    // past the boundary).
    coop_relay_pump(&t.host, 5000 + COOP_RELAY_TIMEOUT_MS - 1);
    CHECK(coop_relay_player_count(&t.host) == 1);

    // Past the window: the silent player is dropped.
    coop_relay_pump(&t.host, 5000 + COOP_RELAY_TIMEOUT_MS);
    CHECK(coop_relay_player_count(&t.host) == 0);
    CHECK(t.host.stats.expired == 1);

    net_stop(&t);
}

void test_entry(void)
{
    CHECK(coop_net_init() == 0);

    out_str("running: join + relay + late-join snapshot...\n");
    test_join_relay_snapshot();

    out_str("running: host assigns slots...\n");
    test_host_assigns_slots();

    out_str("running: unknown sender ignored...\n");
    test_unknown_sender_ignored();

    out_str("running: malformed packets rejected...\n");
    test_malformed_packets();

    out_str("running: silent player expiry...\n");
    test_expiry();

    coop_net_shutdown();

    if (g_failures == 0) {
        out_str("PASS: outlast-2 coop relay tests (5 groups)\n");
        ExitProcess(0);
    }
    out_str("FAILED: ");
    out_uint((unsigned long)g_failures);
    out_str(" check(s)\n");
    ExitProcess(1);
}
