#include <gtest/gtest.h>

#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cmath>

using namespace nexus::geometry;

namespace {

// Helper: create a degree-1 line curve from point a to point b.
NurbsCurve makeLineCurve(Vec3 a, Vec3 b) {
    return NurbsCurve(1,
                      {0.f, 0.f, 1.f, 1.f},
                      {a, b});
}

// Helper: create a degree-2 arc curve (3-point polyline approximation).
NurbsCurve makeArcCurve(Vec3 start, Vec3 mid, Vec3 end) {
    return NurbsCurve(2,
                      {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
                      {start, mid, end});
}

} // namespace

// ── Revolve tests ─────────────────────────────────────────────────────────

TEST(RevolveOperation, RevolveLineAroundZProducesCylinder)
{
    // Profile: line from (1, 0, 0) to (1, 0, 2) — vertical line at radius 1.
    NurbsCurve profile = makeLineCurve({1.f, 0.f, 0.f}, {1.f, 0.f, 2.f});

    RevolveDesc desc;
    desc.axisOrigin    = {0.f, 0.f, 0.f};
    desc.axisDirection = {0.f, 0.f, 1.f};
    desc.angularSamples = 16;

    Mesh mesh;
    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::Success);
    EXPECT_TRUE(report.converged);
    EXPECT_GT(report.vertexCount, 0u);
    EXPECT_GT(report.faceCount, 0u);

    // All vertices should be at radius ~1 from Z axis.
    const auto& pos = mesh.attributes().positions();
    for (size_t i = 0; i < pos.size(); ++i) {
        float r = std::sqrt(pos[i].x * pos[i].x + pos[i].y * pos[i].y);
        EXPECT_NEAR(r, 1.f, 1e-5f);
    }
}

TEST(RevolveOperation, RevolvePartialAngleProducesPartialSurface)
{
    NurbsCurve profile = makeLineCurve({2.f, 0.f, 0.f}, {2.f, 0.f, 1.f});

    RevolveDesc desc;
    desc.axisOrigin    = {0.f, 0.f, 0.f};
    desc.axisDirection = {0.f, 0.f, 1.f};
    desc.startAngleDeg = 0.f;
    desc.endAngleDeg   = 180.f;
    desc.angularSamples = 8;

    Mesh mesh;
    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::Success);
    EXPECT_TRUE(report.converged);
    EXPECT_GT(report.faceCount, 0u);

    // All vertices should have x in range [-2, 2] and y >= 0 (upper half).
    const auto& pos = mesh.attributes().positions();
    for (size_t i = 0; i < pos.size(); ++i) {
        EXPECT_GE(pos[i].y, -1e-5f);
    }
}

TEST(RevolveOperation, RevolveOutputsNurbsSurface)
{
    NurbsCurve profile = makeLineCurve({0.5f, 0.f, 0.f}, {0.5f, 0.f, 1.f});

    RevolveDesc desc;
    desc.outputAsNurbsSurface = true;
    desc.angularSamples = 8;

    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::Success);
    EXPECT_TRUE(surf.isValid());
}

TEST(RevolveOperation, RevolveRejectsInvalidCurve)
{
    NurbsCurve invalid; // default-constructed, not valid

    RevolveDesc desc;
    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(invalid, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::InvalidProfileCurve);
    EXPECT_FALSE(report.converged);
}

TEST(RevolveOperation, RevolveRejectsDegenerateAxis)
{
    NurbsCurve profile = makeLineCurve({1.f, 0.f, 0.f}, {1.f, 0.f, 2.f});

    RevolveDesc desc;
    desc.axisDirection = {0.f, 0.f, 0.f};

    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::DegenerateAxis);
    EXPECT_FALSE(report.converged);
}

TEST(RevolveOperation, RevolveRejectsInvalidAngles)
{
    NurbsCurve profile = makeLineCurve({1.f, 0.f, 0.f}, {1.f, 0.f, 2.f});

    RevolveDesc desc;
    desc.startAngleDeg = 360.f;
    desc.endAngleDeg   = 0.f;

    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::InvalidAngles);
}

// ── Curve extrude tests ────────────────────────────────────────────────────

TEST(CurveExtrude, ExtrudeLineAlongZProducesPlane)
{
    NurbsCurve profile = makeLineCurve({-1.f, 0.f, 0.f}, {1.f, 0.f, 0.f});

    CurveExtrudeDesc desc;
    desc.direction = {0.f, 0.f, 1.f};
    desc.height    = 3.f;
    desc.heightSamples = 4;

    Mesh mesh;
    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::Success);
    EXPECT_TRUE(report.converged);
    EXPECT_GT(report.vertexCount, 0u);
    EXPECT_GT(report.faceCount, 0u);

    // All vertices should have z in [0, 3] and y near 0.
    const auto& pos = mesh.attributes().positions();
    for (size_t i = 0; i < pos.size(); ++i) {
        EXPECT_GE(pos[i].z, -1e-5f);
        EXPECT_LE(pos[i].z, 3.f + 1e-5f);
        EXPECT_NEAR(pos[i].y, 0.f, 1e-5f);
    }
}

