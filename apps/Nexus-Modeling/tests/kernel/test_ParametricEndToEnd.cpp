#include <gtest/gtest.h>

#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/parametric/ParametricFeature.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cmath>
#include <vector>

using namespace nexus::parametric;
using namespace nexus::geometry;

namespace {

double distance3(const ParametricPoint3& a, const ParametricPoint3& b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

// ── Sketch factory method tests ──────────────────────────────────────────

TEST(ParametricSketch, AddArcCreatesNonEmptyPolyline)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    std::vector<ParametricEntityId> pts =
        ParametricSketchFactory::addArc(sk, 0.0, 0.0, 10.0, 0.0, 180.0, 8u);

    EXPECT_EQ(pts.size(), 9u);
    EXPECT_GE(sk.graph.sketchPlaneConstraintCount(), 9u);
    EXPECT_GE(sk.graph.distanceConstraintCount(), 8u);
}

TEST(ParametricSketch, AddEllipseCreatesPolygon)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    std::vector<ParametricEntityId> pts =
        ParametricSketchFactory::addEllipse(sk, 0.0, 0.0, 4.0, 2.0, 24u);

    EXPECT_EQ(pts.size(), 24u);
    EXPECT_GE(sk.graph.sketchPlaneConstraintCount(), 24u);
    EXPECT_GE(sk.graph.distanceConstraintCount(), 24u);

    double maxX = 0.0, maxY = 0.0;
    for (auto id : pts) {
        const auto* p = sk.graph.point(id);
        ASSERT_NE(p, nullptr);
        maxX = std::max(maxX, std::abs(p->x));
        maxY = std::max(maxY, std::abs(p->y));
    }
    EXPECT_NEAR(maxX, 4.0, 0.5);
    EXPECT_NEAR(maxY, 2.0, 0.5);
}

TEST(ParametricSketch, AddPolylineCreatesChain)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    std::vector<ParametricEntityId> pts =
        ParametricSketchFactory::addPolyline(sk,
            {{0.0, 0.0}, {3.0, 0.0}, {3.0, 4.0}, {0.0, 4.0}}, true);

    EXPECT_EQ(pts.size(), 4u);
    EXPECT_GE(sk.graph.distanceConstraintCount(), 4u);

    ParametricSolverReport report = ParametricSolver::solve(sk.graph);
    EXPECT_TRUE(report.converged);

    EXPECT_NEAR(distance3(*sk.graph.point(pts[0]), *sk.graph.point(pts[1])), 3.0, 1e-9);
    EXPECT_NEAR(distance3(*sk.graph.point(pts[1]), *sk.graph.point(pts[2])), 4.0, 1e-9);
}

// ── End-to-end: sketch → constrain → solve → revolt/extrude ──────────────

TEST(ParametricEndToEnd, SketchRectangleRevolveProducesCylinder)
{
    // 1. Create sketch with a constrained rectangle.
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto [origin, xHandle, yHandle, corner] =
        ParametricSketchFactory::addRectangle(sk, 0.0, 0.0, 3.0, 5.0);

    // 2. Solve the constraint graph.
    ParametricSolverReport solveReport = ParametricSolver::solve(sk.graph);
    ASSERT_TRUE(solveReport.converged);

    // 3. Build a vertical profile for the cylinder wall.
    //    The sketch rectangle's right edge (xHandle→corner) defines the wall
    //    at distance width from the Z axis. Use explicit coordinates.
    Vec3 a{3.f, 0.f, 0.f};
    Vec3 b{3.f, 0.f, 5.f};
    NurbsCurve profile(1, {0.f, 0.f, 1.f, 1.f}, {a, b});

    // 4. Revolve around Z axis.
    RevolveDesc revolveDesc;
    revolveDesc.axisOrigin    = {0.f, 0.f, 0.f};
    revolveDesc.axisDirection = {0.f, 0.f, 1.f};
    revolveDesc.angularSamples = 12;
    revolveDesc.capEnds = true;

    Mesh mesh;
    NurbsSurface surf;
    RevolveReport revRep = RevolveOperation::revolve(profile, revolveDesc, surf, &mesh);

    EXPECT_EQ(revRep.diagnostic, RevolveDiagnostic::Success);
    EXPECT_TRUE(revRep.converged);
    EXPECT_GT(revRep.faceCount, 0u);

    // 5. All body vertices should be at radius ~3 from Z axis (cap centers
    //    sit on the axis at radius 0).
    const auto& pos = mesh.attributes().positions();
    const size_t bodyCount = 12u * 2u; // angularSamples * profSteps
    for (size_t i = 0; i < pos.size(); ++i) {
        float r = std::sqrt(pos[i].x * pos[i].x + pos[i].y * pos[i].y);
        if (i < bodyCount) {
            EXPECT_NEAR(r, 3.f, 1e-4f);
        }
    }
}

