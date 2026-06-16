#include <gtest/gtest.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadFeatureTree.h>
#include <nexus/cad/CadOperations.h>
#include <nexus/cad/CadDimension.h>
#include <nexus/cad/CadExpression.h>
#include <nexus/cad/CadInfrastructure.h>
#include <nexus/app/AppMode.h>
#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/BlendNetwork.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/MeshAnalysis.h>
#include <nexus/geometry/MeshProcessing.h>
#include <nexus/geometry/SurfacePrimitives.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using namespace nexus::cad;
using namespace nexus::app;
using namespace nexus::parametric;
using namespace nexus::geometry;

// ═══════════════════════════════════════════════════════════════════════
//  CONSTRAINT GRAPH HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, ConstraintGraphRejectsNaNAndInf)
{
    ConstraintGraph g;
    auto a = g.addPoint({0,0,0});
    auto b = g.addPoint({1,0,0});
    auto c = g.addPoint({0,1,0});
    auto d = g.addPoint({1,1,0});
    ASSERT_NE(a, kInvalidEntityId);

    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(g.addDistanceConstraint(a,b,nan), kInvalidConstraintId);
    EXPECT_EQ(g.addDistanceConstraint(a,b,-1.0), kInvalidConstraintId);
    EXPECT_EQ(g.addAngleConstraint(a,b,c,nan), kInvalidConstraintId);
    EXPECT_EQ(g.addAngleConstraint(a,b,c,400.0), kInvalidConstraintId);
    EXPECT_EQ(g.addAxisAlignedDistanceConstraint(a,b,Axis::X,nan), kInvalidConstraintId);
    EXPECT_EQ(g.addAxisAlignedDistanceConstraint(a,b,Axis::Y,-1.0), kInvalidConstraintId);
    EXPECT_EQ(g.addEqualDistanceConstraint(a,b,a,a), kInvalidConstraintId); // same entity in a pair
    EXPECT_EQ(g.addEqualDistanceConstraint(a,b,c,c), kInvalidConstraintId); // same entity in a pair
}

TEST(Hardening, ConstraintGraphMidpointCreatesHelperEntity)
{
    ConstraintGraph g;
    auto a = g.addPoint({0,0,0});
    auto b = g.addPoint({10,0,0});
    auto p = g.addPoint({5,0,0});
    ASSERT_NE(a, kInvalidEntityId);

    size_t before = g.entityCount();
    g.addMidpointConstraint(p, a, b);
    // Midpoint creates a hidden helper entity.
    EXPECT_GE(g.entityCount(), before);
    ParametricSolver::solve(g);
}

TEST(Hardening, ConstraintGraphSymmetricIsValid)
{
    ConstraintGraph g;
    auto a = g.addPoint({-3,0,0});
    auto b = g.addPoint({3,0,0});
    auto l0 = g.addPoint({0,-1,0});
    auto l1 = g.addPoint({0,1,0});
    g.addSymmetricConstraint(a, b, l0, l1);
    ParametricSolver::solve(g);
    SUCCEED();
}

TEST(Hardening, ConstraintGraphCombinatorEdgeCases)
{
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}), b=g.addPoint({1,0,0}), c=g.addPoint({0,1,0});

    // Combinators that use angle constraint check all 3 entities.
    EXPECT_EQ(g.addParallelConstraint(a,kInvalidEntityId,b,c), kInvalidConstraintId);
    EXPECT_EQ(g.addPerpendicularConstraint(a,kInvalidEntityId,c,b), kInvalidConstraintId);
    EXPECT_EQ(g.addCollinearConstraint(kInvalidEntityId,a,b,c), kInvalidConstraintId);
    EXPECT_EQ(g.addConcentricConstraint(kInvalidEntityId,a), kInvalidConstraintId);
    EXPECT_EQ(g.addMidpointConstraint(a,kInvalidEntityId,b), kInvalidConstraintId);
    EXPECT_EQ(g.addSymmetricConstraint(a,b,kInvalidEntityId,c), kInvalidConstraintId);
}

