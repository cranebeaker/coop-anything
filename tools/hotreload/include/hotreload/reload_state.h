// Reload state contract. Game-free: no engine headers, no Win32 in this header.
//
// A reloadable core DLL is unloaded and a new build is loaded into the same running game. Everything that must survive
// goes through this registry. Everything that must NOT survive (threads, hooks, sockets) is stopped by a participant's
// `shutdown`, and leak probes check it before the loader may unmap the DLL.
//
// Sequence (the glue in core_glue.h drives it from the core's before_unload and init):
//   old core : saveAll(blob)       every participant writes its versioned state. A failure aborts the reload, the old core stays live.
//   old core : drain               wait until no thread (except the ones the core owns) runs core code
//   old core : shutdownAll()       reverse order: stop threads, remove hooks, close or hand over sockets. Then every leak probe is read.
//   loader   : FreeLibrary(old), LoadLibrary(new)
//   new core : its normal init, then restoreAll(blob)
//
// A blob is what the loader keeps in memory between the two cores. It is plain bytes: no pointers, no vtables, nothing
// that points into the old DLL. Process-wide handles (a SOCKET, a HANDLE) may be stored as integers; they stay valid
// across the unmap as long as shutdown() did not close them.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hotreload {

using Bytes = std::vector<uint8_t>;

// Little-endian byte writer and reader. The reader never throws and never reads out of range: after any short read
// ok() is false and every later get returns zero or empty.
class Writer {
public:
    void u8(uint8_t v) { b_.push_back(v); }
    void u16(uint16_t v) { put(v, 2); }
    void u32(uint32_t v) { put(v, 4); }
    void u64(uint64_t v) { put(v, 8); }
    void i64(int64_t v) { put(static_cast<uint64_t>(v), 8); }
    void f64(double v);
    void boolean(bool v) { u8(v ? 1 : 0); }
    void str(const std::string& s);
    void bytes(const Bytes& v);
    void raw(const void* p, size_t n);
    const Bytes& data() const { return b_; }
    Bytes take() { return std::move(b_); }

private:
    void put(uint64_t v, int n) {
        for (int i = 0; i < n; ++i) b_.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    Bytes b_;
};

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    explicit Reader(const Bytes& b) : p_(b.data()), n_(b.size()) {}
    uint8_t u8() { return static_cast<uint8_t>(get(1)); }
    uint16_t u16() { return static_cast<uint16_t>(get(2)); }
    uint32_t u32() { return static_cast<uint32_t>(get(4)); }
    uint64_t u64() { return get(8); }
    int64_t i64() { return static_cast<int64_t>(get(8)); }
    double f64();
    bool boolean() { return u8() != 0; }
    std::string str();
    Bytes bytes();
    bool ok() const { return ok_; }
    size_t remaining() const { return n_ - pos_; }
    bool atEnd() const { return pos_ == n_; }

private:
    uint64_t get(int n);
    const uint8_t* p_;
    size_t n_, pos_ = 0;
    bool ok_ = true;
};

uint32_t crc32(const void* data, size_t n);

// One per piece of state that must survive a reload (a module, a session, a ledger). Names are unique and stable across builds.
struct Participant {
    const char* name = "";
    uint32_t version = 1;  // bump whenever the saved layout changes
    int order = 100;       // save and restore ascending, shutdown descending

    // Optional preflight, run before anything is saved or stopped. Return "" if the module can be unloaded, else the
    // reason (for example "a hook was chained over by another mod"). A refusal leaves the old core fully working.
    std::function<std::string()> canUnload;

    // The old core is still fully alive. Write everything the new core needs. Return false to veto the reload.
    // This can run and then be thrown away (a later step may refuse), so it must not change anything.
    std::function<bool(Writer&)> save;
    // The new core already ran its normal init. `savedVersion` may differ from `version` only if migrate is not set.
    // Return false to reject the blob (the participant then falls back to fresh()).
    std::function<bool(uint32_t savedVersion, Reader&)> restore;
    // Optional: convert an older layout. If set, it is called instead of restore() when savedVersion != version.
    std::function<bool(uint32_t savedVersion, Reader&)> migrate;
    // Stop threads, remove hooks, close sockets (or leave handed-over ones open). Runs after every save succeeded and the drain passed.
    std::function<void()> shutdown;
    // Optional: start from scratch (no blob, bad blob, version mismatch without migrate, restore rejected).
    std::function<void()> fresh;
};