TEST(ParametricEndToEnd, SketchLineExtrudeProducesPlane)
{
    // 1. Create a sketch with a constrained line segment.
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricEntityId p1 = ParametricSketchFactory::addSketchPoint(sk, -2.0, 0.0);
    ParametricEntityId p2 = ParametricSketchFactory::addSketchPoint(sk, 2.0, 0.0);
    ASSERT_NE(ParametricSketchFactory::addLineSegment(sk, p1, p2), kInvalidConstraintId);

    // 2. Solve constraints.
    ParametricSolverReport solveReport = ParametricSolver::solve(sk.graph);
    ASSERT_TRUE(solveReport.converged);

    // 3. Build profile from the solved points.
    const auto* pp1 = sk.graph.point(p1);
    const auto* pp2 = sk.graph.point(p2);
    ASSERT_NE(pp1, nullptr);
    ASSERT_NE(pp2, nullptr);

    Vec3 a{static_cast<float>(pp1->x), static_cast<float>(pp1->y), 0.f};
    Vec3 b{static_cast<float>(pp2->x), static_cast<float>(pp2->y), 0.f};
    NurbsCurve profile(1, {0.f, 0.f, 1.f, 1.f}, {a, b});

    // 4. Extrude along Z.
    CurveExtrudeDesc extrudeDesc;
    extrudeDesc.direction = {0.f, 0.f, 1.f};
    extrudeDesc.height    = 6.f;
    extrudeDesc.heightSamples = 4;

    Mesh mesh;
    NurbsSurface surf;
    CurveExtrudeReport exRep = CurveExtrudeOperation::extrude(profile, extrudeDesc, surf, &mesh);

    EXPECT_EQ(exRep.diagnostic, CurveExtrudeDiagnostic::Success);
    EXPECT_TRUE(exRep.converged);
    EXPECT_GT(exRep.faceCount, 0u);

    // 5. Verify Z extent.
    const auto& pos = mesh.attributes().positions();
    for (size_t i = 0; i < pos.size(); ++i) {
        EXPECT_GE(pos[i].z, -1e-5f);
        EXPECT_LE(pos[i].z, 6.f + 1e-5f);
    }
}

TEST(ParametricEndToEnd, SketchCircleRevolveProducesTorus)
{
    // 1. Create a circle sketch offset from origin.
    SketchProfile sk = ParametricSketchFactory::createSketch();
    std::vector<ParametricEntityId> circlePts =
        ParametricSketchFactory::addCircle(sk, 5.0, 0.0, 1.5, 16u);

    // 2. Solve constraints.
    ParametricSolverReport solveReport = ParametricSolver::solve(sk.graph);
    ASSERT_TRUE(solveReport.converged);

    // 3. Build a profile from the solved circle points.
    std::vector<Vec3> cps;
    for (auto id : circlePts) {
        const auto* p = sk.graph.point(id);
        ASSERT_NE(p, nullptr);
        cps.emplace_back(static_cast<float>(p->x), static_cast<float>(p->y), 0.f);
    }
    // Close the loop by adding the first CP at the end.
    cps.push_back(cps[0]);

    int32_t nCPs = static_cast<int32_t>(cps.size());
    int32_t deg = std::min(3, nCPs - 1);
    std::vector<float> knots(static_cast<size_t>(nCPs + deg + 1));
    for (int32_t j = 0; j <= deg; ++j) knots[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nCPs - deg; ++j)
        knots[static_cast<size_t>(deg + j)] = static_cast<float>(j) / static_cast<float>(nCPs - deg);
    for (int32_t j = 0; j <= deg; ++j) knots[knots.size() - 1 - static_cast<size_t>(j)] = 1.f;

    NurbsCurve profile(deg, std::move(knots), std::move(cps));

    // 4. Revolve around Z axis. The circle at radius 5 produces a torus.
    RevolveDesc revolveDesc;
    revolveDesc.axisOrigin    = {0.f, 0.f, 0.f};
    revolveDesc.axisDirection = {0.f, 0.f, 1.f};
    revolveDesc.angularSamples = 24;

    Mesh mesh;
    NurbsSurface surf;
    RevolveReport revRep = RevolveOperation::revolve(profile, revolveDesc, surf, &mesh);

    EXPECT_EQ(revRep.diagnostic, RevolveDiagnostic::Success);

    // 5. All points should have radius between 3.5 and 6.5 (5 ± 1.5).
    const auto& pos = mesh.attributes().positions();
    for (size_t i = 0; i < pos.size(); ++i) {
        float r = std::sqrt(pos[i].x * pos[i].x + pos[i].y * pos[i].y);
        EXPECT_GE(r, 3.3f);
        EXPECT_LE(r, 6.7f);
    }
}

// ── Hardening: invalid input validation ────────────────────────────────────

