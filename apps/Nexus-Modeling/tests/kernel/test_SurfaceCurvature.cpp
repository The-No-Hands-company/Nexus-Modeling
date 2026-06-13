#include <gtest/gtest.h>

#include <nexus/geometry/SurfaceCurvature.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static NurbsSurface makeFlatSurface()
{
    return NurbsSurface(
        2, 2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, Vec3{0.f, 2.f, 0.f},
            Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 1.f, 0.f}, Vec3{1.f, 2.f, 0.f},
            Vec3{2.f, 0.f, 0.f}, Vec3{2.f, 1.f, 0.f}, Vec3{2.f, 2.f, 0.f},
        },
        3, 3);
}

static NurbsSurface makeCurvedSurface()
{
    return NurbsSurface(
        2, 2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 5.f}, Vec3{0.f, 1.f, 5.f}, Vec3{0.f, 2.f, 5.f},
            Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 1.f, 0.f}, Vec3{1.f, 2.f, 0.f},
            Vec3{2.f, 0.f, 0.f}, Vec3{2.f, 1.f, 0.f}, Vec3{2.f, 2.f, 0.f},
        },
        3, 3);
}

TEST(SurfaceCurvature, FlatPlaneHasZeroCurvature)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    auto samples = SurfaceCurvature::sample(surface, 4, 4);
    ASSERT_GE(samples.size(), 4u);

    for (const auto& s : samples) {
        EXPECT_NEAR(s.gaussianCurvature, 0.f, 1e-4f);
        EXPECT_NEAR(s.meanCurvature, 0.f, 1e-4f);
    }
}

TEST(SurfaceCurvature, ProducesSamplesForCurvedSurface)
{
    auto surface = makeCurvedSurface();
    ASSERT_TRUE(surface.isValid());

    auto samples = SurfaceCurvature::sample(surface, 5, 5);
    EXPECT_EQ(samples.size(), 25u);

    for (const auto& s : samples) {
        EXPECT_TRUE(std::isfinite(s.gaussianCurvature));
        EXPECT_TRUE(std::isfinite(s.meanCurvature));
        EXPECT_TRUE(std::isfinite(s.point.x));
        EXPECT_TRUE(std::isfinite(s.point.y));
        EXPECT_TRUE(std::isfinite(s.point.z));
    }
}

TEST(SurfaceCurvature, EmptySurfaceFails)
{
    NurbsSurface empty;
    EXPECT_FALSE(empty.isValid());

    auto samples = SurfaceCurvature::sample(empty, 2, 2);
    EXPECT_EQ(samples.size(), 0u);
}

} // namespace