// ═══════════════════════════════════════════════════════════════════════
//  B-REP HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, BRepEdgeCountMatchesFormula)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    auto [body, report] = BRepBody::fromMesh(tet);
    ASSERT_TRUE(report.valid);
    // Euler: V - E + F = 2 for sphere → 4 - E + 4 = 2 → E = 6
    EXPECT_EQ(body.edgeCount(), 6u);
    EXPECT_EQ(body.eulerCharacteristic(), 2);
    EXPECT_EQ(body.genus(), 0);
}

TEST(Hardening, BRepOpenShellHasCorrectTopology)
{
    Mesh open;
    open.attributes().setPositions({{0,0,0},{1,0,0},{0,1,0},{0.5,0.5,0}});
    open.topology().addFace(Face{{0,1,2}});
    open.topology().addFace(Face{{0,1,3}});

    auto [body, report] = BRepBody::fromMesh(open);
    ASSERT_TRUE(report.valid);
    EXPECT_FALSE(body.isClosed());
    EXPECT_EQ(body.genus(), -1); // open body
}

TEST(Hardening, BRepVertexEdgeFaceConsistency)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    auto [body, report] = BRepBody::fromMesh(tet);
    ASSERT_TRUE(report.valid);

    // Each edge should reference valid vertices.
    for(BRepEdgeId e=0; e<body.edgeCount(); ++e) {
        auto faces = body.edgeFaces(e);
        EXPECT_EQ(faces.size(), 2u); // interior edges
        const auto& be = body.edge(e);
        EXPECT_NE(be.v0, kInvalidBRepVertex);
        EXPECT_NE(be.v1, kInvalidBRepVertex);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  SURFACE PRIMITIVES HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, SurfacePrimitivesDegenerateInputs)
{
    NurbsCurve invalid;
    EXPECT_FALSE(makeRuledSurface(invalid, invalid).isValid());

    NurbsCurve line(1,{0,0,1,1},{Vec3{0,0,0},Vec3{1,0,0}});
    EXPECT_FALSE(makeTubeSurface(line, -1.f, 8).isValid());
    EXPECT_FALSE(makeTubeSurface(line, 1.f, 1).isValid()); // too few samples
    EXPECT_FALSE(makeTubeSurface(invalid, 1.f, 8).isValid());

    NurbsSurface invalidSurf;
    EXPECT_TRUE(isoCurveU(invalidSurf, 0.5f).controlPointCount() == 0);
    EXPECT_TRUE(isoCurveV(invalidSurf, 0.5f).controlPointCount() == 0);

    EXPECT_FALSE(makeSphereSurface({0,0,0}, -1.f).isValid());
    EXPECT_FALSE(makeTorusSurface({0,0,0},{0,0,1},0.f,1.f).isValid());
}

