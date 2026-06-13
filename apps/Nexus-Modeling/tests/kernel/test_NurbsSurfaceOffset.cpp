#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurfaceOffset.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>

using namespace nexus::geometry;

namespace {

NurbsSurface makePlaneSurface() {
    return NurbsSurface(1, 1,
        {0.f, 0.f, 1.f, 1.f},
        {0.f, 0.f, 1.f, 1.f},
        {{0,0,0}, {1,0,0}, {0,0,1}, {1,0,1}},
        2, 2);
}

} // namespace

TEST(NurbsSurfaceOffset, ProducesSurface) {
    NurbsSurface plane = makePlaneSurface();
    ASSERT_TRUE(plane.isValid());

    NurbsSurfaceOffsetOptions opts;
    opts.distance = 0.5f;
    opts.samplesU = 5;
    opts.samplesV = 5;

    NurbsSurface result = NurbsSurfaceOffset::offset(plane, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.controlPointCountU(), 0);
    EXPECT_GT(result.controlPointCountV(), 0);
}

TEST(NurbsSurfaceOffset, OffsetPlaneShiftsInNormal) {
    NurbsSurface plane = makePlaneSurface();
    ASSERT_TRUE(plane.isValid());

    NurbsSurfaceOffsetOptions opts;
    opts.distance = 1.f;
    opts.samplesU = 8;
    opts.samplesV = 8;

    NurbsSurface result = NurbsSurfaceOffset::offset(plane, opts);
    ASSERT_TRUE(result.isValid());

    auto [uMin, uMax] = result.domainU();
    auto [vMin, vMax] = result.domainV();

    Vec3 mid = result.evaluate((uMin + uMax) * 0.5f, (vMin + vMax) * 0.5f);
    EXPECT_NEAR(mid.y, 1.f, 0.2f);
}

TEST(NurbsSurfaceOffset, ZeroDistanceMatchesOriginal) {
    NurbsSurface plane = makePlaneSurface();
    ASSERT_TRUE(plane.isValid());

    NurbsSurfaceOffsetOptions opts;
    opts.distance = 0.f;
    opts.samplesU = 6;
    opts.samplesV = 6;

    NurbsSurface result = NurbsSurfaceOffset::offset(plane, opts);
    ASSERT_TRUE(result.isValid());

    auto [uMin, uMax] = result.domainU();
    auto [vMin, vMax] = result.domainV();

    for (int i = 0; i <= 3; ++i) {
        float u = uMin + static_cast<float>(i) / 3.f * (uMax - uMin);
        for (int j = 0; j <= 3; ++j) {
            float v = vMin + static_cast<float>(j) / 3.f * (vMax - vMin);
            Vec3 pt = result.evaluate(u, v);
            EXPECT_NEAR(pt.y, 0.f, 2e-4f);
        }
    }
}

TEST(NurbsSurfaceOffset, TooFewSamplesProducesDegenerate) {
    NurbsSurface plane = makePlaneSurface();
    ASSERT_TRUE(plane.isValid());

    NurbsSurfaceOffsetOptions opts;
    opts.samplesU = 0;
    opts.samplesV = 0;

    NurbsSurface result = NurbsSurfaceOffset::offset(plane, opts);
    EXPECT_EQ(result.controlPointCountU(), 0);
    EXPECT_EQ(result.controlPointCountV(), 0);
}

TEST(NurbsSurfaceOffset, DefaultsWork) {
    NurbsSurface plane = makePlaneSurface();
    ASSERT_TRUE(plane.isValid());

    NurbsSurface result = NurbsSurfaceOffset::offset(plane);
    EXPECT_TRUE(result.isValid());
}
