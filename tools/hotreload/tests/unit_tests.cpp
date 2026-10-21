// Unit tests for the reload library: state registry, byte reader and writer, threads, drain, JSON helpers and the control
// pipe. No game, no loader DLL. The mutation twins that these tests guard are listed in src/mut.h.
//
//   hotreload_unit_tests              run every test (prints [PASS] / [FAIL] per test and a summary line)
//   hotreload_unit_tests --twin N     switch twin N on and run the test that guards it. Exit 0 when the test fails
//                                     (the twin is caught), 1 when the test still passes (the twin is missed).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "crc32.h"
#include "hotreload/control_pipe.h"
#include "hotreload/reload_drain.h"
#include "hotreload/reload_state.h"
#include "mut.h"

using namespace hotreload;

// ---- a tiny test framework ---------------------------------------------------------------------------------------
namespace {
int g_checks = 0, g_failedChecks = 0;
struct Test {
    const char* name;
    void (*fn)();
    int twin;
};
std::vector<Test>& tests() {
    static std::vector<Test> t;
    return t;
}
struct Reg {
    Reg(const char* n, void (*f)(), int twin) { tests().push_back({n, f, twin}); }
};
}  // namespace

#define CHECK(c)                                                                        \
    do {                                                                                \
        ++g_checks;                                                                     \
        if (!(c)) {                                                                     \
            ++g_failedChecks;                                                           \
            std::printf("    check failed (line %d): %s\n", __LINE__, #c);              \
        }                                                                               \
    } while (0)
#define TEST(name, twin)                  \
    static void name();                   \
    static Reg reg_##name(#name, name, twin); \
    static void name()

static bool contains(const std::string& s, const std::string& what) { return s.find(what) != std::string::npos; }

// ---- bytes -------------------------------------------------------------------------------------------------------
TEST(writer_reader_roundtrip, 0) {
    Writer w;
    w.u8(7);
    w.u16(0xBEEF);
    w.u32(0xDEADBEEFu);
    w.u64(0x0102030405060708ull);
    w.i64(-42);
    w.f64(3.25);
    w.boolean(true);
    w.str("hello");
    w.bytes(Bytes{1, 2, 3});
    Reader r(w.data());
    CHECK(r.u8() == 7);
    CHECK(r.u16() == 0xBEEF);
    CHECK(r.u32() == 0xDEADBEEFu);
    CHECK(r.u64() == 0x0102030405060708ull);
    CHECK(r.i64() == -42);
    CHECK(r.f64() == 3.25);
    CHECK(r.boolean());
    CHECK(r.str() == "hello");
    CHECK((r.bytes() == Bytes{1, 2, 3}));
    CHECK(r.ok() && r.atEnd());
}

TEST(reader_short_read_is_safe, 0) {
    const uint8_t three[3] = {1, 2, 3};
    Reader r(three, 3);
    CHECK(r.u32() == 0);
    CHECK(!r.ok());
    CHECK(r.u8() == 0);   // after a failure every get returns zero, even when bytes are left
    Writer w;
    w.u32(1000000);   // a string that claims to be a million bytes long
    Reader r2(w.data());
    CHECK(r2.str().empty());
    CHECK(!r2.ok());
}

TEST(crc32_known_value, 0) {
    CHECK(Crc32("123456789", 9) == 0xCBF43926u);
    uint32_t c = 0xFFFFFFFFu;
    c = Crc32Update(c, "12345", 5);
    c = Crc32Update(c, "6789", 4);
    CHECK((c ^ 0xFFFFFFFFu) == 0xCBF43926u);
}

// ---- registry ----------------------------------------------------------------------------------------------------
namespace {
struct Cell {   // one piece of state with a version
    const char* name;
    uint32_t version;
    int order;
    int value = 0;
    int restoreCalls = 0, migrateCalls = 0, freshCalls = 0, shutdownSeq = -1;
};

Participant partOf(Cell& c, bool withMigrate = false) {
    Participant p;
    p.name = c.name;
    p.version = c.version;
    p.order = c.order;
    p.save = [&c](Writer& w) { w.u32(static_cast<uint32_t>(c.value)); return true; };
    p.restore = [&c](uint32_t, Reader& r) { ++c.restoreCalls; c.value = static_cast<int>(r.u32()); return true; };
    if (withMigrate) p.migrate = [&c](uint32_t saved, Reader& r) { ++c.migrateCalls; c.value = static_cast<int>(r.u32()) + 1000 * static_cast<int>(saved); return true; };
    p.fresh = [&c] { ++c.freshCalls; c.value = -1; };
    return p;
}
}  // namespace

TEST(blob_roundtrip_restores_every_participant, 0) {
    Cell a{"a", 1, 10, 11}, b{"b", 1, 20, 22};
    Registry old;
    old.add(partOf(a));
    old.add(partOf(b));
    Bytes blob;
    Report rep;
    CHECK(old.saveAll(&blob, &rep));
    std::vector<std::pair<std::string, uint32_t>> entries;
    CHECK(Registry::inspect(blob, &entries));
    CHECK(entries.size() == 2 && entries[0].first == "a" && entries[1].first == "b" && entries[1].second == 1);
    a.value = b.value = 0;
    Registry fresh;
    fresh.add(partOf(a));
    fresh.add(partOf(b));
    Report rep2;
    CHECK(fresh.restoreAll(blob, &rep2));
    CHECK(a.value == 11 && b.value == 22);
    CHECK(rep2.restored == 2 && rep2.freshStarts == 0 && rep2.ok());
}

TEST(version_mismatch_without_migrate_starts_fresh, 0) {
    Cell a{"a", 1, 10, 5};
    Registry old;
    old.add(partOf(a));
    Bytes blob;
    CHECK(old.saveAll(&blob, nullptr));
    Cell a2{"a", 2, 10, 99};   // the new build has layout version 2 and cannot read version 1
    Registry now;
    now.add(partOf(a2));
    Report rep;
    CHECK(now.restoreAll(blob, &rep));
    CHECK(a2.restoreCalls == 0);   // the old layout is never read as the new one
    CHECK(a2.freshCalls == 1 && rep.freshStarts == 1);
    CHECK(contains(rep.text(), "no migrate"));
}

TEST(version_mismatch_with_migrate_calls_migrate, 0) {
    Cell a{"a", 1, 10, 5};
    Registry old;
    old.add(partOf(a));
    Bytes blob;
    CHECK(old.saveAll(&blob, nullptr));
    Cell a2{"a", 2, 10, 0};
    Registry now;
    now.add(partOf(a2, true));
    Report rep;
    CHECK(now.restoreAll(blob, &rep));
    CHECK(a2.migrateCalls == 1 && a2.restoreCalls == 0);
    CHECK(a2.value == 1005);   // migrate saw savedVersion 1
    CHECK(rep.migrated == 1 && rep.restored == 1);
}

TEST(corrupt_payload_starts_that_participant_fresh, 14) {
    Cell a{"a", 1, 10, 5};
    Registry old;
    old.add(partOf(a));
    Bytes blob;
    CHECK(old.saveAll(&blob, nullptr));
    // Layout: magic, format, count (12 bytes), name ("a": 4 + 1), version (4), payload length (4), then the payload.
    const size_t payloadAt = 12 + 5 + 4 + 4;
    blob[payloadAt] ^= 0xFF;
    const uint32_t total = crc32(blob.data(), blob.size() - 4);   // fix the whole-blob checksum, so only the payload checksum is wrong
    std::memcpy(blob.data() + blob.size() - 4, &total, 4);
    Cell a2{"a", 1, 10, 77};
    Registry now;
    now.add(partOf(a2));
    Report rep;
    now.restoreAll(blob, &rep);
    CHECK(a2.restoreCalls == 0);
    CHECK(a2.freshCalls == 1);
    CHECK(!rep.failures.empty());
}

TEST(corrupt_blob_makes_everything_fresh, 0) {
    Cell a{"a", 1, 10, 5}, b{"b", 1, 20, 6};
    Registry old;
    old.add(partOf(a));
    old.add(partOf(b));
    Bytes blob;
    CHECK(old.saveAll(&blob, nullptr));
    blob[blob.size() / 2] ^= 0x55;   // damaged, whole-blob checksum now wrong
    Registry now;
    now.add(partOf(a));
    now.add(partOf(b));
    Report rep;
    CHECK(!now.restoreAll(blob, &rep));
    CHECK(a.freshCalls == 1 && b.freshCalls == 1 && a.restoreCalls == 0);
    CHECK(!rep.failures.empty());
}

TEST(empty_and_tiny_blobs_are_harmless, 0) {
    Cell a{"a", 1, 10, 5};
    Registry now;
    now.add(partOf(a));
    Report rep;
    CHECK(now.restoreAll(Bytes(), &rep));   // no blob: a cold start, not an error
    CHECK(a.freshCalls == 1 && rep.failures.empty());
    Report rep2;
    CHECK(!now.restoreAll(Bytes{1, 2, 3, 4, 5}, &rep2));
    CHECK(a.freshCalls == 2);
}

TEST(save_failure_vetoes_the_reload, 0) {
    Cell a{"a", 1, 10, 5};
    Participant p = partOf(a);
    p.save = [](Writer&) { return false; };
    Registry reg;
    reg.add(p);
    Bytes blob;
    Report rep;
    CHECK(!reg.saveAll(&blob, &rep));
    CHECK(blob.empty());
    CHECK(contains(rep.text(), "save a"));
}

TEST(throwing_save_vetoes_the_reload, 0) {
    Participant p;
    p.name = "t";
    p.save = [](Writer&) -> bool { throw 5; };
    Registry reg;
    reg.add(p);
    Report rep;
    CHECK(!reg.saveAll(nullptr, &rep));
    CHECK(contains(rep.text(), "threw"));
}

TEST(preflight_refusal_vetoes_before_saving, 19) {
    Cell a{"a", 1, 10, 5};
    int saves = 0;
    Participant p = partOf(a);
    p.canUnload = [] { return std::string("a hook was chained over by another mod"); };
    p.save = [&saves](Writer&) { ++saves; return true; };
    Registry reg;
    reg.add(p);
    Report rep;
    CHECK(!reg.saveAll(nullptr, &rep));
    CHECK(saves == 0);
    CHECK(contains(rep.text(), "chained over"));
}

TEST(shutdown_runs_in_reverse_order, 15) {
    std::vector<int> seq;
    Registry reg;
    for (int order : {10, 30, 20}) {
        Participant p;
        static const char* names[] = {"x10", "x20", "x30"};
        p.name = names[order / 10 - 1];
        p.order = order;
        p.shutdown = [&seq, order] { seq.push_back(order); };
        reg.add(p);
    }
    reg.shutdownAll(nullptr);
    CHECK((seq == std::vector<int>{30, 20, 10}));
}

TEST(throwing_shutdown_does_not_stop_the_rest, 0) {
    std::vector<int> seq;
    Registry reg;
    for (int order : {10, 20, 30}) {
        Participant p;
        static const char* names[] = {"y10", "y20", "y30"};
        p.name = names[order / 10 - 1];
        p.order = order;
        p.shutdown = [&seq, order] {
            seq.push_back(order);
            if (order == 20) throw 1;
        };
        reg.add(p);
    }
    Report rep;
    reg.shutdownAll(&rep);
    CHECK(seq.size() == 3);
    CHECK(contains(rep.text(), "shutdown y20"));
}

TEST(leak_probe_is_reported_after_shutdown, 0) {
    Registry reg;
    int alive = 2;
    reg.addLeakProbe(LeakProbe{"sockets", [&alive] { return alive; }, 0});
    reg.addLeakProbe(LeakProbe{"hooks", [] { return 1; }, 1});   // equal to its baseline: not a leak
    Report rep;
    CHECK(reg.shutdownAll(&rep) == 2);
    CHECK(rep.leaks.size() == 1 && contains(rep.leaks[0], "sockets"));
    alive = 0;
    Report rep2;
    CHECK(reg.shutdownAll(&rep2) == 0 && rep2.ok());
}

TEST(registering_a_name_twice_replaces_it, 0) {
    Cell a{"a", 1, 10, 1}, a2{"a", 1, 10, 2};
    Registry reg;
    reg.add(partOf(a));
    reg.add(partOf(a2));
    CHECK(reg.size() == 1);
    Bytes blob;
    CHECK(reg.saveAll(&blob, nullptr));
    a.value = a2.value = 0;
    reg.restoreAll(blob, nullptr);
    CHECK(a.value == 0 && a2.value == 2);   // only the later entry runs
}

TEST(state_of_a_removed_participant_is_dropped, 0) {
    Cell a{"a", 1, 10, 1}, b{"b", 1, 20, 2};
    Registry old;
    old.add(partOf(a));
    old.add(partOf(b));
    Bytes blob;
    CHECK(old.saveAll(&blob, nullptr));
    Registry now;
    now.add(partOf(a));
    Report rep;
    CHECK(now.restoreAll(blob, &rep));
    CHECK(rep.dropped == 1 && rep.restored == 1);
}

TEST(a_new_participant_without_saved_state_starts_fresh, 0) {
    Cell a{"a", 1, 10, 1}, c{"c", 1, 30, 3};
    Registry old;
    old.add(partOf(a));
    Bytes blob;
    CHECK(old.saveAll(&blob, nullptr));
    Registry now;
    now.add(partOf(a));
    now.add(partOf(c));
    Report rep;
    now.restoreAll(blob, &rep);
    CHECK(c.freshCalls == 1 && c.restoreCalls == 0);
}

TEST(restore_runs_in_ascending_order, 0) {
    std::vector<int> seq;
    Registry reg;
    static const char* names[] = {"z10", "z20", "z30"};
    for (int order : {30, 10, 20}) {
        Participant p;
        p.name = names[order / 10 - 1];
        p.order = order;
        p.save = [](Writer& w) { w.u8(1); return true; };
        p.restore = [&seq, order](uint32_t, Reader& r) { r.u8(); seq.push_back(order); return true; };
        reg.add(p);
    }
    Bytes blob;
    CHECK(reg.saveAll(&blob, nullptr));
    CHECK(reg.restoreAll(blob, nullptr));
    CHECK((seq == std::vector<int>{10, 20, 30}));
}

// ---- threads -----------------------------------------------------------------------------------------------------
TEST(managed_thread_stops_and_joins, 16) {
    auto finished = std::make_shared<std::atomic<bool>>(false);
    ManagedThread t;
    t.start("worker", [finished](const std::function<bool()>& stop) {
        while (!stop()) Sleep(1);
        Sleep(40);   // cleanup that takes a moment
        *finished = true;
    });
    CHECK(t.running());
    t.stopAndJoin();
    CHECK(finished->load());   // join waited for the cleanup
    CHECK(liveThreads() == 0 || HR_MUT(16));
    Sleep(80);   // lets a detached thread (the twin) end before the test returns
}

TEST(managed_thread_ids_are_listed_while_it_runs, 0) {
    std::atomic<unsigned long> tid{0};
    ManagedThread t;
    t.start("worker", [&tid](const std::function<bool()>& stop) {
        tid = GetCurrentThreadId();
        while (!stop()) Sleep(1);
    });
    while (!tid) Sleep(1);
    auto ids = managedThreadIds();
    CHECK(std::find(ids.begin(), ids.end(), tid.load()) != ids.end());
    CHECK(liveThreads() == 1);
    t.stopAndJoin();
    ids = managedThreadIds();
    CHECK(std::find(ids.begin(), ids.end(), tid.load()) == ids.end());
    CHECK(liveThreads() == 0);
}

TEST(a_thread_that_throws_is_cleaned_up, 0) {
    ManagedThread t;
    t.start("thrower", [](const std::function<bool()>&) { throw 1; });
    t.stopAndJoin();
    CHECK(liveThreads() == 0);
    CHECK(managedThreadIds().empty());
}

TEST(adopted_threads_are_listed_until_released, 0) {
    const unsigned long fake = 0x7ABCDEFu;
    adoptThread(fake);
    adoptThread(fake);   // twice is the same as once
    adoptThread(0);      // thread id 0 is never valid
    auto ids = managedThreadIds();
    CHECK(std::count(ids.begin(), ids.end(), fake) == 1);
    CHECK(std::count(ids.begin(), ids.end(), 0ul) == 0);
    releaseThread(fake);
    ids = managedThreadIds();
    CHECK(std::count(ids.begin(), ids.end(), fake) == 0);
}

// ---- drain -------------------------------------------------------------------------------------------------------
namespace {
// A thread that sits inside this test program's code, the way a core's worker sits inside the core: its instruction
// pointer is in the kernel, only the return addresses on its stack point into the program.
struct Parked {
    std::atomic<bool> stop{false}, up{false};
    std::atomic<unsigned long> tid{0};
    std::thread th;
    void start() {
        th = std::thread([this] {
            tid = GetCurrentThreadId();
            up = true;
            while (!stop) Sleep(1);
        });
        while (!up) Sleep(1);
    }
    void end() {
        stop = true;
        if (th.joinable()) th.join();
    }
    ~Parked() { end(); }
};
std::vector<CodeRange> thisProgram() { return {moduleRangeOf(reinterpret_cast<const void*>(&thisProgram))}; }
}  // namespace

TEST(drain_passes_when_nobody_else_runs_here, 0) {
    DrainResult d = waitDrained(thisProgram(), {}, 500);
    CHECK(d.drained);
    CHECK(d.attempts == 1);
    CHECK(d.why.empty());
}

TEST(drain_sees_a_return_address_on_the_stack, 17) {
    Parked p;
    p.start();
    DrainResult d = waitDrained(thisProgram(), {}, 150);
    CHECK(!d.drained);
    CHECK(d.busyThread == p.tid.load());
    CHECK(contains(d.where, "return address"));
    p.end();
}

TEST(drain_timeout_is_a_failure_with_a_reason, 18) {
    Parked p;
    p.start();
    DrainResult d = waitDrained(thisProgram(), {}, 120);
    CHECK(!d.drained);
    CHECK(d.attempts >= 2);   // a positive timeout always gets a retry
    CHECK(d.elapsedMs >= 100);
    CHECK(contains(d.why, "still inside"));
    p.end();
}

TEST(drain_ignores_threads_the_caller_lists, 0) {
    Parked p;
    p.start();
    DrainResult d = waitDrained(thisProgram(), {p.tid.load()}, 150);
    CHECK(d.drained);
    p.end();
}

TEST(drain_waits_until_a_thread_leaves, 0) {
    std::thread t([] { Sleep(60); });
    DrainResult d = waitDrained(thisProgram(), {}, 5000);
    CHECK(d.drained);
    CHECK(d.attempts >= 2);
    t.join();
}

TEST(drain_result_json_escapes_its_text, 0) {
    DrainResult d;
    d.why = "thread \"x\" \\ is\ninside";
    d.where = "a\tb";
    const std::string j = d.toJson();
    std::string why, where;
    CHECK(JsonGetString(j, "why", why) && why == d.why);
    CHECK(JsonGetString(j, "where", where) && where == d.where);
}

// ---- json helpers ------------------------------------------------------------------------------------------------
TEST(json_lookup_handles_nesting_and_strings, 0) {
    const std::string o = "{\"a\":{\"b\":\"}\"},\"c\":[1,{\"d\":2}],\"e\":\"x\\\"y\",\"f\":-5, \"g\" : true }";
    std::string raw, s;
    long long n = 0;
    CHECK(JsonFindValue(o, "a", raw) && raw == "{\"b\":\"}\"}");
    CHECK(JsonFindValue(o, "c", raw) && raw == "[1,{\"d\":2}]");
    CHECK(JsonGetString(o, "e", s) && s == "x\"y");
    CHECK(JsonGetInt(o, "f", n) && n == -5);
    CHECK(JsonFindValue(o, "g", raw) && raw == "true");
    CHECK(!JsonFindValue(o, "d", raw));   // nested keys are not top level
    CHECK(!JsonFindValue("[1,2]", "a", raw));
    CHECK(!JsonFindValue("not json", "a", raw));
    CHECK(!JsonGetInt(o, "e", n));   // a string is not a number
}

TEST(json_escape_round_trips, 0) {
    const std::string original = "line1\nline2\t\"quoted\" back\\slash \x01 end";
    std::string got;
    CHECK(JsonGetString("{\"k\":\"" + JsonEscape(original) + "\"}", "k", got));
    CHECK(got == original);
    std::string u;
    CHECK(JsonGetString("{\"k\":\"caf\\u00e9\"}", "k", u) && u == "caf\xC3\xA9");
}

// ---- control pipe ------------------------------------------------------------------------------------------------
namespace {
int g_pipeCounter = 0;
std::string uniquePipe() { return "hr-unit-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++g_pipeCounter); }

// One request on a new connection; returns the reply line ("" if the connection failed).
std::string roundtrip(const std::string& pipe, const std::string& request) {
    const std::string path = "\\\\.\\pipe\\" + pipe;
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 600; ++i) {
        h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) break;
        const DWORD e = GetLastError();
        if (e != ERROR_PIPE_BUSY && e != ERROR_FILE_NOT_FOUND) break;
        Sleep(5);
    }
    if (h == INVALID_HANDLE_VALUE) return "";
    const std::string line = request + "\n";
    DWORD n = 0;
    WriteFile(h, line.data(), static_cast<DWORD>(line.size()), &n, nullptr);
    std::string reply;
    char c;
    while (ReadFile(h, &c, 1, &n, nullptr) && n == 1 && c != '\n') reply += c;
    CloseHandle(h);
    return reply;
}

// Runs roundtrip on a helper thread while this thread pumps, the way a game's main loop does.
std::string roundtripPumped(ControlServer& server, const std::string& pipe, const std::string& request) {
    std::string reply;
    std::atomic<bool> done{false};
    std::thread t([&] {
        reply = roundtrip(pipe, request);
        done = true;
    });
    while (!done) {
        server.Pump();
        Sleep(1);
    }
    t.join();
    return reply;
}
}  // namespace

