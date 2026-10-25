// Offline tests for the Outlast 2 co-op sync protocol (no game needed).
// Verifies packed wire layout, field offsets, round-trip serialization and
// flag-bit invariants from games/outlast-2/src/coop_protocol.h.
//
// Freestanding (no CRT): built with /NODEFAULTLIB like the proxy DLL, because
// this VS environment has no SDK/CRT headers.
//
// Build + run: games/outlast-2/tests/run.bat

#include "win_min.h"
#include "coop_protocol.h"

// MSVC emits a reference to _fltused when floating point is used; the CRT
// normally provides it. Define it here for the freestanding (no CRT) build.
int _fltused = 0;

static int g_failures = 0;

#define OFFSET_OF(type, member) ((unsigned long long)&(((type*)0)->member))

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
    if (h && h != INVALID_HANDLE_VALUE) {
        WriteFile(h, s, (DWORD)str_len(s), &written, NULL);
    }
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

#define CHECK(cond)                     \
    do {                                \
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

static int bytes_equal(const void* a, const void* b, unsigned long long n)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--) {
        if (*pa++ != *pb++) return 0;
    }
    return 1;
}

static void test_constants(void)
{
    CHECK(COOP_MAGIC == 0x4F4C);
    CHECK(COOP_PROTO_VERSION == 1);
    CHECK(COOP_MAX_PLAYERS == 8);
    CHECK(COOP_TICK_RATE >= 20 && COOP_TICK_RATE <= 30);
}

static void test_wire_layout(void)
{
    CHECK(sizeof(CoopPacketHeader) == 8);
    CHECK(sizeof(CoopPlayerState) == 44);
    CHECK(sizeof(CoopWorldEvent) == 16);
    CHECK(sizeof(CoopLevelBarrier) == 12);
    CHECK(sizeof(CoopEnemyState) == 12);

    CHECK(OFFSET_OF(CoopPacketHeader, magic) == 0);
    CHECK(OFFSET_OF(CoopPacketHeader, version) == 2);
    CHECK(OFFSET_OF(CoopPacketHeader, type) == 3);
    CHECK(OFFSET_OF(CoopPacketHeader, seq) == 4);

    CHECK(OFFSET_OF(CoopPlayerState, slot) == 8);
    CHECK(OFFSET_OF(CoopPlayerState, flags) == 9);
    CHECK(OFFSET_OF(CoopPlayerState, misc) == 10);
    CHECK(OFFSET_OF(CoopPlayerState, stamina) == 11);
    CHECK(OFFSET_OF(CoopPlayerState, pos) == 12);
    CHECK(OFFSET_OF(CoopPlayerState, vel) == 24);
    CHECK(OFFSET_OF(CoopPlayerState, yaw) == 36);
    CHECK(OFFSET_OF(CoopPlayerState, pitch) == 40);

    CHECK(OFFSET_OF(CoopWorldEvent, hdr) == 0);
    CHECK(OFFSET_OF(CoopLevelBarrier, hdr) == 0);
    CHECK(OFFSET_OF(CoopEnemyState, hdr) == 0);
}

