// Offline tests for the remote-puppet interpolation layer (no game needed).
// Covers: basic lerp + playback delay, stale/reordered/duplicate rejection,
// loss gaps, extrapolation limits, teleport snapping, shortest-arc angles,
// and a scripted lossy/reordered fake network run.
//
// Freestanding (no CRT): built with /NODEFAULTLIB like the proxy DLL.
// Build + run: games/outlast-2/tests/run.bat

#include "win_min.h"
#include "coop_protocol.h"
#include "coop_interp.h"

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

static void zero_bytes(void* dst, unsigned long long n)
{
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = 0;
}

static float abs_f(float v)
{
    return v < 0.0f ? -v : v;
}

static int near_f(float a, float b, float eps)
{
    return abs_f(a - b) <= eps;
}

// Builds a state for `slot`/`seq` with position x and optional velocity/flags.
static CoopPlayerState make_state(unsigned char slot, unsigned int seq, float x,
                                  float vx, unsigned char flags, float yaw, float pitch)
{
    CoopPlayerState st;
    zero_bytes(&st, sizeof(st));
    st.hdr.magic = COOP_MAGIC;
    st.hdr.version = COOP_PROTO_VERSION;
    st.hdr.type = COOP_PKT_PLAYER_STATE;
    st.hdr.seq = seq;
    st.slot = slot;
    st.flags = flags;
    st.stamina = 50;
    st.pos[0] = x;
    st.pos[1] = 0.0f;
    st.pos[2] = 0.0f;
    st.vel[0] = vx;
    st.vel[1] = 0.0f;
    st.vel[2] = 0.0f;
    st.yaw = yaw;
    st.pitch = pitch;
    return st;
}

// ================= 1. basic lerp + playback delay =================

static void test_lerp_and_delay(void)
{
    static CoopInterp it;
    coop_interp_init(&it, COOP_INTERP_DELAY_MS);

    CoopPlayerState a = make_state(0, 1, 0.0f, 0.0f, COOP_FLAG_CROUCH, 0.0f, 0.0f);
    CoopPlayerState b = make_state(0, 2, 100.0f, 0.0f, COOP_FLAG_SPRINT, 0.0f, 0.0f);
    CHECK(coop_interp_push(&it, &a, 0) == 1);
    CHECK(coop_interp_push(&it, &b, 100) == 1);

    CoopPlayerState out;

    // now=50 -> playback time 0 (delay 100): hold the oldest snapshot.
    CHECK(coop_interp_sample(&it, 0, 50, &out) == 1);
    CHECK(near_f(out.pos[0], 0.0f, 0.001f));

    // now=150 -> playback 50: halfway between the two snapshots.
    CHECK(coop_interp_sample(&it, 0, 150, &out) == 1);
    CHECK(near_f(out.pos[0], 50.0f, 0.01f));

    // Discrete state comes from the older snapshot (no mid-blend flicker).
    CHECK(out.flags == COOP_FLAG_CROUCH);
    CHECK(out.stamina == 50);

    // Unknown slot has no output.
    CHECK(coop_interp_sample(&it, 5, 150, &out) == 0);
}

// ================= 2. stale / reordered / duplicate =================

static void test_reorder_reject(void)
{
    static CoopInterp it;
    coop_interp_init(&it, 0);

    CoopPlayerState s1 = make_state(0, 1, 10.0f, 0.0f, 0, 0.0f, 0.0f);
    CoopPlayerState s3 = make_state(0, 3, 30.0f, 0.0f, 0, 0.0f, 0.0f);
    CoopPlayerState s2 = make_state(0, 2, 20.0f, 0.0f, 0, 0.0f, 0.0f);

    CHECK(coop_interp_push(&it, &s1, 0) == 1);
    CHECK(coop_interp_push(&it, &s3, 80) == 1);
    CHECK(coop_interp_push(&it, &s2, 40) == 0);  // late older packet
    CHECK(coop_interp_push(&it, &s3, 80) == 0);  // duplicate
    CHECK(it.players[0].count == 2);
    CHECK(it.players[0].last_seq == 3);

    // A late packet never rewinds the puppet.
    CoopPlayerState out;
    CHECK(coop_interp_sample(&it, 0, 80, &out) == 1);
    CHECK(out.pos[0] >= 10.0f);
}

// ================= 3. loss gaps + extrapolation limit =================

static void test_gap_and_extrapolation(void)
{
    static CoopInterp it;
    coop_interp_init(&it, 0);

    // A 300 ms gap between packets 2 and 3 is filled by interpolation.
    CoopPlayerState s1 = make_state(0, 1, 0.0f, 0.0f, 0, 0.0f, 0.0f);
    CoopPlayerState s2 = make_state(0, 2, 100.0f, 0.0f, 0, 0.0f, 0.0f);
    CoopPlayerState s3 = make_state(0, 3, 400.0f, 0.0f, 0, 0.0f, 0.0f);
    CHECK(coop_interp_push(&it, &s1, 0) == 1);
    CHECK(coop_interp_push(&it, &s2, 100) == 1);
    CHECK(coop_interp_push(&it, &s3, 400) == 1);

    CoopPlayerState out;
    CHECK(coop_interp_sample(&it, 0, 250, &out) == 1);
    CHECK(near_f(out.pos[0], 250.0f, 0.01f));

    // After the newest packet: extrapolate on velocity, capped at 150 ms.
    static CoopInterp ex;
    coop_interp_init(&ex, 0);
    CoopPlayerState m = make_state(0, 1, 0.0f, 10.0f, 0, 0.0f, 0.0f); // 10 units/s
    CHECK(coop_interp_push(&ex, &m, 0) == 1);

    CHECK(coop_interp_sample(&ex, 0, 200, &out) == 1);
    CHECK(near_f(out.pos[0], 1.5f, 0.001f)); // 10 * 0.15 s, not 10 * 0.2 s

    CHECK(coop_interp_sample(&ex, 0, 5000, &out) == 1);
    CHECK(near_f(out.pos[0], 1.5f, 0.001f)); // holds once the cap is reached
}

