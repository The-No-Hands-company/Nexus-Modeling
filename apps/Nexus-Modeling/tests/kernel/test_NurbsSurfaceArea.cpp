#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/NurbsSurfaceArea.h>

#include <cmath>

using namespace nexus::geometry;

namespace {

NurbsSurface makeUnitSquare()
{
    int32_t deg = 1;
    std::vector<float> knotsU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> knotsV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f},
    };
    return NurbsSurface(deg, deg, std::move(knotsU), std::move(knotsV),
                        std::move(ctl), 2, 2);
}

NurbsSurface makeRectangle(float w, float h)
{
    int32_t deg = 1;
    std::vector<float> knotsU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> knotsV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {w, 0.f, 0.f},
        {0.f, h, 0.f},   {w, h, 0.f},
    };
    return NurbsSurface(deg, deg, std::move(knotsU), std::move(knotsV),
                        std::move(ctl), 2, 2);
}

} // namespace

TEST(NurbsSurfaceArea, UnitSquareAreaApproxOne)
{
    NurbsSurface s = makeUnitSquare();
    ASSERT_TRUE(s.isValid());

    NurbsSurfaceAreaResult result = NurbsSurfaceArea::compute(s);

    EXPECT_NEAR(result.totalArea, 1.f, 0.02f);
}

TEST(NurbsSurfaceArea, RectangleAreaMatches)
{
    NurbsSurface s = makeRectangle(2.f, 3.f);
    ASSERT_TRUE(s.isValid());

    NurbsSurfaceAreaResult result = NurbsSurfaceArea::compute(s);

    EXPECT_NEAR(result.totalArea, 6.f, 0.1f);
}

TEST(NurbsSurfaceArea, SubPatchAreaIsFractionOfTotal)
{
    NurbsSurface s = makeRectangle(2.f, 3.f);
    ASSERT_TRUE(s.isValid());

    NurbsSurfaceAreaResult full = NurbsSurfaceArea::compute(s);

    NurbsSurfaceAreaResult half = NurbsSurfaceArea::subPatch(s, 0.f, 1.f, 0.f, 0.5f);

    EXPECT_GT(full.totalArea, 0.f);
    EXPECT_GT(half.totalArea, 0.f);
    EXPECT_LT(half.totalArea, full.totalArea * 0.9f);
}

TEST(NurbsSurfaceArea, EmptySurfaceFails)
{
    NurbsSurface s;
    EXPECT_FALSE(s.isValid());

    NurbsSurfaceAreaResult result = NurbsSurfaceArea::compute(s);

    EXPECT_FLOAT_EQ(result.totalArea, 0.f);
}
