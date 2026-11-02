// Small deterministic RNG (splitmix64 seeded xorshift). Same seed -> same sequence on every platform.
#pragma once
#include <cstdint>

namespace coop {
class Rng {
public:
    explicit Rng(uint64_t seed = 1) { reseed(seed); }
    void reseed(uint64_t seed) {
        s_ = seed + 0x9E3779B97F4A7C15ull;
        for (int i = 0; i < 4; ++i) next();
    }
    uint64_t next() {
        uint64_t z = (s_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    uint32_t u32() { return static_cast<uint32_t>(next() >> 32); }
    // inclusive range
    uint32_t range(uint32_t lo, uint32_t hi) { return lo + static_cast<uint32_t>(next() % (static_cast<uint64_t>(hi - lo) + 1)); }
    double uni() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
    bool chance(double p) { return uni() < p; }
private:
    uint64_t s_;
};
}  // namespace coop