TEST(CurveExtrude, ExtrudeWithDraftAngleTapers)
{
    NurbsCurve profile = makeLineCurve({-1.f, 0.f, 0.f}, {1.f, 0.f, 0.f});

    CurveExtrudeDesc desc;
    desc.direction    = {0.f, 0.f, 1.f};
    desc.height       = 2.f;
    desc.draftAngleDeg = 10.f;
    desc.heightSamples = 8;

    Mesh mesh;
    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::Success);
    EXPECT_TRUE(report.converged);

    // At the top (z ~ 2), the width should be less than the base (z ~ 0).
    const auto& pos = mesh.attributes().positions();
    float maxBaseX = 0.f;
    float maxTopX  = 0.f;
    for (size_t i = 0; i < pos.size(); ++i) {
        if (std::abs(pos[i].z) < 0.01f) {
            maxBaseX = std::max(maxBaseX, std::abs(pos[i].x));
        }
        if (std::abs(pos[i].z - 2.f) < 0.01f) {
            maxTopX = std::max(maxTopX, std::abs(pos[i].x));
        }
    }
    EXPECT_LT(maxTopX, maxBaseX);
}

TEST(CurveExtrude, ExtrudeOutputsNurbsSurface)
{
    NurbsCurve profile = makeLineCurve({0.f, 0.f, 0.f}, {2.f, 0.f, 0.f});

    CurveExtrudeDesc desc;
    desc.direction = {0.f, 0.f, 1.f};
    desc.height    = 5.f;
    desc.outputAsNurbsSurface = true;

    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::Success);
    EXPECT_TRUE(surf.isValid());
}

TEST(CurveExtrude, ExtrudeRejectsInvalidCurve)
{
    NurbsCurve invalid;

    CurveExtrudeDesc desc;
    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(invalid, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::InvalidProfileCurve);
    EXPECT_FALSE(report.converged);
}

TEST(CurveExtrude, ExtrudeRejectsZeroDirection)
{
    NurbsCurve profile = makeLineCurve({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f});

    CurveExtrudeDesc desc;
    desc.direction = {0.f, 0.f, 0.f};

    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::InvalidDirection);
}

TEST(CurveExtrude, ExtrudeRejectsNonPositiveHeight)
{
    NurbsCurve profile = makeLineCurve({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f});

    CurveExtrudeDesc desc;
    desc.height = 0.f;

    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, nullptr);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::InvalidHeight);
}

TEST(CurveExtrude, ExtrudeDeterministic)
{
    NurbsCurve profile = makeLineCurve({0.f, 0.f, 0.f}, {3.f, 0.f, 0.f});

    CurveExtrudeDesc desc;
    desc.direction = {0.f, 0.f, 2.f};
    desc.height    = 4.f;

    Mesh meshA, meshB;
    NurbsSurface sA, sB;

    CurveExtrudeOperation::extrude(profile, desc, sA, &meshA);
    CurveExtrudeOperation::extrude(profile, desc, sB, &meshB);

    EXPECT_EQ(meshA.attributes().positions().size(),
              meshB.attributes().positions().size());

    const auto& posA = meshA.attributes().positions();
    const auto& posB = meshB.attributes().positions();
    for (size_t i = 0; i < posA.size(); ++i) {
        EXPECT_FLOAT_EQ(posA[i].x, posB[i].x);
        EXPECT_FLOAT_EQ(posA[i].y, posB[i].y);
        EXPECT_FLOAT_EQ(posA[i].z, posB[i].z);
    }
}

// ── End cap tests ─────────────────────────────────────────────────────────

TEST(RevolveOperation, CapsProduceExtraFaces)
{
    NurbsCurve profile = makeLineCurve({2.f, 0.f, 0.f}, {2.f, 0.f, 5.f});

    RevolveDesc desc;
    desc.angularSamples = 12;
    desc.capEnds = true;

    Mesh mesh;
    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::Success);
    EXPECT_GT(report.capFaceCount, 0u);
    EXPECT_GT(report.faceCount, report.capFaceCount);
}

TEST(RevolveOperation, CapsNotAddedForPartialAngle)
{
    NurbsCurve profile = makeLineCurve({2.f, 0.f, 0.f}, {2.f, 0.f, 3.f});

    RevolveDesc desc;
    desc.startAngleDeg = 0.f;
    desc.endAngleDeg   = 180.f;
    desc.angularSamples = 8;
    desc.capEnds = true;

    Mesh mesh;
    NurbsSurface surf;
    RevolveReport report = RevolveOperation::revolve(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, RevolveDiagnostic::Success);
    EXPECT_EQ(report.capFaceCount, 0u);
}

TEST(CurveExtrude, CapsProduceExtraFaces)
{
    // Triangle profile with 3 control points → enough for fan triangulation.
    NurbsCurve profile(2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {Vec3{0.f, 0.f, 0.f}, Vec3{2.f, 0.f, 0.f}, Vec3{0.f, 3.f, 0.f}});

    CurveExtrudeDesc desc;
    desc.direction = {0.f, 0.f, 1.f};
    desc.height    = 4.f;
    desc.capEnds   = true;

    Mesh mesh;
    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::Success);
    EXPECT_GT(report.capFaceCount, 0u);
}

TEST(CurveExtrude, CapsSkippedForTooFewProfileSamples)
{
    // A profile with only 2 vertices → not enough for fan triangulation.
    NurbsCurve profile(1, {0.f, 0.f, 1.f, 1.f}, {Vec3{0,0,0}, Vec3{1,0,0}});

    CurveExtrudeDesc desc;
    desc.direction = {0.f, 0.f, 1.f};
    desc.height    = 3.f;
    desc.capEnds   = true;
    // profileSampleCount returns controlPointCount which is 2,
    // so ps=2, and caps require ps>=3.

    Mesh mesh;
    NurbsSurface surf;
    CurveExtrudeReport report = CurveExtrudeOperation::extrude(profile, desc, surf, &mesh);

    EXPECT_EQ(report.diagnostic, CurveExtrudeDiagnostic::Success);
    EXPECT_EQ(report.capFaceCount, 0u);
}
