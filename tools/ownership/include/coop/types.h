// Small shared types for the actor-ownership library. Plain C++17, no dependencies.
#pragma once
#include <cstddef>
#include <cstdint>

namespace coop {

// A player slot. Slot 0 is always the host (the machine that runs the OwnershipService).
// The service keeps one bit per slot in a byte, so there are at most 8 slots.
using PlayerId = uint8_t;
constexpr uint8_t kMaxSlots = 8;
constexpr PlayerId kHostPlayer = 0;
constexpr PlayerId kNoPlayer = 0xFF;

// Fowler-Noll-Vo 1a, 64 bit. Used for the ownership digest. The same bytes always give the same hash.
inline uint64_t hash64(const void* data, size_t size, uint64_t seed = 1469598103934665603ull) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < size; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

}  // namespace coop
