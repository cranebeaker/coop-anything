// Unit tests for the co-op relay session layer (no game, no sockets).
// Covers: join/find/duplicate join, capacity, slot reuse, touch+expire,
// relay fan-out targets, and late-join snapshot.
//
// Freestanding (no CRT): built with /NODEFAULTLIB like the proxy DLL.
// Build + run: games/outlast-2/tests/run.bat

#include "win_min.h"
#include "coop_protocol.h"
#include "coop_net.h"
#include "coop_session.h"

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

static CoopNetAddress addr(unsigned int ip, unsigned short port)
{
    CoopNetAddress a;
    a.ip = ip;
    a.port = port;
    return a;
}

static void test_join_find(void)
{
    CoopSession s;
    coop_session_init(&s, 1);
    CHECK(coop_session_count(&s) == 0);

    CoopNetAddress a = addr(0x0100007Fu, 5001);
    CoopNetAddress b = addr(0x0100007Fu, 5002);

    int slot_a = coop_session_add(&s, &a);
    int slot_b = coop_session_add(&s, &b);
    CHECK(slot_a == 0);
    CHECK(slot_b == 1);
    CHECK(coop_session_count(&s) == 2);

    // Duplicate join returns the existing slot.
    CHECK(coop_session_add(&s, &a) == slot_a);

    // Find by address.
    CHECK(coop_session_find(&s, &a) == slot_a);
    CHECK(coop_session_find(&s, &b) == slot_b);
    CoopNetAddress unknown = addr(0x0100007Fu, 5999);
    CHECK(coop_session_find(&s, &unknown) == -1);
}

static void test_capacity_and_reuse(void)
{
    CoopSession s;
    coop_session_init(&s, 1);

    for (int i = 0; i < COOP_SESSION_MAX; i++) {
        CoopNetAddress a = addr(0x0100007Fu, (unsigned short)(6000 + i));
        CHECK(coop_session_add(&s, &a) == i);
    }
    CHECK(coop_session_count(&s) == COOP_SESSION_MAX);

    CoopNetAddress overflow = addr(0x0100007Fu, 7000);
    CHECK(coop_session_add(&s, &overflow) == -1);

    // Free slot 0, then a new player reuses it.
    CHECK(coop_session_remove(&s, 0) == 0);
    CHECK(coop_session_count(&s) == COOP_SESSION_MAX - 1);
    CHECK(coop_session_add(&s, &overflow) == 0);

    // Removing an empty slot fails.
    CHECK(coop_session_remove(&s, COOP_SESSION_MAX) == -1);
    CHECK(coop_session_remove(&s, -1) == -1);
}

static void test_touch_expire(void)
{
    CoopSession s;
    coop_session_init(&s, 1);
    CoopNetAddress a = addr(0x0100007Fu, 8001);
    int slot = coop_session_add(&s, &a);
    CHECK(slot >= 0);

    coop_session_touch(&s, slot, 100);
    CHECK(coop_session_expire(&s, 120, 50) == 0); // 20 ms since touch
    CHECK(coop_session_expire(&s, 160, 50) == 1); // 60 ms since touch
    CHECK(coop_session_count(&s) == 0);
}

static void test_relay_targets(void)
{
    CoopSession s;
    coop_session_init(&s, 1);
    CoopNetAddress a = addr(0x0100007Fu, 9001);
    CoopNetAddress b = addr(0x0100007Fu, 9002);
    CoopNetAddress c = addr(0x0100007Fu, 9003);
    coop_session_add(&s, &a);
    coop_session_add(&s, &b);
    coop_session_add(&s, &c);

    CoopNetAddress out[COOP_SESSION_MAX];
    int n = coop_session_relay_targets(&s, 0, out, COOP_SESSION_MAX);
    CHECK(n == 2);
    CHECK(out[0].port == 9002);
    CHECK(out[1].port == 9003);
    for (int i = 0; i < n; i++)
        CHECK(out[i].port != 9001); // never relay back to the sender
}

static void test_snapshot(void)
{
    CoopSession s;
    coop_session_init(&s, 1);
    CoopNetAddress a = addr(0x0100007Fu, 9101);
    CoopNetAddress b = addr(0x0100007Fu, 9102);
    CoopNetAddress c = addr(0x0100007Fu, 9103);
    coop_session_add(&s, &a);
    coop_session_add(&s, &b);
    coop_session_add(&s, &c);

    CoopPlayerState st;
    st.hdr.magic = COOP_MAGIC;
    st.hdr.version = COOP_PROTO_VERSION;
    st.hdr.type = COOP_PKT_PLAYER_STATE;

    st.hdr.seq = 11;
    CHECK(coop_session_update_state(&s, 0, &st) == 0);
    st.hdr.seq = 22;
    CHECK(coop_session_update_state(&s, 1, &st) == 0);
    // slot 2 has joined but sent no state yet

    CoopPlayerState out[COOP_SESSION_MAX];
    int n = coop_session_snapshot(&s, 0, out, COOP_SESSION_MAX);
    CHECK(n == 1); // only slot 1 has state, slot 0 excluded
    CHECK(out[0].hdr.seq == 22);

    st.hdr.seq = 33;
    CHECK(coop_session_update_state(&s, 2, &st) == 0);
    n = coop_session_snapshot(&s, 1, out, COOP_SESSION_MAX);
    CHECK(n == 2); // slots 0 (11) and 2 (33), slot 1 excluded
    CHECK(out[0].hdr.seq == 11);
    CHECK(out[1].hdr.seq == 33);

    // Snapshot honours the output cap.
    n = coop_session_snapshot(&s, -1, out, 1);
    CHECK(n == 1);
}

void test_entry(void)
{
    out_str("running: join/find...\n");
    test_join_find();

    out_str("running: capacity/reuse...\n");
    test_capacity_and_reuse();

    out_str("running: touch/expire...\n");
    test_touch_expire();

    out_str("running: relay targets...\n");
    test_relay_targets();

    out_str("running: late-join snapshot...\n");
    test_snapshot();

    if (g_failures == 0) {
        out_str("PASS: outlast-2 coop session tests (5 groups)\n");
        ExitProcess(0);
    }
    out_str("FAILED: ");
    out_uint((unsigned long)g_failures);
    out_str(" check(s)\n");
    ExitProcess(1);
}