// ================= 4. teleport snapping =================

static void test_teleport_snap(void)
{
    static CoopInterp it;
    coop_interp_init(&it, 0);

    CoopPlayerState a = make_state(0, 1, 0.0f, 0.0f, 0, 0.0f, 0.0f);
    CoopPlayerState b = make_state(0, 2, 5000.0f, 0.0f, 0, 0.0f, 0.0f); // > 800 units
    CHECK(coop_interp_push(&it, &a, 0) == 1);
    CHECK(coop_interp_push(&it, &b, 100) == 1);

    CoopPlayerState out;
    CHECK(coop_interp_sample(&it, 0, 50, &out) == 1);
    // Mid-blend would be 2500: the layer must snap instead of sliding.
    CHECK(near_f(out.pos[0], 5000.0f, 0.001f));
}

// ================= 5. shortest-arc angles =================

static void test_angle_wrap(void)
{
    static CoopInterp it;
    coop_interp_init(&it, 0);

    CoopPlayerState a = make_state(0, 1, 0.0f, 0.0f, 0, 350.0f, -170.0f);
    CoopPlayerState b = make_state(0, 2, 0.0f, 0.0f, 0, 10.0f, 170.0f);
    CHECK(coop_interp_push(&it, &a, 0) == 1);
    CHECK(coop_interp_push(&it, &b, 100) == 1);

    CoopPlayerState out;
    CHECK(coop_interp_sample(&it, 0, 50, &out) == 1);

    // 350 -> 10 must pass through 0, not sweep backwards through 180.
    CHECK(near_f(out.yaw, 0.0f, 0.01f) || near_f(abs_f(out.yaw), 360.0f, 0.01f));

    // -170 -> 170 must pass through +/-180.
    CHECK(near_f(abs_f(out.pitch), 180.0f, 0.01f));
}

// ================= 6. scripted lossy / reordered fake network =================

static void test_fake_network(void)
{
    static CoopInterp it;
    coop_interp_init(&it, COOP_INTERP_DELAY_MS);

    // 25 Hz sender for 2 s: 50 packets, x advances 100 units per packet.
    // Every 3rd packet is lost, every 7th duplicated, adjacent pairs are
    // swapped now and then, and arrival jitters by up to 10 ms.
    CoopPlayerState st[64];
    unsigned int send_ms[64];
    int n = 0;
    for (int tick = 0; tick < 50; tick++) {
        if (tick % 3 == 2) continue; // loss
        st[n] = make_state(0, (unsigned int)(tick + 1), (float)tick * 100.0f,
                           2500.0f, COOP_FLAG_SPRINT, 0.0f, 0.0f);
        send_ms[n] = (unsigned int)tick * 40; // 25 Hz stream time
        n++;
    }

    unsigned int recv_base = 0;
    for (int i = 0; i < n; i++) {
        int j = i;
        if (i + 1 < n && i % 5 == 3) j = i + 1;       // reorder pair
        else if (i > 0 && (i - 1) % 5 == 3) j = i - 1; // its partner
        if (j < 0 || j >= n) j = i;

        unsigned int jitter = (unsigned int)((j * 7) % 11); // 0..10 ms
        unsigned int recv_ms = recv_base + send_ms[j] + jitter;
        coop_interp_push(&it, &st[j], recv_ms);
    }
    // Duplicates of the last packet must be rejected.
    CHECK(coop_interp_push(&it, &st[n - 1], recv_base + 9999) == 0);

    // Sample at 25 Hz like the render loop would, after the buffer fills.
    CoopPlayerState out;
    float prev_x = -1.0f;
    int samples = 0;
    for (unsigned int now = 200; now <= 2300; now += 40) {
        if (!coop_interp_sample(&it, 0, now, &out)) continue;
        samples++;

        // Never NaN, never rewinds, never jumps absurdly far per frame.
        CHECK(out.pos[0] == out.pos[0]);
        if (prev_x >= 0.0f) {
            CHECK(out.pos[0] >= prev_x - 0.001f);
            CHECK(out.pos[0] - prev_x <= 160.0f);
        }
        prev_x = out.pos[0];
    }
    CHECK(samples > 40);
    CHECK(prev_x > 4000.0f); // tracked the sender to the end of the run

    // Buffer is bounded regardless of how many packets arrived.
    CHECK(it.players[0].count <= COOP_INTERP_BUFFER);

    // Removing a player stops its output.
    coop_interp_remove(&it, 0);
    CHECK(coop_interp_sample(&it, 0, 2300, &out) == 0);
    CHECK(coop_interp_active_count(&it) == 0);
}

void test_entry(void)
{
    out_str("running: lerp + playback delay...\n");
    test_lerp_and_delay();

    out_str("running: stale/reordered/duplicate rejection...\n");
    test_reorder_reject();

    out_str("running: loss gaps + extrapolation limit...\n");
    test_gap_and_extrapolation();

    out_str("running: teleport snapping...\n");
    test_teleport_snap();

    out_str("running: shortest-arc angles...\n");
    test_angle_wrap();

    out_str("running: scripted lossy/reordered network...\n");
    test_fake_network();

    if (g_failures == 0) {
        out_str("PASS: outlast-2 coop interpolation tests (6 groups)\n");
        ExitProcess(0);
    }
    out_str("FAILED: ");
    out_uint((unsigned long)g_failures);
    out_str(" check(s)\n");
    ExitProcess(1);
}
