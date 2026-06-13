#include <gtest/gtest.h>

#include <nexus/geometry/NurbsKnotRefinement.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static NurbsCurve makeCubicCurve() {
    return NurbsCurve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,2,0}, {3,2,0}, {4,0,0}});
}

static NurbsSurface makeBilinearSurface() {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 0.f},
    };
    return NurbsSurface(1, 1, kU, kV, ctl, 2, 2);
}

TEST(NurbsKnotRefinement, InsertKnotSucceedsAndIncreasesCPCount) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int32_t origCP = c.controlPointCount();

    NurbsCurve refined = NurbsKnotRefinement::insertKnot(c, 0.5f);
    ASSERT_TRUE(refined.isValid());
    EXPECT_EQ(refined.controlPointCount(), origCP + 1);
}

TEST(NurbsKnotRefinement, RefineCurveWithMultipleKnotsIncreasesCPCount) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int32_t origCP = c.controlPointCount();

    std::vector<float> knots = {0.25f, 0.5f, 0.75f};
    NurbsCurve refined = NurbsKnotRefinement::refineCurve(c, knots);
    ASSERT_TRUE(refined.isValid());
    EXPECT_EQ(refined.controlPointCount(), origCP + static_cast<int32_t>(knots.size()));
}

TEST(NurbsKnotRefinement, OutOfRangeKnotInsertionFails) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int32_t origCP = c.controlPointCount();

    NurbsCurve before = NurbsKnotRefinement::insertKnot(c, -0.5f);
    EXPECT_EQ(before.controlPointCount(), origCP);

    NurbsCurve after = NurbsKnotRefinement::insertKnot(c, 1.5f);
    EXPECT_EQ(after.controlPointCount(), origCP);
}

TEST(NurbsKnotRefinement, RefinedCurvePreservesEndpointPositions) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());

    Vec3 origStart = c.evaluate(0.f);
    Vec3 origEnd = c.evaluate(1.f);

    std::vector<float> knots = {0.3f, 0.6f};
    NurbsCurve refined = NurbsKnotRefinement::refineCurve(c, knots);
    ASSERT_TRUE(refined.isValid());

    Vec3 refStart = refined.evaluate(0.f);
    Vec3 refEnd = refined.evaluate(1.f);

    EXPECT_NEAR(refStart.x, origStart.x, 0.001f);
    EXPECT_NEAR(refStart.y, origStart.y, 0.001f);
    EXPECT_NEAR(refStart.z, origStart.z, 0.001f);

    EXPECT_NEAR(refEnd.x, origEnd.x, 0.001f);
    EXPECT_NEAR(refEnd.y, origEnd.y, 0.001f);
    EXPECT_NEAR(refEnd.z, origEnd.z, 0.001f);
}