TEST(control_ping_is_answered_without_the_main_loop, 0) {
    const std::string pipe = uniquePipe();
    ControlServer server(pipe, [](const std::string&, const std::string&, std::string&, int&) { return false; });
    std::string err;
    CHECK(server.Start(err));
    const std::string reply = roundtrip(pipe, "{\"id\":9,\"method\":\"ping\"}");   // nobody calls Pump
    CHECK(contains(reply, "\"id\":9"));
    CHECK(contains(reply, "\"pid\":" + std::to_string(GetCurrentProcessId())));
}

TEST(control_methods_run_on_the_pumping_thread, 0) {
    const std::string pipe = uniquePipe();
    std::atomic<unsigned long> handlerThread{0};
    std::string seenParams;
    ControlServer server(pipe, [&](const std::string& m, const std::string& p, std::string& out, int&) {
        handlerThread = GetCurrentThreadId();
        seenParams = p;
        out = "{\"echo\":\"" + m + "\"}";
        return true;
    });
    std::string err;
    CHECK(server.Start(err));
    const std::string reply = roundtripPumped(server, pipe, "{\"id\":\"abc\",\"method\":\"demo.echo\",\"params\":{\"x\":[1,2]}}");
    CHECK(reply == "{\"id\":\"abc\",\"result\":{\"echo\":\"demo.echo\"}}");
    CHECK(handlerThread.load() == GetCurrentThreadId());
    CHECK(seenParams == "{\"x\":[1,2]}");
}