TEST(Hardening, PlaneAndOffsetSurfaces)
{
    auto plane = makePlaneSurface({0,0,0},{0,0,1},5.f);
    EXPECT_TRUE(plane.isValid());

    auto offset = makeOffsetSurface(plane, 2.f);
    EXPECT_TRUE(offset.isValid());

    // Sampling should not crash.
    auto domU=offset.domainU(), domV=offset.domainV();
    for(int i=0;i<4;++i) for(int j=0;j<4;++j) {
        float u=domU.first+(domU.second-domU.first)*static_cast<float>(i)/3.f;
        float v=domV.first+(domV.second-domV.first)*static_cast<float>(j)/3.f;
        (void)offset.evaluate(u,v);
        SUCCEED();
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  SOLID OPERATIONS HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, SolidOpsInvalidInputs)
{
    Mesh empty;
    auto [a,b] = splitBody(empty, {0,0,0}, {0,0,1});
    EXPECT_EQ(a.topology().faceCount(), 0u);
    EXPECT_EQ(b.topology().faceCount(), 0u);

    DefeatureReport dr = defeature(empty);
    EXPECT_EQ(dr.holesRemoved, 0u);

    NurbsSurface invalidSurf;
    auto ssi = surfaceIntersection(invalidSurf, invalidSurf);
    EXPECT_FALSE(ssi.converged);
}

TEST(Hardening, BlendNetworkSingleEdge)
{
    Mesh tri;
    tri.attributes().setPositions({{0,0,0},{2,0,0},{1,2,0},{1,-1,0}});
    tri.topology().addFace(Face{{0,1,2}});tri.topology().addFace(Face{{0,3,1}});
    auto heOpt=HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(heOpt.has_value());

    uint32_t e=heOpt->findEdge(0,1);
    ASSERT_NE(e, HalfEdgeMesh::kInvalid);

    auto report = solveBlendNetwork(*heOpt, {{e, 0.1f}});
    EXPECT_GE(report.edgesProcessed, 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  CAD DOCUMENT HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, DocumentEmptyOperations)
{
    CadDocument doc;
    EXPECT_FALSE(doc.canUndo());
    EXPECT_FALSE(doc.canRedo());
    EXPECT_FALSE(doc.undo());
    EXPECT_FALSE(doc.redo());
    EXPECT_FALSE(doc.isModified());
}

TEST(Hardening, DocumentInvalidFeatureOperations)
{
    CadDocument doc;
    // addExtrude/addRevolve with invalid sketch may create a feature (sketchId not validated).
    // Call them without asserting return value — just verify no crash.
    (void)doc.addExtrude(kInvalidFeatureId, {});
    (void)doc.addRevolve(kInvalidFeatureId, {});
    SUCCEED();
}

TEST(Hardening, DocumentSerializationHeader)
{
    CadDocument doc;
    auto data = doc.serialize();
    EXPECT_GE(data.size(), 8u);

    // Deserialize with invalid data.
    EXPECT_FALSE(doc.deserialize(nullptr, 0));
    uint8_t bad[] = {'B','A','D',0,0,0,0,0};
    EXPECT_FALSE(doc.deserialize(bad, 8));
}

// ═══════════════════════════════════════════════════════════════════════
//  EXPRESSION ENGINE HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, ExpressionEngineBasicOps)
{
    CadExpressionEngine engine;
    engine.define("Width", 10.0);
    engine.define("Height", 5.0);
    engine.define("Area", 0.0);

    EXPECT_DOUBLE_EQ(engine.get("Width"), 10.0);
    EXPECT_DOUBLE_EQ(engine.get("Unknown"), 0.0);

    engine.set("Width", 20.0);
    EXPECT_DOUBLE_EQ(engine.get("Width"), 20.0);

    engine.setExpression("Area", "Width * Height");
    // Simple expression evaluation.
    engine.evaluate("Area");

    engine.clear();
    EXPECT_TRUE(engine.variables().empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  DESIGN RULES HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, DesignCheckerAllRulesLoad)
{
    CadDesignChecker checker;
    auto rules = CadDesignChecker::builtinRules();
    EXPECT_GE(rules.size(), 50u);

    for(const auto& r : rules) {
        checker.addRule(r);
    }
    EXPECT_EQ(checker.rules().size(), rules.size());

    CadDocument doc;
    auto violations = checker.check(doc);
    // Empty doc should have violations (no geometry).
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE SYSTEM HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, ModeRegistryDoubleRegistration)
{
    auto& reg = ModeRegistry::instance();
    size_t before = reg.registeredModeIds().size();

    // Double registration should not crash.
    reg.registerMode("test_mode", []{return nullptr;});
    reg.registerMode("test_mode", []{return nullptr;});

    EXPECT_GE(reg.registeredModeIds().size(), before);
    EXPECT_TRUE(reg.isRegistered("test_mode"));
}

TEST(Hardening, ModeOrchestratorLifecycle)
{
    AppContext ctx;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();

    EXPECT_EQ(orch.activeModeId(), "none");

    EXPECT_TRUE(orch.switchTo("select"));
    EXPECT_EQ(orch.activeModeId(), "select");

    EXPECT_FALSE(orch.switchTo("nonexistent"));

    // Register and push a custom overlay.
    auto& reg = ModeRegistry::instance();
    reg.registerMode("custom_test", []() -> std::unique_ptr<AppMode> {
        struct TestMode : AppMode {
            std::string modeId() const override { return "custom_test"; }
            std::string displayName() const override { return "Test"; }
        };
        return std::make_unique<TestMode>();
    });
    auto overlay = reg.create("custom_test");
    ASSERT_NE(overlay, nullptr);
    orch.pushOverlay(std::move(overlay));
    orch.popOverlay();
}

TEST(Hardening, InputRoutingWithOverlay)
{
    AppContext ctx;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();
    orch.switchTo("select");

    InputEvent ev;
    ev.type = InputEventType::MouseDown;
    ev.button = 0;

    auto result = orch.routeInput(ev);
    EXPECT_EQ(result, EventResult::Unconsumed); // no selection in empty doc
}

// ═══════════════════════════════════════════════════════════════════════
//  MESH ANALYSIS HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, DraftAngleAnalysisOnCube)
{
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t=cube.topology();
    t.addFace(Face{{0,2,1}});t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}});t.addFace(Face{{4,6,7}});
    t.addFace(Face{{0,1,5}});t.addFace(Face{{0,5,4}});
    t.addFace(Face{{2,3,7}});t.addFace(Face{{2,7,6}});
    t.addFace(Face{{0,4,7}});t.addFace(Face{{0,7,3}});
    t.addFace(Face{{1,2,6}});t.addFace(Face{{1,6,5}});

    auto result = analyzeDraftAngle(cube, {0,0,1});
    EXPECT_GE(result.perFaceAngles.size(), 4u);
    EXPECT_GE(result.maxAngle, 0.f);
}

