#include <gtest/gtest.h>

#include <nexus/geometry/SurfaceContinuity.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static NurbsSurface makePlanarSurface(float offsetZ = 0.f) {
    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, offsetZ}, {1.f, 0.f, offsetZ}, {2.f, 0.f, offsetZ}, {3.f, 0.f, offsetZ},
        {0.f, 1.f, offsetZ}, {1.f, 1.f, offsetZ}, {2.f, 1.f, offsetZ}, {3.f, 1.f, offsetZ},
        {0.f, 2.f, offsetZ}, {1.f, 2.f, offsetZ}, {2.f, 2.f, offsetZ}, {3.f, 2.f, offsetZ},
        {0.f, 3.f, offsetZ}, {1.f, 3.f, offsetZ}, {2.f, 3.f, offsetZ}, {3.f, 3.f, offsetZ},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

TEST(SurfaceContinuity, SameEdgeOnSameSurfaceHasG0) {
    NurbsSurface s = makePlanarSurface(0.f);
    ASSERT_TRUE(s.isValid());

    SurfaceContinuityOptions opts;
    opts.samples = 64;

    SurfaceContinuityResult result = SurfaceContinuity::analyze(
        s, SurfaceEdge::UMax, s, SurfaceEdge::UMax, opts);

    EXPECT_TRUE(result.g0Continuous);
    EXPECT_NEAR(result.maxPositionalGap, 0.f, 0.001f);
    EXPECT_NEAR(result.avgPositionalGap, 0.f, 0.001f);
}

TEST(SurfaceContinuity, GapBetweenOffsetSurfacesDetected) {
    NurbsSurface s0 = makePlanarSurface(0.f);
    NurbsSurface s1 = makePlanarSurface(1.f);
    ASSERT_TRUE(s0.isValid());
    ASSERT_TRUE(s1.isValid());

    SurfaceContinuityOptions opts;
    opts.samples = 64;

    SurfaceContinuityResult result = SurfaceContinuity::analyze(
        s0, SurfaceEdge::UMax, s1, SurfaceEdge::UMax, opts);

    EXPECT_GT(result.maxPositionalGap, 0.5f);
}

TEST(SurfaceContinuity, EmptySurfacesFail) {
    NurbsSurface invalid;

    SurfaceContinuityResult result = SurfaceContinuity::analyze(
        invalid, SurfaceEdge::UMax, invalid, SurfaceEdge::UMax);

    EXPECT_FALSE(result.g0Continuous);
    EXPECT_FALSE(result.g1Continuous);
}