TEST(control_errors_carry_code_and_message, 0) {
    const std::string pipe = uniquePipe();
    ControlServer server(pipe, [](const std::string& m, const std::string&, std::string& out, int& code) {
        if (m == "boom") {
            code = -32000;
            out = "it \"broke\"";
            return false;
        }
        code = -32601;
        out = "unknown method: " + m;
        return false;
    });
    std::string err;
    CHECK(server.Start(err));
    std::string r1 = roundtripPumped(server, pipe, "{\"id\":1,\"method\":\"nope\"}");
    CHECK(contains(r1, "\"code\":-32601") && contains(r1, "unknown method: nope"));
    std::string r2 = roundtripPumped(server, pipe, "{\"id\":2,\"method\":\"boom\"}");
    CHECK(contains(r2, "\"code\":-32000") && contains(r2, "it \\\"broke\\\""));
    std::string r3 = roundtrip(pipe, "this is not json");
    CHECK(contains(r3, "\"code\":-32600"));
}

TEST(control_serves_one_client_after_another, 0) {
    const std::string pipe = uniquePipe();
    int calls = 0;
    ControlServer server(pipe, [&](const std::string&, const std::string&, std::string& out, int&) {
        out = std::to_string(++calls);
        return true;
    });
    std::string err;
    CHECK(server.Start(err));
    CHECK(contains(roundtripPumped(server, pipe, "{\"id\":1,\"method\":\"m\"}"), "\"result\":1}"));
    CHECK(contains(roundtripPumped(server, pipe, "{\"id\":2,\"method\":\"m\"}"), "\"result\":2}"));
    CHECK(contains(roundtripPumped(server, pipe, "{\"id\":3,\"method\":\"m\"}"), "\"result\":3}"));
}

