#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSamples.h>
#include <nexus/parametric/ParametricSerialization.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/parametric/ParametricSceneBridge.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

using namespace nexus::parametric;
using namespace nexus::render;

namespace {

double distance3(const ParametricPoint3& a, const ParametricPoint3& b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double angleDeg3(const ParametricPoint3& a, const ParametricPoint3& b, const ParametricPoint3& c)
{
    const double abx = a.x - b.x, aby = a.y - b.y, abz = a.z - b.z;
    const double cbx = c.x - b.x, cby = c.y - b.y, cbz = c.z - b.z;
    const double abLen = std::sqrt(abx*abx + aby*aby + abz*abz);
    const double cbLen = std::sqrt(cbx*cbx + cby*cby + cbz*cbz);
    if (abLen < 1e-15 || cbLen < 1e-15) return 0.0;
    const double dot = (abx*cbx + aby*cby + abz*cbz) / (abLen * cbLen);
    return std::acos(std::clamp(dot, -1.0, 1.0)) * (180.0 / std::numbers::pi);
}

} // namespace

// ── Existing tests ──────────────────────────────────────────────────────────

TEST(ParametricFoundation, ConstraintGraphAddRemoveEntitiesAndConstraints)
{
    ConstraintGraph graph;

    const ParametricEntityId e0 = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId e1 = graph.addPoint({2.0, 0.0, 0.0});

    ASSERT_NE(e0, kInvalidEntityId);
    ASSERT_NE(e1, kInvalidEntityId);
    EXPECT_EQ(e1, e0 + 1u);
    EXPECT_TRUE(graph.hasEntity(e0));
    EXPECT_TRUE(graph.hasEntity(e1));

    const ParametricConstraintId c0 = graph.addDistanceConstraint(e0, e1, 3.0);
    ASSERT_NE(c0, kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 1u);

    EXPECT_TRUE(graph.removeEntity(e0));
    EXPECT_FALSE(graph.hasEntity(e0));
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, DistanceSolverConvergesForSingleConstraint)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 5.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);

    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    EXPECT_NEAR(distance3(*pa, *pb), 5.0, 1e-9);
}

TEST(ParametricFoundation, SerializationRoundTripPreservesGraphState)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({1.5, 2.0, -0.5});
    const ParametricEntityId b = graph.addPoint({4.5, 2.0, -0.5});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 3.0), kInvalidConstraintId);

    const std::string serialized = ParametricGraphSerializer::serialize(graph);

    ConstraintGraph restored;
    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize(serialized, restored);

    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_EQ(restored.entityCount(), graph.entityCount());
    EXPECT_EQ(restored.distanceConstraintCount(), graph.distanceConstraintCount());

    const std::string reSerialized = ParametricGraphSerializer::serialize(restored);
    EXPECT_EQ(serialized, reSerialized);
}

TEST(ParametricFoundation, ReplaySolveIsDeterministicAcrossLoads)
{
    ConstraintGraph baseline;

    const ParametricEntityId a = baseline.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = baseline.addPoint({2.0, 0.0, 0.0});
    const ParametricEntityId c = baseline.addPoint({5.0, 0.0, 0.0});

    ASSERT_NE(baseline.addDistanceConstraint(a, b, 3.0), kInvalidConstraintId);
    ASSERT_NE(baseline.addDistanceConstraint(b, c, 4.0), kInvalidConstraintId);

    const std::string initial = ParametricGraphSerializer::serialize(baseline);

    ConstraintGraph runA;
    ConstraintGraph runB;
    ASSERT_TRUE(ParametricGraphSerializer::deserialize(initial, runA).valid);
    ASSERT_TRUE(ParametricGraphSerializer::deserialize(initial, runB).valid);

    const ParametricSolverReport reportA = ParametricSolver::solve(runA);
    const ParametricSolverReport reportB = ParametricSolver::solve(runB);
    EXPECT_TRUE(reportA.errors.empty());
    EXPECT_TRUE(reportB.errors.empty());

    const std::string solvedA = ParametricGraphSerializer::serialize(runA);
    const std::string solvedB = ParametricGraphSerializer::serialize(runB);

    EXPECT_EQ(solvedA, solvedB);
}

TEST(ParametricFoundation, DeserializeRejectsInvalidHeader)
{
    ConstraintGraph graph;
    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize("BAD_HEADER\nE 1 0 0 0\n", graph);

    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.errors.empty());
}

TEST(ParametricFoundation, DeserializeRejectsNonFiniteNumericPayloads)
{
    ConstraintGraph graph;
    const std::string badData =
        "NEXUS_PARAM_GRAPH_V1\n"
        "E 1 0 0 0\n"
        "E 2 nan 0 0\n"
        "C DIST 1 1 2 inf\n";

    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize(badData, graph);

    EXPECT_FALSE(report.valid);
    EXPECT_NE(std::find(report.errors.begin(), report.errors.end(),
                        "failed to parse entity line: E 2 nan 0 0"),
              report.errors.end());
    EXPECT_NE(std::find(report.errors.begin(), report.errors.end(),
                        "failed to parse constraint line: C DIST 1 1 2 inf"),
              report.errors.end());
    EXPECT_EQ(graph.entityCount(), 1u);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, SolverErrorsAreDeterministicAndSorted)
{
    ConstraintGraph graph;
    ParametricSolverConfig config{};
    config.maxIterations = 0;

    const ParametricSolverReport reportA = ParametricSolver::solve(graph, config);
    const ParametricSolverReport reportB = ParametricSolver::solve(graph, config);

    EXPECT_FALSE(reportA.converged);
    EXPECT_FALSE(reportB.converged);
    EXPECT_EQ(reportA.errors, reportB.errors);
    EXPECT_TRUE(std::is_sorted(reportA.errors.begin(), reportA.errors.end()));
    EXPECT_TRUE(std::is_sorted(reportB.errors.begin(), reportB.errors.end()));
}

