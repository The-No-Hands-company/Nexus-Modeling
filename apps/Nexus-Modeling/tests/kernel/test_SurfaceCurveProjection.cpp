#include <gtest/gtest.h>

#include <nexus/geometry/SurfaceCurveProjection.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static NurbsSurface makeXYPlane() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {-1.f, -1.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, -1.f, 0.f}, {2.f, -1.f, 0.f},
        {-1.f,  0.f, 0.f}, {0.f,  0.f, 0.f}, {1.f,  0.f, 0.f}, {2.f,  0.f, 0.f},
        {-1.f,  1.f, 0.f}, {0.f,  1.f, 0.f}, {1.f,  1.f, 0.f}, {2.f,  1.f, 0.f},
        {-1.f,  2.f, 0.f}, {0.f,  2.f, 0.f}, {1.f,  2.f, 0.f}, {2.f,  2.f, 0.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

static NurbsCurve makeCurveAbovePlane(float height) {
    return NurbsCurve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0.f, 0.f, height}, {0.3f, 1.f, height}, {0.7f, 1.f, height}, {1.f, 0.f, height}});
}

TEST(SurfaceCurveProjection, CurveAbovePlaneProjectsOntoPlane) {
    NurbsSurface plane = makeXYPlane();
    NurbsCurve curve = makeCurveAbovePlane(2.f);
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(curve.isValid());

    auto result = SurfaceCurveProjection::projectCurve(plane, curve, 64);
    EXPECT_EQ(result.projectedPoints.size(), 64u);
    EXPECT_EQ(result.params.size(), 64u);

    for (const auto& p : result.projectedPoints) {
        EXPECT_NEAR(p.z, 0.f, 0.1f);
    }
}

TEST(SurfaceCurveProjection, EmptySurfaceFails) {
    NurbsSurface invalid;
    NurbsCurve curve = makeCurveAbovePlane(1.f);

    auto result = SurfaceCurveProjection::projectCurve(invalid, curve, 32);
    EXPECT_TRUE(result.projectedPoints.empty());
    EXPECT_TRUE(result.params.empty());
}

TEST(SurfaceCurveProjection, ProjectsAtCorrectSampleCount) {
    NurbsSurface plane = makeXYPlane();
    NurbsCurve curve = makeCurveAbovePlane(1.f);
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(curve.isValid());

    int32_t samples = 32;
    auto result = SurfaceCurveProjection::projectCurve(plane, curve, samples);
    EXPECT_EQ(static_cast<int32_t>(result.projectedPoints.size()), samples);
    EXPECT_EQ(static_cast<int32_t>(result.params.size()), samples);
}
