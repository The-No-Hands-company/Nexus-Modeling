#pragma once

#include <cstdint>
#include <limits>

namespace nexus::geometry::internal {

class SplitMix64 {
public:
    explicit constexpr SplitMix64(uint64_t seed) noexcept
        : m_state(seed) {}

    [[nodiscard]] uint64_t next() noexcept {
        m_state += 0x9E3779B97F4A7C15ULL;
        uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    [[nodiscard]] double uniform01() noexcept {
        static constexpr double kInvMax = 1.0 / static_cast<double>(
            std::numeric_limits<uint64_t>::max());
        return static_cast<double>(next() >> 11) * kInvMax;
    }

private:
    uint64_t m_state;
};

} // namespace nexus::geometry::internal