struct Report {
    std::vector<std::string> notes;     // human-readable, one line each
    std::vector<std::string> failures;  // anything that makes the reload unsafe or lossy
    std::vector<std::string> leaks;     // leak probes that read non-zero after shutdown
    int restored = 0, freshStarts = 0, migrated = 0, dropped = 0;
    bool ok() const { return failures.empty() && leaks.empty(); }
    std::string text() const;
};

// A leak probe returns how many of something are still alive (installed hooks, running threads, open sockets).
// `baseline` is what is legitimately alive when the core is idle.
struct LeakProbe {
    std::string name;
    std::function<int()> count;
    int baseline = 0;
};

class Registry {
public:
    // Registering twice under the same name replaces the earlier entry (a re-run of static init must not duplicate).
    void add(const Participant& p);
    void addLeakProbe(const LeakProbe& p);
    void clear();
    size_t size() const { return parts_.size(); }
    std::vector<std::string> names() const;
    uint32_t versionOf(const std::string& name) const;  // 0 = unknown

    // Old core. False = the reload must be aborted and the old core kept (nothing was shut down).
    bool saveAll(Bytes* blob, Report* rep);
    // Old core. Runs every shutdown (a throwing one does not stop the rest), then reads every leak probe.
    // Returns the number of leaks. The loader must NOT unmap the core when it is non-zero.
    int shutdownAll(Report* rep);
    // New core. A missing, short or corrupt blob is never fatal: every participant just gets fresh().
    // Returns false only when the blob was unusable as a whole.
    bool restoreAll(const Bytes& blob, Report* rep);

    // For tests and status calls: the (name, version) entries of a blob. False if the blob is damaged.
    static bool inspect(const Bytes& blob, std::vector<std::pair<std::string, uint32_t>>* entries);

private:
    std::vector<Participant> parts_;
    std::vector<LeakProbe> probes_;
};

// The process-wide registry of one core DLL. Every loaded core has its own, because the registry lives in the core's image.
Registry& global();

struct Registrar {
    explicit Registrar(const Participant& p) { global().add(p); }
};
// Register a participant at static initialisation time. The arguments are the Participant fields in order:
//   name, version, order, canUnload, save, restore, migrate, shutdown, fresh   (trailing ones may be left out)
//   COOP_RELOAD_STATE(my_module, "my_module", 2, 50, nullptr, saveFn, restoreFn, migrateFn, shutdownFn, freshFn);
// Or build a Participant yourself and call hotreload::global().add(p) from your init.
#define COOP_RELOAD_STATE(ident, ...) \
    static const ::hotreload::Registrar hotreload_participant_##ident(::hotreload::Participant{__VA_ARGS__})

// ---- tracked threads -----------------------------------------------------------------------------------------------
// Every thread a core DLL starts should be a ManagedThread: the destructor asks it to stop and joins, and liveThreads()
// is the leak probe. A raw std::thread (or CreateThread) that outlives its DLL crashes the game the moment it runs.
int liveThreads();

class ManagedThread {
public:
    ManagedThread() = default;
    ~ManagedThread() { stopAndJoin(); }
    ManagedThread(const ManagedThread&) = delete;
    ManagedThread& operator=(const ManagedThread&) = delete;
    // body(stopRequested): must return soon after stopRequested() turns true.
    void start(const char* name, std::function<void(const std::function<bool()>& stopRequested)> body);
    void stopAndJoin();
    bool running() const;
    const std::string& name() const { return name_; }

private:
    struct State;
    std::shared_ptr<State> st_;
    std::string name_;
};

// Windows thread ids of the threads started through ManagedThread right now, plus the adopted ones. The reload drain
// ignores them: their owner stops and joins them, and everything else that runs core code must have left it.
std::vector<unsigned long> managedThreadIds();

// A thread your core owns but does not start through ManagedThread: a worker that a library you link in started, parked
// waiting for jobs inside your code. Its owner stops and joins it in its participant's shutdown (which runs after the
// drain), so the drain must ignore it like a ManagedThread. EVERY THREAD THE CORE OWNS MUST BE REGISTERED THIS WAY OR
// BE A ManagedThread. A thread that is neither makes the drain refuse every reload, forever.
void adoptThread(unsigned long tid);
void releaseThread(unsigned long tid);

LeakProbe threadProbe();

}  // namespace hotreload