TEST(ParametricFoundation, SolverRejectsNonFiniteConvergenceEpsilon)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 2.0), kInvalidConstraintId);

    ParametricSolverConfig nanConfig{};
    nanConfig.convergenceEpsilon = std::numeric_limits<double>::quiet_NaN();
    const ParametricSolverReport nanReport = ParametricSolver::solve(graph, nanConfig);
    EXPECT_FALSE(nanReport.converged);
    EXPECT_NE(std::find(nanReport.errors.begin(), nanReport.errors.end(),
                        "convergenceEpsilon must be finite"),
              nanReport.errors.end());

    ParametricSolverConfig infConfig{};
    infConfig.convergenceEpsilon = std::numeric_limits<double>::infinity();
    const ParametricSolverReport infReport = ParametricSolver::solve(graph, infConfig);
    EXPECT_FALSE(infReport.converged);
    EXPECT_NE(std::find(infReport.errors.begin(), infReport.errors.end(),
                        "convergenceEpsilon must be finite"),
              infReport.errors.end());
}

TEST(ParametricFoundation, SerializationErrorsAreDeterministicAndSorted)
{
    ConstraintGraph graphA;
    ConstraintGraph graphB;

    const std::string badData =
        "NEXUS_PARAM_GRAPH_V1\n"
        "C BADKIND 0 0 1 2\n"
        "Q invalid_payload\n"
        "E broken_entity\n";

    const ParametricSerializationReport reportA =
        ParametricGraphSerializer::deserialize(badData, graphA);
    const ParametricSerializationReport reportB =
        ParametricGraphSerializer::deserialize(badData, graphB);

    EXPECT_FALSE(reportA.valid);
    EXPECT_FALSE(reportB.valid);
    EXPECT_EQ(reportA.errors, reportB.errors);
    EXPECT_TRUE(std::is_sorted(reportA.errors.begin(), reportA.errors.end()));
    EXPECT_TRUE(std::is_sorted(reportB.errors.begin(), reportB.errors.end()));
}

TEST(ParametricFoundation, CoincidentConstraintForcesPointMatch)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({2.0, -1.0, 4.0});
    const ParametricEntityId b = graph.addPoint({10.0, 5.0, -9.0});
    ASSERT_NE(graph.addCoincidentConstraint(a, b), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    EXPECT_NEAR(pa->x, pb->x, 1e-9);
    EXPECT_NEAR(pa->y, pb->y, 1e-9);
    EXPECT_NEAR(pa->z, pb->z, 1e-9);
}

TEST(ParametricFoundation, AxisAlignedDistanceConstrainsSelectedAxisAndCollapsesOthers)
{
    ConstraintGraph graph;

    const ParametricEntityId a = graph.addPoint({1.0, 2.0, 3.0});
    const ParametricEntityId b = graph.addPoint({4.0, 5.0, 6.0});
    ASSERT_NE(graph.addAxisAlignedDistanceConstraint(a, b, Axis::Y, 7.5), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    EXPECT_NEAR(pb->y - pa->y, 7.5, 1e-9);
}

TEST(ParametricFoundation, SerializationRoundTripIncludesNewConstraintTypes)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 2.0, 3.0});
    const ParametricEntityId c = graph.addPoint({2.0, 4.0, 6.0});

    ASSERT_NE(graph.addCoincidentConstraint(a, b), kInvalidConstraintId);
    ASSERT_NE(graph.addAxisAlignedDistanceConstraint(b, c, Axis::X, 5.0), kInvalidConstraintId);

    const std::string serialized = ParametricGraphSerializer::serialize(graph);

    ConstraintGraph restored;
    const ParametricSerializationReport report =
        ParametricGraphSerializer::deserialize(serialized, restored);
    EXPECT_TRUE(report.valid);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_EQ(restored.coincidentConstraintCount(), 1u);
    EXPECT_EQ(restored.axisAlignedDistanceConstraintCount(), 1u);
}

TEST(ParametricFoundation, SketchSampleGeneratorBuildsAndSolvesRectangleLikeModel)
{
    SketchSampleModel sample = ParametricSampleGenerator::makeSketchRectangle(8.0, 3.0);

    ASSERT_NE(sample.origin, kInvalidEntityId);
    ASSERT_NE(sample.xHandle, kInvalidEntityId);
    ASSERT_NE(sample.yHandle, kInvalidEntityId);
    ASSERT_NE(sample.corner, kInvalidEntityId);

    const ParametricSolverReport report = ParametricSampleGenerator::solveSketchRectangle(sample);
    EXPECT_TRUE(report.errors.empty());
    EXPECT_TRUE(report.converged);

    const ParametricPoint3* origin = sample.graph.point(sample.origin);
    const ParametricPoint3* xHandle = sample.graph.point(sample.xHandle);
    const ParametricPoint3* yHandle = sample.graph.point(sample.yHandle);
    const ParametricPoint3* corner = sample.graph.point(sample.corner);
    ASSERT_NE(origin, nullptr);
    ASSERT_NE(xHandle, nullptr);
    ASSERT_NE(yHandle, nullptr);
    ASSERT_NE(corner, nullptr);

    // All axis-aligned distance constraints must be satisfied.
    // origin->xHandle along X = 8, origin->yHandle along Y = 3,
    // xHandle->corner along Y = 3, yHandle->corner along X = 8.
    EXPECT_NEAR(xHandle->x - origin->x, 8.0, 1e-9);
    EXPECT_NEAR(yHandle->y - origin->y, 3.0, 1e-9);
    EXPECT_NEAR(corner->y - xHandle->y, 3.0, 1e-9);
    EXPECT_NEAR(corner->x - yHandle->x, 8.0, 1e-9);
}

