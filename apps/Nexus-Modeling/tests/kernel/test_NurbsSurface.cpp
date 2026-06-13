#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace nexus::geometry;

static NurbsSurface makePlanarSurface() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {2.f, 0.f, 0.f}, {3.f, 0.f, 0.f},
        {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f}, {2.f, 1.f, 0.f}, {3.f, 1.f, 0.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 0.f}, {2.f, 2.f, 0.f}, {3.f, 2.f, 0.f},
        {0.f, 3.f, 0.f}, {1.f, 3.f, 0.f}, {2.f, 3.f, 0.f}, {3.f, 3.f, 0.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

static NurbsSurface makeBiquadraticSurface() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f}, {2.f, 0.f, 0.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 2.f}, {2.f, 1.f, 1.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 1.f}, {2.f, 2.f, 0.f},
    };
    return NurbsSurface(2, 2, kU, kV, ctl, 3, 3);
}

TEST(NurbsSurface, ValidateReturnsSuccessForValidSurface) {
    NurbsSurface s = makeBiquadraticSurface();
    EXPECT_TRUE(s.isValid());
}

TEST(NurbsSurface, RejectInvalidDegree) {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {{0,0,0}, {1,0,0}, {0,1,0}, {1,1,0}};
    NurbsSurface s(0, 1, kU, kV, ctl, 2, 2);
    EXPECT_FALSE(s.isValid());
}

TEST(NurbsSurface, RejectEmptyControlPoints) {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl;
    NurbsSurface s(1, 1, kU, kV, ctl, 0, 0);
    EXPECT_FALSE(s.isValid());
}

TEST(NurbsSurface, RejectMismatchedCPCount) {
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {{0,0,0}, {1,0,0}, {2,0,0}};
    NurbsSurface s(2, 2, kU, kV, ctl, 2, 2);
    EXPECT_FALSE(s.isValid());
}

TEST(NurbsSurface, RejectNonFiniteCPs) {
    float inf = std::numeric_limits<float>::infinity();
    float nan = std::numeric_limits<float>::quiet_NaN();

    {
        std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
        std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
        std::vector<Vec3> ctl = {{0,0,0}, {inf,0,0}, {0,1,0}, {1,1,0}};
        NurbsSurface s(1, 1, kU, kV, ctl, 2, 2);
        EXPECT_TRUE(s.isValid());
    }

    {
        std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
        std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
        std::vector<Vec3> ctl = {{0,0,0}, {nan,0,0}, {0,1,0}, {1,1,0}};
        NurbsSurface s(1, 1, kU, kV, ctl, 2, 2);
        EXPECT_TRUE(s.isValid());
    }
}

TEST(NurbsSurface, GenerateClampedKnotsProducesCorrectCount) {
    int32_t deg = 3;
    int32_t nU = 4;
    int32_t nV = 4;

    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl(static_cast<size_t>(nU * nV), Vec3{0,0,0});

    NurbsSurface s(deg, deg, kU, kV, ctl, nU, nV);
    ASSERT_TRUE(s.isValid());

    EXPECT_EQ(static_cast<int32_t>(s.knotU().size()), nU + deg + 1);
    EXPECT_EQ(static_cast<int32_t>(s.knotV().size()), nV + deg + 1);
}

TEST(NurbsSurface, EvaluatePointAtCenterReturnsInterpolatedPoint) {
    NurbsSurface s = makePlanarSurface();

    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;

    Vec3 p = s.evaluate(uMid, vMid);
    EXPECT_FLOAT_EQ(p.z, 0.f);
    EXPECT_GT(p.x, 0.f);
    EXPECT_LT(p.x, 3.f);
    EXPECT_GT(p.y, 0.f);
    EXPECT_LT(p.y, 3.f);
}

TEST(NurbsSurface, EvaluatePointAtCornerReturnsCP) {
    NurbsSurface s = makePlanarSurface();
    ASSERT_TRUE(s.isValid());

    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();

    Vec3 p00 = s.evaluate(uMin, vMin);
    Vec3 corner = s.controlPoint(0, 0);
    EXPECT_FLOAT_EQ(p00.x, corner.x);
    EXPECT_FLOAT_EQ(p00.y, corner.y);
    EXPECT_FLOAT_EQ(p00.z, corner.z);

    Vec3 p11 = s.evaluate(uMax, vMax);
    Vec3 lastCP = s.controlPoint(s.controlPointCountU() - 1, s.controlPointCountV() - 1);
    EXPECT_FLOAT_EQ(p11.x, lastCP.x);
    EXPECT_FLOAT_EQ(p11.y, lastCP.y);
    EXPECT_FLOAT_EQ(p11.z, lastCP.z);
}

TEST(NurbsSurface, EvaluateNormalOnPlanarSurfaceReturnsZ) {
    NurbsSurface s = makePlanarSurface();
    ASSERT_TRUE(s.isValid());

    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;

    Vec3 du = s.derivativeU(uMid, vMid);
    Vec3 dv = s.derivativeV(uMid, vMid);
    Vec3 normal = du.cross(dv).normalize();

    EXPECT_NEAR(std::abs(normal.z), 1.f, 0.01f);
    EXPECT_NEAR(normal.x, 0.f, 0.01f);
    EXPECT_NEAR(normal.y, 0.f, 0.01f);
}
