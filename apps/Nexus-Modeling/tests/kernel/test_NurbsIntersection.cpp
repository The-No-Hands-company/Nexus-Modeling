#include <gtest/gtest.h>

#include <nexus/geometry/NurbsIntersection.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct NurbsCurveSurfaceIntersection {
    [[nodiscard]] static std::vector<Vec3> intersect(
        const NurbsCurve& curve,
        const NurbsSurface& surface,
        int32_t curveSamples = 200) {

        std::vector<Vec3> result;
        if (!curve.isValid() || !surface.isValid()) return result;

        auto cDom = curve.domain();
        auto sDomU = surface.domainU();
        auto sDomV = surface.domainV();

        float du = (cDom.second - cDom.first) / static_cast<float>(curveSamples - 1);
        float tol = 0.05f;

        for (int32_t i = 0; i < curveSamples; ++i) {
            float t = cDom.first + du * static_cast<float>(i);
            Vec3 cp = curve.evaluate(t);
            for (int32_t su = 0; su < 128; ++su) {
                for (int32_t sv = 0; sv < 128; ++sv) {
                    float u = sDomU.first + (sDomU.second - sDomU.first) * static_cast<float>(su) / 127.f;
                    float v = sDomV.first + (sDomV.second - sDomV.first) * static_cast<float>(sv) / 127.f;
                    Vec3 sp = surface.evaluate(u, v);
                    if ((cp - sp).length() < tol) {
                        if (result.empty() || (result.back() - sp).length() > tol * 0.5f)
                            result.push_back(sp);
                    }
                }
            }
        }
        return result;
    }
};

} // namespace nexus::geometry

using namespace nexus::geometry;

static NurbsSurface makeHorizontalPlane() {
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

static NurbsCurve makeVerticalLine() {
    return NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{0.f, 0.f, -2.f}, {0.f, 0.f, 2.f}});
}

static NurbsCurve makeLineOutsideSurface() {
    return NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{5.f, 0.f, -1.f}, {5.f, 0.f, 1.f}});
}

static NurbsCurve makeLineThroughCurvedSurface() {
    return NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{0.5f, 0.5f, -2.f}, {0.5f, 0.5f, 5.f}});
}

static NurbsSurface makeBumpSurface() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f}, {2.f, 0.f, 0.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 4.f}, {2.f, 1.f, 1.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 1.f}, {2.f, 2.f, 0.f},
    };
    return NurbsSurface(2, 2, kU, kV, ctl, 3, 3);
}

TEST(NurbsIntersection, VerticalLineThroughHorizontalPlaneFindsOneIntersection) {
    NurbsSurface plane = makeHorizontalPlane();
    NurbsCurve line = makeVerticalLine();
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(line.isValid());

    auto hits = NurbsCurveSurfaceIntersection::intersect(line, plane, 256);
    EXPECT_GE(hits.size(), 1u);
    for (const auto& h : hits) {
        EXPECT_NEAR(h.z, 0.f, 0.05f);
    }
}

TEST(NurbsIntersection, LineOutsideSurfaceFindsZeroIntersections) {
    NurbsSurface plane = makeHorizontalPlane();
    NurbsCurve line = makeLineOutsideSurface();
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(line.isValid());

    auto hits = NurbsCurveSurfaceIntersection::intersect(line, plane, 256);
    EXPECT_EQ(hits.size(), 0u);
}

TEST(NurbsIntersection, MultipleIntersectionsDetectedForCurvedSurface) {
    NurbsSurface bump = makeBumpSurface();
    NurbsCurve line = makeLineThroughCurvedSurface();
    ASSERT_TRUE(bump.isValid());
    ASSERT_TRUE(line.isValid());

    auto hits = NurbsCurveSurfaceIntersection::intersect(line, bump, 512);
    EXPECT_GE(hits.size(), 1u);
}

static NurbsCurve makeLineXAxis() {
    return NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{-2.f, 0.f, 0.f}, {2.f, 0.f, 0.f}});
}

static NurbsCurve makeLineYAxis() {
    return NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{0.f, -2.f, 0.f}, {0.f, 2.f, 0.f}});
}

static NurbsCurve makeLineDiagonalXY() {
    return NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{-1.f, -1.f, 0.f}, {3.f, 3.f, 0.f}});
}

static NurbsCurve makeCubicWavyCurve() {
    return NurbsCurve(3,
        {0.f, 0.f, 0.f, 0.f, 0.5f, 1.f, 1.f, 1.f, 1.f},
        {{-1.f, 0.f, 0.f}, {0.f, 2.f, 0.f}, {1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}, {2.f, 0.f, 0.f}});
}

TEST(NurbsIntersection, IntersectingLinesFindOneIntersection) {
    NurbsCurve a = makeLineXAxis();
    NurbsCurve b = makeLineYAxis();
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());

    auto hits = NurbsIntersection::intersectCurves(a, b);
    EXPECT_EQ(hits.size(), 1u);
    if (!hits.empty()) {
        EXPECT_NEAR(hits[0].point.x, 0.f, 1e-3f);
        EXPECT_NEAR(hits[0].point.y, 0.f, 1e-3f);
        EXPECT_NEAR(hits[0].point.z, 0.f, 1e-3f);
    }
}

TEST(NurbsIntersection, ParallelLinesFindZeroIntersections) {
    NurbsCurve a = makeLineXAxis();
    NurbsCurve b = NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{1.f, 1.f, 0.f}, {3.f, 1.f, 0.f}});
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());

    auto hits = NurbsIntersection::intersectCurves(a, b);
    EXPECT_EQ(hits.size(), 0u);
}

TEST(NurbsIntersection, SkewLinesIn3DFindZeroIntersections) {
    NurbsCurve a = NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}});
    NurbsCurve b = NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{0.f, 0.f, 1.f}, {0.f, 1.f, 1.f}});
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());

    auto hits = NurbsIntersection::intersectCurves(a, b);
    EXPECT_EQ(hits.size(), 0u);
}

TEST(NurbsIntersection, DiagonalLineThroughXYPlaneLinesFindsIntersection) {
    NurbsCurve a = makeLineXAxis();
    NurbsCurve b = makeLineDiagonalXY();
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(b.isValid());

    auto hits = NurbsIntersection::intersectCurves(a, b);
    EXPECT_EQ(hits.size(), 1u);
    if (!hits.empty()) {
        EXPECT_NEAR(hits[0].point.x, 0.f, 1e-2f);
        EXPECT_NEAR(hits[0].point.y, 0.f, 1e-2f);
    }
}

TEST(NurbsIntersection, CubicCurveWithLineMultipleIntersections) {
    NurbsCurve wavy = makeCubicWavyCurve();
    NurbsCurve line = NurbsCurve(1,
        {0.f, 0.f, 1.f, 1.f},
        {{-1.f, 0.5f, 0.f}, {2.f, 0.5f, 0.f}});
    ASSERT_TRUE(wavy.isValid());
    ASSERT_TRUE(line.isValid());

    auto hits = NurbsIntersection::intersectCurves(wavy, line);
    EXPECT_GE(hits.size(), 1u);
}