TEST(ParametricFoundation, SketchSampleGeneratorRejectsNonFiniteRectangleDimensions)
{
    const SketchSampleModel nanWidth =
        ParametricSampleGenerator::makeSketchRectangle(std::numeric_limits<double>::quiet_NaN(), 3.0);
    EXPECT_EQ(nanWidth.origin, kInvalidEntityId);
    EXPECT_EQ(nanWidth.xHandle, kInvalidEntityId);
    EXPECT_EQ(nanWidth.yHandle, kInvalidEntityId);
    EXPECT_EQ(nanWidth.corner, kInvalidEntityId);
    EXPECT_EQ(nanWidth.graph.entityCount(), 0u);

    const SketchSampleModel infHeight =
        ParametricSampleGenerator::makeSketchRectangle(8.0, std::numeric_limits<double>::infinity());
    EXPECT_EQ(infHeight.origin, kInvalidEntityId);
    EXPECT_EQ(infHeight.xHandle, kInvalidEntityId);
    EXPECT_EQ(infHeight.yHandle, kInvalidEntityId);
    EXPECT_EQ(infHeight.corner, kInvalidEntityId);
    EXPECT_EQ(infHeight.graph.entityCount(), 0u);
}

TEST(ParametricFoundation, SketchSampleSolverRejectsZeroIterationConfigAtEntry)
{
    SketchSampleModel sample = ParametricSampleGenerator::makeSketchRectangle(8.0, 3.0);
    ASSERT_NE(sample.origin, kInvalidEntityId);

    ParametricSolverConfig config{};
    config.maxIterations = 0u;

    const ParametricSolverReport report =
        ParametricSampleGenerator::solveSketchRectangle(sample, config);

    EXPECT_FALSE(report.converged);
    EXPECT_EQ(report.errors,
              std::vector<std::string>{"maxIterations must be greater than zero"});
}

