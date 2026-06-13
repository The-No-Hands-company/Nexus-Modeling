#include <nexus/procedural/Noise3D.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nexus::procedural {

static constexpr int32_t kPermTable[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

Noise3D::Noise3D(uint32_t seed) {
    int32_t perm[256];
    for (int32_t i = 0; i < 256; ++i) perm[i] = i;

    uint32_t s = seed;
    for (int32_t i = 255; i > 0; --i) {
        s = s * 1103515245 + 12345;
        int32_t j = static_cast<int32_t>(s % static_cast<uint32_t>(i + 1));
        std::swap(perm[i], perm[j]);
    }

    for (int32_t i = 0; i < 256; ++i) {
        m_perm[i] = perm[i];
        m_perm[i + 256] = perm[i];
    }
}

float Noise3D::grad(int32_t hash, float x, float y, float z) const noexcept {
    int32_t h = hash & 15;
    float u = (h < 8) ? x : y;
    float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Noise3D::noise(float x, float y, float z) const noexcept {
    int32_t ix = static_cast<int32_t>(std::floor(x)) & 255;
    int32_t iy = static_cast<int32_t>(std::floor(y)) & 255;
    int32_t iz = static_cast<int32_t>(std::floor(z)) & 255;

    float fx = x - std::floor(x);
    float fy = y - std::floor(y);
    float fz = z - std::floor(z);

    float u = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    float v = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);
    float w = fz * fz * fz * (fz * (fz * 6.0f - 15.0f) + 10.0f);

    int32_t a = m_perm[ix] + iy;
    int32_t aa = m_perm[a] + iz;
    int32_t ab = m_perm[a + 1] + iz;
    int32_t b = m_perm[ix + 1] + iy;
    int32_t ba = m_perm[b] + iz;
    int32_t bb = m_perm[b + 1] + iz;

    float g000 = grad(m_perm[aa], fx, fy, fz);
    float g100 = grad(m_perm[ba], fx - 1.0f, fy, fz);
    float g010 = grad(m_perm[ab], fx, fy - 1.0f, fz);
    float g110 = grad(m_perm[bb], fx - 1.0f, fy - 1.0f, fz);
    float g001 = grad(m_perm[aa + 1], fx, fy, fz - 1.0f);
    float g101 = grad(m_perm[ba + 1], fx - 1.0f, fy, fz - 1.0f);
    float g011 = grad(m_perm[ab + 1], fx, fy - 1.0f, fz - 1.0f);
    float g111 = grad(m_perm[bb + 1], fx - 1.0f, fy - 1.0f, fz - 1.0f);

    float l00 = g000 + u * (g100 - g000);
    float l01 = g010 + u * (g110 - g010);
    float l10 = g001 + u * (g101 - g001);
    float l11 = g011 + u * (g111 - g011);

    float l0 = l00 + v * (l01 - l00);
    float l1 = l10 + v * (l11 - l10);

    return l0 + w * (l1 - l0);
}

float Noise3D::sample(float x, float y, float z) const noexcept {
    return noise(x, y, z);
}

float Noise3D::fbm(float x, float y, float z,
                    int32_t octaves,
                    float lacunarity,
                    float persistence) const noexcept {
    float value = 0.f;
    float amplitude = 1.f;
    float frequency = 1.f;
    float maxValue = 0.f;

    for (int32_t i = 0; i < octaves; ++i) {
        value += amplitude * noise(x * frequency, y * frequency, z * frequency);
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    if (maxValue > 1e-10f) value /= maxValue;
    return value;
}

} // namespace nexus::procedural
