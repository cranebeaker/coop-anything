// Reload drain. Before the core DLL is unmapped, wait until no other thread is executing, or has a frame on its stack
// in, the code that is about to go away. Windows, in-process. A thread that is still inside would crash the game the
// moment the DLL is unmapped.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace hotreload {

struct CodeRange {
    uintptr_t lo = 0, hi = 0;  // [lo, hi)
    const char* what = "";     // static label used in messages ("core image", "hook trampoline")
};

struct DrainResult {
    bool drained = false;
    int attempts = 0;
    int scanned = 0;               // threads inspected in the last attempt
    unsigned long busyThread = 0;  // last thread found inside (0 if drained)
    unsigned elapsedMs = 0;        // wall time spent waiting
    unsigned timeoutMs = 0;        // the limit that was asked for
    std::string why;               // empty when drained, otherwise a complete sentence for the log
    std::string where;             // what made the last busy thread count ("instruction pointer 0x... in <label> ..." or "return address ...")
    // One JSON object with the fields above (strings escaped).
    std::string toJson() const;
};

// Waits up to timeoutMs. `ignore` = thread ids that are not counted (the caller itself is always ignored). The glue passes
// the ManagedThreads and adopted threads here, because their owners stop and join them after the drain.
// A thread counts as inside when its instruction pointer is in a range, or when any aligned word of its live stack
// (stack pointer up to the stack base, at most 1 MB) points into a range (a return address). This is conservative: a
// stale word can delay the drain, never make it succeed wrongly. A thread that cannot be opened, suspended or read, and a
// process with more than 4096 threads, also count as busy, never as drained. timeoutMs 0 = one attempt. A positive
// timeout always gets at least two attempts, even if the first scan alone took longer than the timeout.
// While threads are suspended this does no allocation and no logging.
DrainResult waitDrained(const std::vector<CodeRange>& ranges, const std::vector<unsigned long>& ignore, unsigned timeoutMs);

// The image of the module that contains `anyAddressInCore`, as a range.
CodeRange moduleRangeOf(const void* anyAddressInCore);

}  // namespace hotreload