TEST(ParametricFoundation, AddDistanceConstraintRejectsNaNDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(graph.addDistanceConstraint(a, b, nan), kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddDistanceConstraintRejectsPositiveInfDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(graph.addDistanceConstraint(a, b, inf), kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddDistanceConstraintRejectsNegativeDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    EXPECT_EQ(graph.addDistanceConstraint(a, b, -1.0), kInvalidConstraintId);
    EXPECT_EQ(graph.distanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddAxisAlignedDistanceConstraintRejectsNaNDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(graph.addAxisAlignedDistanceConstraint(a, b, Axis::X, nan), kInvalidConstraintId);
    EXPECT_EQ(graph.axisAlignedDistanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddAxisAlignedDistanceConstraintRejectsPositiveInfDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(graph.addAxisAlignedDistanceConstraint(a, b, Axis::Y, inf), kInvalidConstraintId);
    EXPECT_EQ(graph.axisAlignedDistanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddAxisAlignedDistanceConstraintRejectsNegativeDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    EXPECT_EQ(graph.addAxisAlignedDistanceConstraint(a, b, Axis::Z, -5.0), kInvalidConstraintId);
    EXPECT_EQ(graph.axisAlignedDistanceConstraintCount(), 0u);
}

TEST(ParametricFoundation, AddPointRejectsNonFiniteCoordinates)
{
    ConstraintGraph graph;

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(graph.addPoint({nan, 0.0, 0.0}), kInvalidEntityId);
    EXPECT_EQ(graph.addPoint({0.0, inf, 0.0}), kInvalidEntityId);
    EXPECT_EQ(graph.entityCount(), 0u);
}

TEST(ParametricFoundation, SetPointRejectsNonFiniteCoordinates)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({1.0, 2.0, 3.0});
    ASSERT_NE(a, kInvalidEntityId);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    EXPECT_FALSE(graph.setPoint(a, {nan, 2.0, 3.0}));
    EXPECT_FALSE(graph.setPoint(a, {1.0, inf, 3.0}));

    const ParametricPoint3* p = graph.point(a);
    ASSERT_NE(p, nullptr);
    EXPECT_DOUBLE_EQ(p->x, 1.0);
    EXPECT_DOUBLE_EQ(p->y, 2.0);
    EXPECT_DOUBLE_EQ(p->z, 3.0);
}

// ── New: Solver ─────────────────────────────────────────────────────────────

TEST(ParametricFoundation, SymmetricSolverBothEntitiesMoveForDistanceConstraint)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({10.0, 0.0, 0.0});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 4.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const ParametricPoint3* pa = graph.point(a);
    const ParametricPoint3* pb = graph.point(b);
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);

    // Both should have moved from their initial positions.
    EXPECT_NE(pa->x, 0.0);
    EXPECT_NE(pb->x, 10.0);
    EXPECT_NEAR(distance3(*pa, *pb), 4.0, 1e-9);
}

TEST(ParametricFoundation, TriangleConstraintSolverConverges)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({10.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({5.0, 8.0, 0.0});

    ASSERT_NE(graph.addDistanceConstraint(a, b, 5.0), kInvalidConstraintId);
    ASSERT_NE(graph.addDistanceConstraint(b, c, 6.0), kInvalidConstraintId);
    ASSERT_NE(graph.addDistanceConstraint(c, a, 7.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_NEAR(distance3(*graph.point(a), *graph.point(b)), 5.0, 1e-7);
    EXPECT_NEAR(distance3(*graph.point(b), *graph.point(c)), 6.0, 1e-7);
    EXPECT_NEAR(distance3(*graph.point(c), *graph.point(a)), 7.0, 1e-7);
}

TEST(ParametricFoundation, SolverConvergesWithCoincidentAndDistance)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({5.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({3.0, 4.0, 0.0});
    const ParametricEntityId d = graph.addPoint({1.0, 1.0, 1.0});

    ASSERT_NE(graph.addCoincidentConstraint(a, d), kInvalidConstraintId);
    ASSERT_NE(graph.addDistanceConstraint(a, b, 3.0), kInvalidConstraintId);
    ASSERT_NE(graph.addDistanceConstraint(b, c, 5.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    // a and d should be coincident.
    EXPECT_NEAR(graph.point(a)->x, graph.point(d)->x, 1e-9);
    EXPECT_NEAR(graph.point(a)->y, graph.point(d)->y, 1e-9);

    // Distances should hold.
    EXPECT_NEAR(distance3(*graph.point(a), *graph.point(b)), 3.0, 1e-7);
    EXPECT_NEAR(distance3(*graph.point(b), *graph.point(c)), 5.0, 1e-7);
}

// ── New: Angle constraint ───────────────────────────────────────────────────

TEST(ParametricFoundation, AddAngleConstraintValidatesInput)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({0.0, 1.0, 0.0});

    EXPECT_NE(graph.addAngleConstraint(a, b, c, 90.0), kInvalidConstraintId);
    EXPECT_EQ(graph.angleConstraintCount(), 1u);

    // Rejects self-referencing.
    EXPECT_EQ(graph.addAngleConstraint(a, a, b, 90.0), kInvalidConstraintId);
    // Rejects NaN.
    EXPECT_EQ(graph.addAngleConstraint(a, b, c, std::numeric_limits<double>::quiet_NaN()),
              kInvalidConstraintId);
    // Rejects out-of-range.
    EXPECT_EQ(graph.addAngleConstraint(a, b, c, 400.0), kInvalidConstraintId);
    EXPECT_EQ(graph.addAngleConstraint(a, b, c, -361.0), kInvalidConstraintId);
}

TEST(ParametricFoundation, AngleConstraintConvergesToRightAngle)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({1.0, 0.5, 0.0});

    ASSERT_NE(graph.addAngleConstraint(a, b, c, 90.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const double angle = angleDeg3(*graph.point(a), *graph.point(b), *graph.point(c));
    EXPECT_NEAR(angle, 90.0, 1e-6);
}

TEST(ParametricFoundation, AngleConstraintConvergesToAcuteAngle)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({-1.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({-0.6, 0.5, 0.0});

    ASSERT_NE(graph.addAngleConstraint(a, b, c, 30.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_NEAR(angleDeg3(*graph.point(a), *graph.point(b), *graph.point(c)), 30.0, 1e-6);
}

TEST(ParametricFoundation, AngleConstraintSerializationRoundTrip)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({0.0, 1.0, 0.0});
    ASSERT_NE(graph.addAngleConstraint(a, b, c, 45.0), kInvalidConstraintId);

    const std::string ser = ParametricGraphSerializer::serialize(graph);
    ConstraintGraph restored;
    const auto report = ParametricGraphSerializer::deserialize(ser, restored);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(restored.angleConstraintCount(), 1u);
    EXPECT_EQ(ser, ParametricGraphSerializer::serialize(restored));
}

// ── New: Equal-distance constraint ──────────────────────────────────────────

TEST(ParametricFoundation, AddEqualDistanceConstraintValidatesInput)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({2.0, 0.0, 0.0});
    const ParametricEntityId d = graph.addPoint({3.0, 0.0, 0.0});

    EXPECT_NE(graph.addEqualDistanceConstraint(a, b, c, d), kInvalidConstraintId);
    EXPECT_EQ(graph.equalDistanceConstraintCount(), 1u);

    // Same-pair self-reference: A==B rejected.
    EXPECT_EQ(graph.addEqualDistanceConstraint(a, a, c, d), kInvalidConstraintId);
    // Missing entity.
    EXPECT_EQ(graph.addEqualDistanceConstraint(a, b, c, kInvalidEntityId), kInvalidConstraintId);
}

TEST(ParametricFoundation, EqualDistanceConstraintMakesDistancesMatch)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({3.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({5.0, 0.0, 0.0});
    const ParametricEntityId d = graph.addPoint({5.0, 4.0, 0.0});

    // |a-b| = 3, |c-d| = 4 — equal-distance should scale one pair.
    ASSERT_NE(graph.addEqualDistanceConstraint(a, b, c, d), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const double d1 = distance3(*graph.point(a), *graph.point(b));
    const double d2 = distance3(*graph.point(c), *graph.point(d));
    EXPECT_NEAR(d1, d2, 1e-9);
}

TEST(ParametricFoundation, EqualDistanceConstraintSerializationRoundTrip)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({2.0, 0.0, 0.0});
    const ParametricEntityId d = graph.addPoint({3.0, 0.0, 0.0});
    ASSERT_NE(graph.addEqualDistanceConstraint(a, b, c, d), kInvalidConstraintId);

    const std::string ser = ParametricGraphSerializer::serialize(graph);
    ConstraintGraph restored;
    const auto report = ParametricGraphSerializer::deserialize(ser, restored);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(restored.equalDistanceConstraintCount(), 1u);
    EXPECT_EQ(ser, ParametricGraphSerializer::serialize(restored));
}

// ── New: Point-on-line constraint ────────────────────────────────────────

TEST(ParametricFoundation, AddPointOnLineConstraintValidatesInput)
{
    ConstraintGraph graph;
    const ParametricEntityId p = graph.addPoint({5.0, 3.0, 0.0});
    const ParametricEntityId l0 = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId l1 = graph.addPoint({1.0, 0.0, 0.0});

    EXPECT_NE(graph.addPointOnLineConstraint(p, l0, l1), kInvalidConstraintId);
    EXPECT_EQ(graph.pointOnLineConstraintCount(), 1u);

    // Rejects self-referencing.
    EXPECT_EQ(graph.addPointOnLineConstraint(p, p, l1), kInvalidConstraintId);
    EXPECT_EQ(graph.addPointOnLineConstraint(p, l0, p), kInvalidConstraintId);
    EXPECT_EQ(graph.addPointOnLineConstraint(l0, l0, l1), kInvalidConstraintId);
    // Rejects missing entity.
    EXPECT_EQ(graph.addPointOnLineConstraint(p, l0, kInvalidEntityId), kInvalidConstraintId);
    // Rejects degenerate line (same endpoints).
    EXPECT_EQ(graph.addPointOnLineConstraint(p, l0, l0), kInvalidConstraintId);
}

TEST(ParametricFoundation, PointOnLineConstraintConvergesForPointOffLine)
{
    ConstraintGraph graph;
    const ParametricEntityId p = graph.addPoint({4.0, 3.0, 0.0});
    const ParametricEntityId l0 = graph.addPoint({0.0, 1.0, 0.0});
    const ParametricEntityId l1 = graph.addPoint({2.0, 1.0, 0.0});

    ASSERT_NE(graph.addPointOnLineConstraint(p, l0, l1), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    // P, L0, L1 should be collinear (P projects onto line L0-L1).
    const auto* pp = graph.point(p);
    const auto* pl0 = graph.point(l0);
    const auto* pl1 = graph.point(l1);
    ASSERT_NE(pp, nullptr);
    ASSERT_NE(pl0, nullptr);
    ASSERT_NE(pl1, nullptr);

    const double ex = (pp->y - pl0->y) * (pl1->z - pl0->z) - (pp->z - pl0->z) * (pl1->y - pl0->y);
    const double ey = (pp->z - pl0->z) * (pl1->x - pl0->x) - (pp->x - pl0->x) * (pl1->z - pl0->z);
    const double ez = (pp->x - pl0->x) * (pl1->y - pl0->y) - (pp->y - pl0->y) * (pl1->x - pl0->x);
    EXPECT_NEAR(std::sqrt(ex*ex + ey*ey + ez*ez), 0.0, 1e-8);
}

TEST(ParametricFoundation, PointOnLineConstraintConvergesIn3D)
{
    ConstraintGraph graph;
    const ParametricEntityId p = graph.addPoint({1.0, 5.0, 3.0});
    const ParametricEntityId l0 = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId l1 = graph.addPoint({2.0, 2.0, 2.0});

    ASSERT_NE(graph.addPointOnLineConstraint(p, l0, l1), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    // P should be collinear with L0 and L1: (P - L0) × (L1 - L0) = 0.
    const auto* pp = graph.point(p);
    const auto* pl0 = graph.point(l0);
    const auto* pl1 = graph.point(l1);
    ASSERT_NE(pp, nullptr);
    ASSERT_NE(pl0, nullptr);
    ASSERT_NE(pl1, nullptr);

    const double ex = (pp->y - pl0->y) * (pl1->z - pl0->z) - (pp->z - pl0->z) * (pl1->y - pl0->y);
    const double ey = (pp->z - pl0->z) * (pl1->x - pl0->x) - (pp->x - pl0->x) * (pl1->z - pl0->z);
    const double ez = (pp->x - pl0->x) * (pl1->y - pl0->y) - (pp->y - pl0->y) * (pl1->x - pl0->x);
    EXPECT_NEAR(std::sqrt(ex*ex + ey*ey + ez*ez), 0.0, 1e-8);
}

TEST(ParametricFoundation, PointOnLineConstraintSerializationRoundTrip)
{
    ConstraintGraph graph;
    const ParametricEntityId p = graph.addPoint({3.0, 4.0, 0.0});
    const ParametricEntityId l0 = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId l1 = graph.addPoint({1.0, 0.0, 0.0});
    ASSERT_NE(graph.addPointOnLineConstraint(p, l0, l1), kInvalidConstraintId);

    const std::string ser = ParametricGraphSerializer::serialize(graph);
    ConstraintGraph restored;
    const auto report = ParametricGraphSerializer::deserialize(ser, restored);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(restored.pointOnLineConstraintCount(), 1u);
    EXPECT_EQ(ser, ParametricGraphSerializer::serialize(restored));
}

// ── New: Cascade removal on entity delete ───────────────────────────────────

TEST(ParametricFoundation, RemoveEntityCascadesToNewConstraintTypes)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({2.0, 0.0, 0.0});
    const ParametricEntityId d = graph.addPoint({3.0, 0.0, 0.0});

    ASSERT_NE(graph.addAngleConstraint(a, b, c, 45.0), kInvalidConstraintId);
    ASSERT_NE(graph.addEqualDistanceConstraint(a, b, c, d), kInvalidConstraintId);
    ASSERT_NE(graph.addSketchPlaneConstraint(b, d), kInvalidConstraintId);

    EXPECT_TRUE(graph.removeEntity(b));
    EXPECT_EQ(graph.angleConstraintCount(), 0u);
    EXPECT_EQ(graph.equalDistanceConstraintCount(), 0u);
    EXPECT_EQ(graph.sketchPlaneConstraintCount(), 0u);
}

// ── New: Remove new constraint types ────────────────────────────────────────

TEST(ParametricFoundation, RemoveAngleConstraint)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({0.0, 1.0, 0.0});
    const ParametricConstraintId id = graph.addAngleConstraint(a, b, c, 60.0);
    ASSERT_NE(id, kInvalidConstraintId);
    EXPECT_EQ(graph.angleConstraintCount(), 1u);

    EXPECT_TRUE(graph.removeConstraint(id));
    EXPECT_EQ(graph.angleConstraintCount(), 0u);
    EXPECT_FALSE(graph.hasConstraint(id));
}

TEST(ParametricFoundation, RemoveEqualDistanceConstraint)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({2.0, 0.0, 0.0});
    const ParametricEntityId d = graph.addPoint({3.0, 0.0, 0.0});
    const ParametricConstraintId id = graph.addEqualDistanceConstraint(a, b, c, d);
    ASSERT_NE(id, kInvalidConstraintId);
    EXPECT_EQ(graph.equalDistanceConstraintCount(), 1u);

    EXPECT_TRUE(graph.removeConstraint(id));
    EXPECT_EQ(graph.equalDistanceConstraintCount(), 0u);
    EXPECT_FALSE(graph.hasConstraint(id));
}

TEST(ParametricFoundation, RemovePointOnLineConstraint)
{
    ConstraintGraph graph;
    const ParametricEntityId p = graph.addPoint({1.0, 2.0, 3.0});
    const ParametricEntityId l0 = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId l1 = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricConstraintId id = graph.addPointOnLineConstraint(p, l0, l1);
    ASSERT_NE(id, kInvalidConstraintId);
    EXPECT_EQ(graph.pointOnLineConstraintCount(), 1u);

    EXPECT_TRUE(graph.removeConstraint(id));
    EXPECT_EQ(graph.pointOnLineConstraintCount(), 0u);
    EXPECT_FALSE(graph.hasConstraint(id));
}

// ── New: Sketch-plane constraint ──────────────────────────────────────────

TEST(ParametricFoundation, AddSketchPlaneConstraintValidatesInput)
{
    ConstraintGraph graph;
    const ParametricEntityId plane = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId pt = graph.addPoint({5.0, 3.0, 7.0});

    EXPECT_NE(graph.addSketchPlaneConstraint(plane, pt), kInvalidConstraintId);
    EXPECT_EQ(graph.sketchPlaneConstraintCount(), 1u);

    // Rejects self-referencing.
    EXPECT_EQ(graph.addSketchPlaneConstraint(plane, plane), kInvalidConstraintId);
    // Rejects missing entity.
    EXPECT_EQ(graph.addSketchPlaneConstraint(plane, kInvalidEntityId), kInvalidConstraintId);
    EXPECT_EQ(graph.addSketchPlaneConstraint(kInvalidEntityId, pt), kInvalidConstraintId);
}

TEST(ParametricFoundation, SketchPlaneConstraintForcesZMatch)
{
    ConstraintGraph graph;
    const ParametricEntityId plane = graph.addPoint({8.0, 2.0, 0.0});
    const ParametricEntityId pt = graph.addPoint({4.0, 6.0, 9.0});

    ASSERT_NE(graph.addSketchPlaneConstraint(plane, pt), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    // Point Z should match plane Z (both move toward midpoint with symmetric).
    // The constraint is satisfied when the point lies on the plane (Z=0).
    // Verify that the point's Z is close to the plane's Z.
    const auto* pplane = graph.point(plane);
    const auto* ppoint = graph.point(pt);
    ASSERT_NE(pplane, nullptr);
    ASSERT_NE(ppoint, nullptr);
    EXPECT_NEAR(ppoint->z, pplane->z, 1e-8);
}

TEST(ParametricFoundation, SketchPlaneConstraintSerializationRoundTrip)
{
    ConstraintGraph graph;
    const ParametricEntityId plane = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId pt = graph.addPoint({3.0, 4.0, 0.0});
    ASSERT_NE(graph.addSketchPlaneConstraint(plane, pt), kInvalidConstraintId);

    const std::string ser = ParametricGraphSerializer::serialize(graph);
    ConstraintGraph restored;
    const auto report = ParametricGraphSerializer::deserialize(ser, restored);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(restored.sketchPlaneConstraintCount(), 1u);
    EXPECT_EQ(ser, ParametricGraphSerializer::serialize(restored));
}

TEST(ParametricFoundation, RemoveSketchPlaneConstraint)
{
    ConstraintGraph graph;
    const ParametricEntityId plane = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId pt = graph.addPoint({1.0, 2.0, 3.0});
    const ParametricConstraintId id = graph.addSketchPlaneConstraint(plane, pt);
    ASSERT_NE(id, kInvalidConstraintId);
    EXPECT_EQ(graph.sketchPlaneConstraintCount(), 1u);

    EXPECT_TRUE(graph.removeConstraint(id));
    EXPECT_EQ(graph.sketchPlaneConstraintCount(), 0u);
    EXPECT_FALSE(graph.hasConstraint(id));
}

// ── New: Total constraint count ─────────────────────────────────────────────

TEST(ParametricFoundation, TotalConstraintCount)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});
    const ParametricEntityId c = graph.addPoint({2.0, 0.0, 0.0});

    EXPECT_EQ(graph.totalConstraintCount(), 0u);
    graph.addDistanceConstraint(a, b, 3.0);
    EXPECT_EQ(graph.totalConstraintCount(), 1u);
    graph.addAngleConstraint(a, b, c, 90.0);
    EXPECT_EQ(graph.totalConstraintCount(), 2u);
    graph.addCoincidentConstraint(a, c);
    EXPECT_EQ(graph.totalConstraintCount(), 3u);
    graph.addSketchPlaneConstraint(b, c);
    EXPECT_EQ(graph.totalConstraintCount(), 4u);
}

// ── New: DOF analysis ───────────────────────────────────────────────────────

TEST(ParametricFoundation, DegreesOfFreedomAnalysis)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    DegreesOfFreedom dof0 = graph.analyzeDegreesOfFreedom();
    EXPECT_EQ(dof0.freeVariables, 6);
    EXPECT_EQ(dof0.effectiveConstraints, 0);
    EXPECT_EQ(dof0.estimatedRemainingDOF, 6);
    EXPECT_FALSE(dof0.likelyOverconstrained);

    graph.addDistanceConstraint(a, b, 3.0);
    DegreesOfFreedom dof1 = graph.analyzeDegreesOfFreedom();
    EXPECT_EQ(dof1.freeVariables, 6);
    EXPECT_EQ(dof1.effectiveConstraints, 1);
    EXPECT_EQ(dof1.estimatedRemainingDOF, 5);
    EXPECT_FALSE(dof1.likelyOverconstrained);

    graph.addCoincidentConstraint(a, b);
    DegreesOfFreedom dof2 = graph.analyzeDegreesOfFreedom();
    EXPECT_EQ(dof2.freeVariables, 6);
    EXPECT_EQ(dof2.effectiveConstraints, 4);
    EXPECT_EQ(dof2.estimatedRemainingDOF, 2);
    EXPECT_FALSE(dof2.likelyOverconstrained);
}

TEST(ParametricFoundation, DegreesOfFreedomDetectsOverconstrained)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({1.0, 0.0, 0.0});

    // 6 DOF with coincident (3) + 3 distance (1 each) = 6 effective > 6 DOF.
    graph.addCoincidentConstraint(a, b);
    graph.addDistanceConstraint(a, b, 1.0);
    graph.addDistanceConstraint(a, b, 2.0);
    graph.addDistanceConstraint(a, b, 3.0);

    DegreesOfFreedom dof = graph.analyzeDegreesOfFreedom();
    EXPECT_EQ(dof.freeVariables, 6);
    EXPECT_EQ(dof.effectiveConstraints, 6);
    EXPECT_EQ(dof.estimatedRemainingDOF, 0);
    EXPECT_FALSE(dof.likelyOverconstrained);

    // Add one more distance to push it over.
    graph.addDistanceConstraint(a, b, 4.0);
    dof = graph.analyzeDegreesOfFreedom();
    EXPECT_TRUE(dof.likelyOverconstrained);
    EXPECT_EQ(dof.estimatedRemainingDOF, -1);
}

// ── New: Scene graph integration ────────────────────────────────────────────

TEST(ParametricBridge, ApplySolutionToNodePositions)
{
    ConstraintGraph graph;
    const ParametricEntityId id1 = graph.addPoint({2.5, 3.0, -1.0});
    const ParametricEntityId id2 = graph.addPoint({7.0, 8.0, 9.0});
    ASSERT_NE(id1, kInvalidEntityId);
    ASSERT_NE(id2, kInvalidEntityId);

    SceneGraph scene;
    Node* node1 = scene.createNode("bound1");
    Node* node2 = scene.createNode("bound2");
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    node1->parametricBindingId = id1;
    node2->parametricBindingId = id2;

    applySolutionToNodePositions(graph, scene);

    EXPECT_FLOAT_EQ(node1->localTransform().translation.x, 2.5f);
    EXPECT_FLOAT_EQ(node1->localTransform().translation.y, 3.0f);
    EXPECT_FLOAT_EQ(node1->localTransform().translation.z, -1.0f);

    EXPECT_FLOAT_EQ(node2->localTransform().translation.x, 7.0f);
    EXPECT_FLOAT_EQ(node2->localTransform().translation.y, 8.0f);
    EXPECT_FLOAT_EQ(node2->localTransform().translation.z, 9.0f);
}

TEST(ParametricBridge, UnboundNodePositionUnaffected)
{
    ConstraintGraph graph;
    const ParametricEntityId id = graph.addPoint({5.0, -2.0, 3.0});
    ASSERT_NE(id, kInvalidEntityId);

    SceneGraph scene;
    Node* node = scene.createNode("unbound");
    ASSERT_NE(node, nullptr);
    node->localTransform().translation = {100.0f, 200.0f, 300.0f};
    // No parametricBindingId set.

    applySolutionToNodePositions(graph, scene);

    EXPECT_FLOAT_EQ(node->localTransform().translation.x, 100.0f);
    EXPECT_FLOAT_EQ(node->localTransform().translation.y, 200.0f);
    EXPECT_FLOAT_EQ(node->localTransform().translation.z, 300.0f);
}

TEST(ParametricBridge, ApplySolutionAfterSolverSyncsPositions)
{
    ConstraintGraph graph;
    const ParametricEntityId a = graph.addPoint({0.0, 0.0, 0.0});
    const ParametricEntityId b = graph.addPoint({10.0, 0.0, 0.0});
    ASSERT_NE(graph.addDistanceConstraint(a, b, 3.0), kInvalidConstraintId);

    const ParametricSolverReport report = ParametricSolver::solve(graph);
    ASSERT_TRUE(report.converged);

    SceneGraph scene;
    Node* nodeA = scene.createNode("ptA");
    Node* nodeB = scene.createNode("ptB");
    ASSERT_NE(nodeA, nullptr);
    ASSERT_NE(nodeB, nullptr);
    nodeA->parametricBindingId = a;
    nodeB->parametricBindingId = b;

    applySolutionToNodePositions(graph, scene);

    const float ex = nodeA->localTransform().translation.x;
    const float ey = nodeA->localTransform().translation.y;
    const float ez = nodeA->localTransform().translation.z;
    const float bx = nodeB->localTransform().translation.x;
    const float by = nodeB->localTransform().translation.y;
    const float bz = nodeB->localTransform().translation.z;

    const float dx = bx - ex, dy = by - ey, dz = bz - ez;
    const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    EXPECT_NEAR(dist, 3.0f, 1e-5f);
}

// ── New: Sketch profile API ──────────────────────────────────────────────

TEST(ParametricSketch, CreateSketchHasPlaneAndZeroEntities)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    EXPECT_NE(sk.plane, kInvalidEntityId);
    EXPECT_EQ(sk.graph.entityCount(), 1u);
    EXPECT_TRUE(sk.points.empty());
}

TEST(ParametricSketch, AddSketchPointConstrainsToPlane)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    const ParametricEntityId pt = ParametricSketchFactory::addSketchPoint(sk, 5.0, 3.0);

    EXPECT_NE(pt, kInvalidEntityId);
    EXPECT_GE(sk.graph.sketchPlaneConstraintCount(), 1u);
    EXPECT_EQ(sk.points.size(), 1u);

    // Solve and verify the point is on the plane (Z = 0).
    const ParametricSolverReport report = ParametricSolver::solve(sk.graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    const auto* pp = sk.graph.point(pt);
    ASSERT_NE(pp, nullptr);
    EXPECT_NEAR(pp->z, sk.graph.point(sk.plane)->z, 1e-8);
}

TEST(ParametricSketch, AddLineSegmentCreatesDistanceConstraint)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    const ParametricEntityId p1 = ParametricSketchFactory::addSketchPoint(sk, 0.0, 0.0);
    const ParametricEntityId p2 = ParametricSketchFactory::addSketchPoint(sk, 3.0, 4.0);
    const ParametricConstraintId cid = ParametricSketchFactory::addLineSegment(sk, p1, p2);

    EXPECT_NE(cid, kInvalidConstraintId);
    EXPECT_GE(sk.graph.distanceConstraintCount(), 1u);
}

TEST(ParametricSketch, AddRectangleCreatesFourCornersWithConstraints)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto [origin, xHandle, yHandle, corner] =
        ParametricSketchFactory::addRectangle(sk, 1.0, 2.0, 8.0, 3.0);

    EXPECT_NE(origin, kInvalidEntityId);
    EXPECT_NE(xHandle, kInvalidEntityId);
    EXPECT_NE(yHandle, kInvalidEntityId);
    EXPECT_NE(corner, kInvalidEntityId);
    EXPECT_GE(sk.points.size(), 4u);
    EXPECT_GE(sk.graph.axisAlignedDistanceConstraintCount(), 4u);

    // Solve and verify the rectangle constraints hold.
    const ParametricSolverReport report = ParametricSolver::solve(sk.graph);
    EXPECT_TRUE(report.converged);
    ASSERT_TRUE(report.errors.empty());

    EXPECT_NEAR(sk.graph.point(xHandle)->x - sk.graph.point(origin)->x, 8.0, 1e-9);
    EXPECT_NEAR(sk.graph.point(yHandle)->y - sk.graph.point(origin)->y, 3.0, 1e-9);
}

TEST(ParametricSketch, AddCircleCreatesSegmentedPolygon)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    const std::vector<ParametricEntityId> pts =
        ParametricSketchFactory::addCircle(sk, 0.0, 0.0, 5.0, 16u);

    EXPECT_EQ(pts.size(), 16u);
    EXPECT_GE(sk.points.size(), 16u);
    EXPECT_GE(sk.graph.distanceConstraintCount(), 16u);

    // Each vertex should be constrained to the sketch plane.
    const size_t planeCount = sk.graph.sketchPlaneConstraintCount();
    EXPECT_GE(planeCount, 16u);
}
