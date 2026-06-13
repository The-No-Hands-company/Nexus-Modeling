#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurveOffset.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cmath>

using namespace nexus::geometry;

TEST(NurbsCurveOffset, OffsetLineYEqualsDistance) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {3,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 1.f;
    opts.planeNormal = {0.f, 1.f, 0.f};
    opts.samples = 10;

    auto points = NurbsCurveOffset::offset(curve, opts);
    ASSERT_EQ(points.size(), 10u);
    for (const auto& p : points) {
        EXPECT_NEAR(p.z, -1.f, 1e-4f);
    }
}

TEST(NurbsCurveOffset, OffsetCurvedLineProducesNonEmpty) {
    NurbsCurve curve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,2,0}, {3,2,0}, {4,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 0.5f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 20;

    auto points = NurbsCurveOffset::offset(curve, opts);
    ASSERT_EQ(points.size(), 20u);
    for (int i = 0; i < 20; ++i) {
        float u = static_cast<float>(i) / 19.f;
        Vec3 orig = curve.evaluate(u);
        bool differs = (std::abs(points[i].x - orig.x) > 1e-6f ||
                        std::abs(points[i].y - orig.y) > 1e-6f ||
                        std::abs(points[i].z - orig.z) > 1e-6f);
        EXPECT_TRUE(differs) << "offset point equals original at sample " << i;
    }
}

TEST(NurbsCurveOffset, ZeroDistanceMatchesOriginal) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {2,0,1}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 0.f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 5;

    auto points = NurbsCurveOffset::offset(curve, opts);
    ASSERT_EQ(points.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        float u = static_cast<float>(i) / 4.f;
        Vec3 orig = curve.evaluate(u);
        EXPECT_NEAR(points[i].x, orig.x, 1e-5f);
        EXPECT_NEAR(points[i].y, orig.y, 1e-5f);
        EXPECT_NEAR(points[i].z, orig.z, 1e-5f);
    }
}

TEST(NurbsCurveOffset, TooFewSamplesReturnsEmpty) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {1,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.samples = 0;

    auto points = NurbsCurveOffset::offset(curve, opts);
    EXPECT_TRUE(points.empty());
}

TEST(NurbsCurveOffset, DegenerateNormalDoesNotCrash) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {2,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 1.f;
    opts.planeNormal = {0.f, 0.f, 0.f};
    opts.samples = 5;

    auto points = NurbsCurveOffset::offset(curve, opts);
    EXPECT_FALSE(points.empty());
}

// ─── fitToNurbs() tests ──────────────────────────────────────────────────

TEST(NurbsCurveOffset, FitToNurbsReturnsValidCurve) {
    NurbsCurve curve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,2,0}, {3,2,0}, {4,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 0.5f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 50;
    opts.fitDegree = 3;

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, opts, &report);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(report.valid);
}

TEST(NurbsCurveOffset, FitToNurbsLineProducesOffsetCurve) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {4,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 1.f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 20;
    opts.fitDegree = 1;

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, opts, &report);
    ASSERT_TRUE(result.isValid());
    EXPECT_TRUE(report.valid);

    float midU = 0.5f;
    Vec3 p = result.evaluate(midU);
    EXPECT_NEAR(p.y, 1.f, 1e-3f);
    EXPECT_NEAR(p.z, 0.f, 1e-3f);
}

TEST(NurbsCurveOffset, FitToNurbsInvalidCurveReturnsInvalid) {
    NurbsCurve curve; // default — not valid

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, {}, &report);
    EXPECT_FALSE(result.isValid());
    EXPECT_FALSE(report.valid);
}

TEST(NurbsCurveOffset, FitToNurbsSelfIntersectWarning) {
    NurbsCurve curve(3,
        {0.f, 0.f, 0.f, 0.f, 0.5f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {0.2f,0.8f,0}, {0.8f,0.8f,0}, {1,0,0}, {1.5f,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 2.f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 100;

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, opts, &report);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(report.hasWarning(NurbsCurveOffsetDiagnostic::SelfIntersectWarning));
}

TEST(NurbsCurveOffset, FitToNurbsZeroDistanceMatchesInput) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {3,1,2}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 0.f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 20;
    opts.fitDegree = 1;

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, opts, &report);
    ASSERT_TRUE(result.isValid());
    EXPECT_TRUE(report.valid);

    for (int i = 0; i <= 4; ++i) {
        float u = static_cast<float>(i) / 4.f;
        Vec3 orig = curve.evaluate(u);
        Vec3 off = result.evaluate(u);
        EXPECT_NEAR(off.x, orig.x, 1e-3f);
        EXPECT_NEAR(off.y, orig.y, 1e-3f);
        EXPECT_NEAR(off.z, orig.z, 1e-3f);
    }
}

TEST(NurbsCurveOffset, FitToNurbsFewSamplesReturnsInvalid) {
    NurbsCurve curve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {1,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.samples = 1;

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, opts, &report);
    EXPECT_FALSE(result.isValid());
    EXPECT_FALSE(report.valid);
}

TEST(NurbsCurveOffset, FitToNurbsNoSelfIntersectionForSmallOffset) {
    NurbsCurve curve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,2,0}, {3,2,0}, {4,0,0}});
    ASSERT_TRUE(curve.isValid());

    NurbsCurveOffsetOptions opts;
    opts.distance = 0.1f;
    opts.planeNormal = {0.f, 0.f, 1.f};
    opts.samples = 50;

    NurbsCurveOffsetReport report;
    NurbsCurve result = NurbsCurveOffset::fitToNurbs(curve, opts, &report);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(report.valid);
    EXPECT_FALSE(report.hasWarning(NurbsCurveOffsetDiagnostic::SelfIntersectWarning));
}
