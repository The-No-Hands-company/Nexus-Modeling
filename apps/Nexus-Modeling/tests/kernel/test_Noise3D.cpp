#include <gtest/gtest.h>

#include <nexus/procedural/Noise3D.h>

using namespace nexus::procedural;

TEST(Noise3D, RangeInBounds) {
    Noise3D noise(42);
    float minVal = 10.f, maxVal = -10.f;
    for (float x = 0.f; x < 10.f; x += 0.5f)
        for (float y = 0.f; y < 10.f; y += 0.5f)
            for (float z = 0.f; z < 10.f; z += 0.5f) {
                float v = noise.sample(x, y, z);
                minVal = std::min(minVal, v);
                maxVal = std::max(maxVal, v);
            }
    EXPECT_GE(minVal, -1.1f);
    EXPECT_LE(maxVal, 1.1f);
}

TEST(Noise3D, Deterministic) {
    Noise3D a(123);
    Noise3D b(123);
    for (int i = 0; i < 50; ++i) {
        float x = i * 0.1f, y = i * 0.2f, z = i * 0.3f;
        EXPECT_FLOAT_EQ(a.sample(x, y, z), b.sample(x, y, z));
    }
}

TEST(Noise3D, DifferentSeedsDiffer) {
    Noise3D a(1);
    Noise3D b(2);
    bool anyDiff = false;
    for (int i = 0; i < 20; ++i) {
        float x = i * 0.7f, y = i * 1.1f, z = i * 0.3f;
        if (a.sample(x, y, z) != b.sample(x, y, z)) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff);
}

TEST(Noise3D, FbmMoreOctavesMoreDetail) {
    Noise3D noise(42);
    // Non-integer coordinates to avoid lattice zeros in Perlin noise
    float v1 = noise.fbm(1.3f, 2.7f, 3.1f, 1);
    float v2 = noise.fbm(1.3f, 2.7f, 3.1f, 4);
    EXPECT_NE(v1, v2);
}
