#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ec {

// SplitMix64: tiny, deterministic, platform-stable. We deliberately do not use
// std::uniform_*_distribution because its sequence is implementation-defined.
class Rng {
public:
    explicit Rng(std::uint64_t seed = 1) : state_(seed ? seed : 0x9E3779B97F4A7C15ULL) {}

    std::uint64_t nextU64() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    std::uint32_t nextU32() { return static_cast<std::uint32_t>(nextU64() >> 32); }

    double nextDouble() {
        return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0);
    }

    float nextFloat() { return static_cast<float>(nextDouble()); }

    int nextInt(int minInclusive, int maxInclusive) {
        if (maxInclusive <= minInclusive) return minInclusive;
        const std::uint32_t span = static_cast<std::uint32_t>(maxInclusive - minInclusive + 1);
        return minInclusive + static_cast<int>(nextU32() % span);
    }

    bool chance(double probability) { return nextDouble() < probability; }

    double signedUnit() { return nextDouble() * 2.0 - 1.0; }

    double gaussian() {
        // Box-Muller; only called off the audio thread.
        const double u1 = std::max(1e-12, nextDouble());
        const double u2 = nextDouble();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }

    void reseed(std::uint64_t seed) { state_ = seed ? seed : 0x9E3779B97F4A7C15ULL; }
    std::uint64_t state() const { return state_; }

private:
    std::uint64_t state_;
};

inline std::uint64_t mixSeed(std::uint64_t a, std::uint64_t b) {
    std::uint64_t z = a + 0x9E3779B97F4A7C15ULL + (b << 6) + (b >> 2);
    z ^= b;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

} // namespace ec