TEST(control_request_times_out_when_the_main_loop_is_stuck, 0) {
    const std::string pipe = uniquePipe();
    int calls = 0;
    ControlServer server(pipe, [&](const std::string&, const std::string&, std::string& out, int&) {
        ++calls;
        out = "1";
        return true;
    });
    std::string err;
    CHECK(server.Start(err));
    const auto t0 = std::chrono::steady_clock::now();
    const std::string reply = roundtrip(pipe, "{\"id\":1,\"method\":\"m\",\"params\":{\"timeoutMs\":1000}}");   // nobody pumps
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(contains(reply, "\"code\":-32001"));
    CHECK(ms >= 900);
    server.Pump();
    CHECK(calls == 0);   // the abandoned request is not run later
}

TEST(control_pipe_name_in_use_is_reported, 0) {
    const std::string pipe = uniquePipe();
    auto h = [](const std::string&, const std::string&, std::string&, int&) { return true; };
    ControlServer first(pipe, h), second(pipe, h);
    std::string err1, err2;
    CHECK(first.Start(err1));
    CHECK(!second.Start(err2));
    CHECK(contains(err2, "already in use"));
}

// ---- runner ------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--twin") == 0) {
#ifdef HOTRELOAD_RUNTIME_MUT
        const int twin = std::atoi(argv[2]);
        for (const Test& t : tests()) {
            if (t.twin != twin) continue;
            hotreload_mut::set(twin);
            g_failedChecks = 0;
            t.fn();
            hotreload_mut::set(0);
            const bool caught = g_failedChecks > 0;
            std::printf("[%s] twin %d guarded by %s\n", caught ? "CAUGHT" : "MISSED", twin, t.name);
            return caught ? 0 : 1;
        }
        std::printf("[MISSED] twin %d has no unit test\n", twin);
        return 1;
#else
        std::printf("this build has no mutation twins (configure with -DHOTRELOAD_MUTATION_TWINS=ON)\n");
        return 2;
#endif
    }
    int passed = 0, failed = 0;
    for (const Test& t : tests()) {
        const int before = g_checks;
        g_failedChecks = 0;
        t.fn();
        const bool ok = g_failedChecks == 0;
        std::printf("[%s] %s (%d checks)\n", ok ? "PASS" : "FAIL", t.name, g_checks - before);
        (ok ? passed : failed)++;
    }
    std::printf("== %d tests, %d passed, %d failed, %d checks ==\n", passed + failed, passed, failed, g_checks);
    return failed ? 1 : 0;
}
