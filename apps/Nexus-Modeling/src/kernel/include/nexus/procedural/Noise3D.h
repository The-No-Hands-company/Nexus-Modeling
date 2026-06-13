#pragma once
// ── Nexus Procedural — Noise3D

#include <cstdint>

namespace nexus::procedural {

class Noise3D {
public:
    Noise3D(uint32_t seed = 0);

    [[nodiscard]] float sample(float x, float y, float z) const noexcept;

    [[nodiscard]] float fbm(float x, float y, float z,
                            int32_t octaves = 6,
                            float lacunarity = 2.0f,
                            float persistence = 0.5f) const noexcept;

private:
    static constexpr int32_t kTableSize = 256;

    int32_t m_perm[kTableSize * 2] = {};

    [[nodiscard]] float grad(int32_t hash, float x, float y, float z) const noexcept;
    [[nodiscard]] float noise(float x, float y, float z) const noexcept;
};

} // namespace nexus::procedural
