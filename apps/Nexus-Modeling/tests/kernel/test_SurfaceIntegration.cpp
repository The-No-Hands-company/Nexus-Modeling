#include <gtest/gtest.h>

#include <nexus/geometry/SurfaceIntegration.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

static NurbsSurface makeUnitSquare()
{
    int32_t degU = 1, degV = 1;
    int32_t nU = 2, nV = 2;
    std::vector<float> knotU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> knotV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f},
        {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f},
    };

    return NurbsSurface(degU, degV, knotU, knotV, ctl, nU, nV);
}

static NurbsSurface makeRectangle()
{
    int32_t degU = 1, degV = 1;
    int32_t nU = 2, nV = 2;
    std::vector<float> knotU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> knotV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {0.f, 3.f, 0.f},
        {2.f, 0.f, 0.f}, {2.f, 3.f, 0.f},
    };

    return NurbsSurface(degU, degV, knotU, knotV, ctl, nU, nV);
}

TEST(SurfaceIntegration, UnitSquareAreaApproxOne)
{
    auto surf = makeUnitSquare();
    ASSERT_TRUE(surf.isValid());

    SurfaceIntegrationOptions opts;
    opts.samplesU = 64;
    opts.samplesV = 64;

    auto result = SurfaceIntegration::compute(surf, opts);
    EXPECT_NEAR(result.area, 1.f, 0.1f);
}

TEST(SurfaceIntegration, RectangleAreaMatches)
{
    auto surf = makeRectangle();
    ASSERT_TRUE(surf.isValid());

    SurfaceIntegrationOptions opts;
    opts.samplesU = 64;
    opts.samplesV = 64;

    auto result = SurfaceIntegration::compute(surf, opts);
    EXPECT_NEAR(result.area, 6.f, 0.2f);
}

TEST(SurfaceIntegration, EmptySurfaceFails)
{
    NurbsSurface invalid;

    auto result = SurfaceIntegration::compute(invalid);
    EXPECT_FLOAT_EQ(result.area, 0.f);
    EXPECT_FLOAT_EQ(result.signedVolume, 0.f);
}

} // namespace