TEST(ParametricSketch, AddRectangleRejectsNonPositiveDimensions)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto [a, b, c, d] = ParametricSketchFactory::addRectangle(sk, 0, 0, 0, 5);
    EXPECT_EQ(a, kInvalidEntityId);
    EXPECT_EQ(b, kInvalidEntityId);

    std::tie(a, b, c, d) = ParametricSketchFactory::addRectangle(sk, 0, 0, 3, -1);
    EXPECT_EQ(a, kInvalidEntityId);
}

TEST(ParametricSketch, AddCircleRejectsNonPositiveRadius)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto pts = ParametricSketchFactory::addCircle(sk, 0, 0, 0, 8u);
    EXPECT_TRUE(pts.empty());

    pts = ParametricSketchFactory::addCircle(sk, 0, 0, -5, 8u);
    EXPECT_TRUE(pts.empty());
}

TEST(ParametricSketch, AddArcRejectsInvalidInputs)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto pts = ParametricSketchFactory::addArc(sk, 0, 0, 0, 0, 90, 4u);
    EXPECT_TRUE(pts.empty());

    pts = ParametricSketchFactory::addArc(sk, 0, 0, 5, 180, 0, 4u);
    EXPECT_TRUE(pts.empty());
}

TEST(ParametricSketch, AddEllipseRejectsNonPositiveRadii)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto pts = ParametricSketchFactory::addEllipse(sk, 0, 0, 0, 3, 8u);
    EXPECT_TRUE(pts.empty());

    pts = ParametricSketchFactory::addEllipse(sk, 0, 0, 4, 0, 8u);
    EXPECT_TRUE(pts.empty());
}

TEST(ParametricSketch, AddPolylineRejectsTooFewPoints)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto     pts = ParametricSketchFactory::addPolyline(sk, {{0, 0}}, false);
    EXPECT_TRUE(pts.empty());
}

// ── Feature wrapper tests ─────────────────────────────────────────────────

TEST(ParametricFeature, ExtrudeFeatureBuilds)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk, 0.0, 0.0, 3.0, 4.0);

    ExtrudeFeature feature(sk);
    feature.setDirection({0.f, 0.f, 1.f});
    feature.setHeight(10.f);
    feature.setCapEnds(true);

    EXPECT_TRUE(feature.rebuild());
    EXPECT_TRUE(feature.valid());

    const auto* mesh = feature.mesh();
    ASSERT_NE(mesh, nullptr);
    EXPECT_GT(mesh->attributes().vertexCount(), 0u);
    EXPECT_GT(mesh->topology().faceCount(), 0u);
}

TEST(ParametricFeature, RevolveFeatureBuilds)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 3.0, 0.0);
    ParametricSketchFactory::addSketchPoint(sk, 3.0, 5.0);
    // No extra constraints — just the plane constraints from addSketchPoint.

    RevolveFeature feature(sk);
    feature.setAxis({0.f, 0.f, 0.f}, {0.f, 0.f, 1.f});
    feature.setCapEnds(true);

    EXPECT_TRUE(feature.rebuild());
    EXPECT_TRUE(feature.valid());

    const auto* mesh = feature.mesh();
    ASSERT_NE(mesh, nullptr);
    EXPECT_GT(mesh->attributes().vertexCount(), 0u);
    EXPECT_GT(mesh->topology().faceCount(), 0u);
}

TEST(ParametricFeature, RebuildAfterParameterChange)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, -1.0, 0.0);
    ParametricSketchFactory::addSketchPoint(sk, 1.0, 0.0);

    ExtrudeFeature feature(sk);
    feature.setDirection({0.f, 0.f, 1.f});
    feature.setHeight(5.f);
    feature.setHeightSamples(4);

    EXPECT_TRUE(feature.rebuild());
    auto vc1 = feature.mesh()->topology().faceCount();
    EXPECT_GT(vc1, 0u);

    feature.setHeightSamples(8);
    EXPECT_TRUE(feature.rebuild());
    auto vc2 = feature.mesh()->topology().faceCount();

    EXPECT_GT(vc2, vc1);
}

TEST(ParametricFeature, InvalidAfterNoRebuild)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 1.0, 0.0);
    ParametricSketchFactory::addSketchPoint(sk, 2.0, 0.0);

    ExtrudeFeature feature(sk);
    feature.setHeight(5.f);

    EXPECT_FALSE(feature.valid());
    EXPECT_EQ(feature.mesh(), nullptr);
}

TEST(ParametricFeature, FailsWithoutProfile)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    // No sketch points — cannot build profile.

    ExtrudeFeature feature(sk);
    feature.setHeight(5.f);

    EXPECT_FALSE(feature.rebuild());
    EXPECT_FALSE(feature.valid());
}

TEST(ParametricFeature, FailsWithInvalidExtrudeParams)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 0.0, 0.0);
    ParametricSketchFactory::addSketchPoint(sk, 3.0, 0.0);

    ExtrudeFeature feature(sk);
    feature.setDirection({0.f, 0.f, 0.f}); // zero direction vector

    EXPECT_FALSE(feature.rebuild());
    EXPECT_FALSE(feature.valid());
}
