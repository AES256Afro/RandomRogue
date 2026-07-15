// Seeded, portable RNG (PCG32). One master seed is split into independent
// streams so worldgen / language / runtime don't perturb each other.
#pragma once
#include <cstdint>
#include <vector>

struct Rng {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    Rng() = default;
    Rng(uint64_t seed, uint64_t stream) {
        state = 0u;
        inc = (stream << 1u) | 1u;
        next();
        state += seed;
        next();
    }

    uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Inclusive range [lo, hi].
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + (int)(next() % (uint32_t)(hi - lo + 1));
    }
    int d(int sides) { return range(1, sides); }
    bool chance(int pct) { return range(1, 100) <= pct; }

    template <class T>
    const T& pick(const std::vector<T>& v) { return v[next() % v.size()]; }
};

// Stream ids for the master seed split.
enum RngStream : uint64_t {
    STREAM_LANG    = 11,
    STREAM_WORLD   = 22,
    STREAM_RUNTIME = 33,
    STREAM_HISTORY = 44,
};
