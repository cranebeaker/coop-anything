// Loopback test for the co-op UDP transport (no game needed).
// Proves packet flow over UDP loopback: self send/receive, two-socket
// client->host delivery with source address, and receive timeout.
//
// Freestanding (no CRT): built with /NODEFAULTLIB like the proxy DLL.
// Build + run: games/outlast-2/tests/run.bat

#include "win_min.h"
#include "coop_protocol.h"
#include "coop_net.h"

int _fltused = 0;

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

static int bytes_equal(const void* a, const void* b, unsigned long long n)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--) {
        if (*pa++ != *pb++) return 0;
    }
    return 1;
}

#define LOOPBACK_IP 0x0100007Fu // 127.0.0.1, network order

static void fill_state(CoopPlayerState* s, unsigned int seq)
{
    s->hdr.magic = COOP_MAGIC;
    s->hdr.version = COOP_PROTO_VERSION;
    s->hdr.type = COOP_PKT_PLAYER_STATE;
    s->hdr.seq = seq;
    s->slot = 2;
    s->flags = COOP_FLAG_CROUCH;
    s->misc = COOP_MISC_MIC;
    s->stamina = 55;
    s->pos[0] = 3.5f;
    s->pos[1] = 4.25f;
    s->pos[2] = -7.0f;
    s->vel[0] = 0.0f;
    s->vel[1] = 0.0f;
    s->vel[2] = 1.75f;
    s->yaw = 180.0f;
    s->pitch = 5.0f;
}

static void test_self_roundtrip(void)
{
    unsigned long long sock = coop_net_open(0);
    CHECK(sock != COOP_NET_INVALID_SOCKET);
    if (sock == COOP_NET_INVALID_SOCKET) return;

    unsigned short port = 0;
    CHECK(coop_net_local_port(sock, &port) == 0);
    CHECK(port != 0);

    CoopNetAddress to;
    to.ip = LOOPBACK_IP;
    to.port = port;

    CoopPlayerState pkt;
    fill_state(&pkt, 1);

    int sent = coop_net_send(sock, &pkt, (int)sizeof(pkt), &to);
    CHECK(sent == (int)sizeof(pkt));

    unsigned char buf[128];
    CoopNetAddress from;
    int n = coop_net_recv(sock, buf, (int)sizeof(buf), &from, 2000);
    CHECK(n == (int)sizeof(pkt));
    CHECK(bytes_equal(buf, &pkt, sizeof(pkt)));
    CHECK(from.port == port);
    CHECK(from.ip == LOOPBACK_IP);

    coop_net_close(sock);
}

static void test_two_socket_delivery(void)
{
    unsigned long long rx = coop_net_open(0);
    unsigned long long tx = coop_net_open(0);
    CHECK(rx != COOP_NET_INVALID_SOCKET);
    CHECK(tx != COOP_NET_INVALID_SOCKET);
    if (rx == COOP_NET_INVALID_SOCKET || tx == COOP_NET_INVALID_SOCKET) {
        coop_net_close(rx);
        coop_net_close(tx);
        return;
    }

    unsigned short rx_port = 0, tx_port = 0;
    CHECK(coop_net_local_port(rx, &rx_port) == 0);
    CHECK(coop_net_local_port(tx, &tx_port) == 0);
    CHECK(rx_port != 0 && tx_port != 0);
    CHECK(rx_port != tx_port);

    CoopNetAddress to;
    to.ip = LOOPBACK_IP;
    to.port = rx_port;

    CoopPlayerState pkt;
    fill_state(&pkt, 4242);
    int sent = coop_net_send(tx, &pkt, (int)sizeof(pkt), &to);
    CHECK(sent == (int)sizeof(pkt));

    unsigned char buf[128];
    CoopNetAddress from;
    int n = coop_net_recv(rx, buf, (int)sizeof(buf), &from, 2000);
    CHECK(n == (int)sizeof(pkt));
    CHECK(bytes_equal(buf, &pkt, sizeof(pkt)));
    CHECK(from.port == tx_port); // source is the sender's bound port
    CHECK(from.ip == LOOPBACK_IP);

    coop_net_close(rx);
    coop_net_close(tx);
}

static void test_recv_timeout(void)
{
    unsigned long long sock = coop_net_open(0);
    CHECK(sock != COOP_NET_INVALID_SOCKET);
    if (sock == COOP_NET_INVALID_SOCKET) return;

    unsigned char buf[64];
    CoopNetAddress from;
    int n = coop_net_recv(sock, buf, (int)sizeof(buf), &from, 150);
    CHECK(n < 0); // nothing was sent: timeout expected

    coop_net_close(sock);
}

void test_entry(void)
{
    out_str("running: coop_net_init...\n");
    CHECK(coop_net_init() == 0);

    out_str("running: self roundtrip...\n");
    test_self_roundtrip();

    out_str("running: two-socket delivery...\n");
    test_two_socket_delivery();

    out_str("running: recv timeout...\n");
    test_recv_timeout();

    coop_net_shutdown();

    if (g_failures == 0) {
        out_str("PASS: outlast-2 coop UDP loopback tests (3 groups)\n");
        ExitProcess(0);
    }
    out_str("FAILED: ");
    out_uint((unsigned long)g_failures);
    out_str(" check(s)\n");
    ExitProcess(1);
}