static void test_roundtrip(void)
{
    CoopPlayerState out;
    zero_bytes(&out, sizeof out);
    out.hdr.magic = COOP_MAGIC;
    out.hdr.version = COOP_PROTO_VERSION;
    out.hdr.type = COOP_PKT_PLAYER_STATE;
    out.hdr.seq = 12345;
    out.slot = 3;
    out.flags = COOP_FLAG_CROUCH | COOP_FLAG_SLIDE;
    out.misc = COOP_MISC_NIGHTVISION;
    out.stamina = 77;
    out.pos[0] = 1.5f;
    out.pos[1] = -2.25f;
    out.pos[2] = 300.0f;
    out.vel[0] = 0.5f;
    out.vel[1] = 0.0f;
    out.vel[2] = -0.125f;
    out.yaw = 90.0f;
    out.pitch = -12.5f;

    unsigned char wire[sizeof(CoopPlayerState)];
    copy_bytes(wire, &out, sizeof out);

    CoopPlayerState in;
    zero_bytes(&in, sizeof in);
    copy_bytes(&in, wire, sizeof in);

    CHECK(bytes_equal(&out, &in, sizeof out));
    CHECK(in.hdr.magic == COOP_MAGIC);
    CHECK(in.hdr.type == COOP_PKT_PLAYER_STATE);
    CHECK(in.hdr.seq == 12345);
    CHECK(in.slot == 3);
    CHECK(in.flags == (COOP_FLAG_CROUCH | COOP_FLAG_SLIDE));
    CHECK(in.pos[0] == 1.5f);
    CHECK(in.pos[1] == -2.25f);
    CHECK(in.vel[2] == -0.125f);
    CHECK(in.yaw == 90.0f);
    CHECK(in.pitch == -12.5f);

    CoopWorldEvent we;
    zero_bytes(&we, sizeof we);
    we.hdr.magic = COOP_MAGIC;
    we.hdr.type = COOP_PKT_WORLD_EVENT;
    we.kind = 2;
    we.actorId = 11;
    we.extra = 45.0f;
    unsigned char wwire[sizeof(CoopWorldEvent)];
    copy_bytes(wwire, &we, sizeof we);
    CoopWorldEvent win;
    zero_bytes(&win, sizeof win);
    copy_bytes(&win, wwire, sizeof win);
    CHECK(bytes_equal(&we, &win, sizeof we));
}

static void test_flag_bits(void)
{
    unsigned char flags[8];
    flags[0] = COOP_FLAG_CROUCH;
    flags[1] = COOP_FLAG_PRONE;
    flags[2] = COOP_FLAG_SLIDE;
    flags[3] = COOP_FLAG_SWIM;
    flags[4] = COOP_FLAG_DIVE;
    flags[5] = COOP_FLAG_SPRINT;
    flags[6] = COOP_FLAG_DEAD;
    flags[7] = COOP_FLAG_CAMCORDER;

    for (unsigned i = 0; i < 8; i++) {
        CHECK(flags[i] != 0);
        CHECK((flags[i] & (flags[i] - 1)) == 0);
        for (unsigned j = i + 1; j < 8; j++) {
            CHECK((flags[i] & flags[j]) == 0);
        }
    }

    unsigned char misc[5];
    misc[0] = COOP_MISC_NIGHTVISION;
    misc[1] = COOP_MISC_MIC;
    misc[2] = COOP_MISC_BLEEDING;
    misc[3] = COOP_MISC_BANDAGE;
    misc[4] = COOP_MISC_INTERACT;

    for (unsigned i = 0; i < 5; i++) {
        CHECK(misc[i] != 0);
        CHECK((misc[i] & (misc[i] - 1)) == 0);
    }
}

static void test_packet_types_distinct(void)
{
    unsigned char types[6];
    types[0] = COOP_PKT_PLAYER_STATE;
    types[1] = COOP_PKT_WORLD_EVENT;
    types[2] = COOP_PKT_LEVEL_BARRIER;
    types[3] = COOP_PKT_ENEMY_STATE;
    types[4] = COOP_PKT_JOIN;
    types[5] = COOP_PKT_HEARTBEAT;

    for (unsigned i = 0; i < 6; i++) {
        for (unsigned j = i + 1; j < 6; j++) {
            CHECK(types[i] != types[j]);
        }
    }
}

void test_entry(void)
{
    test_constants();
    test_wire_layout();
    test_roundtrip();
    test_flag_bits();
    test_packet_types_distinct();

    if (g_failures == 0) {
        out_str("PASS: outlast-2 coop protocol tests (5 groups)\n");
        ExitProcess(0);
    }
    out_str("FAILED: ");
    out_uint((unsigned long)g_failures);
    out_str(" check(s)\n");
    ExitProcess(1);
}