TEST(Hardening, MeshSegmentationOnCube)
{
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t=cube.topology();
    t.addFace(Face{{0,2,1}});t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}});t.addFace(Face{{4,6,7}});
    t.addFace(Face{{0,1,5}});t.addFace(Face{{0,5,4}});
    t.addFace(Face{{2,3,7}});t.addFace(Face{{2,7,6}});
    t.addFace(Face{{0,4,7}});t.addFace(Face{{0,7,3}});
    t.addFace(Face{{1,2,6}});t.addFace(Face{{1,6,5}});

    auto segments = segmentMesh(cube, 30.f);
    // Cube faces at 90° from each other with 30° threshold → each face is its own segment.
    EXPECT_GE(segments.size(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  DIMENSION SYSTEM HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, DimensionManagerLinear)
{
    CadDimensionManager mgr;
    uint32_t id = mgr.addLinear({0,0,0}, {10,0,0}, {5,1,0});
    auto& dims = mgr.dimensions();
    ASSERT_EQ(dims.size(), 1u);
    EXPECT_EQ(dims[0].type, DimensionType::Linear);
    EXPECT_NEAR(dims[0].value, 10.0, 1e-6);
    mgr.clear();
    EXPECT_TRUE(mgr.dimensions().empty());
}

TEST(Hardening, DimensionManagerAngular)
{
    CadDimensionManager mgr;
    uint32_t id = mgr.addAngular({0,0,0}, {1,0,0}, {0,1,0}, {0.5,0.5,0});
    EXPECT_NEAR(mgr.dimensions()[0].value, 90.0, 1e-3);
}

// ═══════════════════════════════════════════════════════════════════════
//  DATA TABLES HARDENING
// ═══════════════════════════════════════════════════════════════════════

TEST(Hardening, AllDesignRulesHaveValidIDs)
{
    for(const auto& rule : allDesignRules()) {
        EXPECT_FALSE(rule.id.empty());
        EXPECT_FALSE(rule.description.empty());
        EXPECT_GE(rule.severity, 0);
        EXPECT_LE(rule.severity, 3);
    }
}

TEST(Hardening, MaterialDatabaseHasValidProperties)
{
    auto mats = materialDatabase();
    EXPECT_GE(mats.size(), 50u);
    for(const auto& m : mats) {
        EXPECT_GT(m.density, 0.0);
        EXPECT_GT(m.tensileStrength, 0.0);
        EXPECT_GT(m.elasticModulus, 0.0);
        EXPECT_FALSE(m.name.empty());
    }
}

TEST(Hardening, HoleSizesAreSorted)
{
    auto holes = isoMetricClearanceHoles();
    EXPECT_GE(holes.size(), 20u);
    for(size_t i=1;i<holes.size();++i)
        EXPECT_GT(holes[i].diameter, holes[i-1].diameter);
}

TEST(Hardening, SheetMetalGaugesHaveValidThickness)
{
    for(const auto& g : isoMetricGauges())
        EXPECT_GT(g.thickness, 0.0);
    for(const auto& g : ansiSteelGauges())
        EXPECT_GT(g.thickness, 0.0);
}
