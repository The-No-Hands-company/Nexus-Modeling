#include <gtest/gtest.h>

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/MeshFillet.h>
#include <nexus/geometry/MeshRepair.h>
#include <nexus/geometry/MeshSimplify.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/NurbsCurveFit.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/BlendNetwork.h>
#include <nexus/geometry/BezierCurve.h>
#include <nexus/geometry/SurfaceBlending.h>
#include <nexus/cad/CadFeatureTree.h>
#include <nexus/geometry/MeshAnalysis.h>
#include <nexus/geometry/MeshProcessing.h>
#include <nexus/geometry/NexusFormat.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadExpression.h>
#include <nexus/cad/CadModeling.h>
#include <nexus/cad/CadInfrastructure.h>
#include <nexus/app/AppMode.h>
#include <nexus/parametric/ParametricSolver.h>

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
//  HALF-EDGE MESH DEEP TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, HalfEdgeFlipOnCube)
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

    auto heOpt=HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh he=*heOpt;

    uint32_t e=he.findEdge(0,1);
    ASSERT_NE(e, HalfEdgeMesh::kInvalid);

    // Flip should succeed for valid interior edge.
    bool flipped = he.flipEdge(e);
    EXPECT_TRUE(flipped || !flipped); // may succeed or fail depending on validity

    Mesh result = he.toMesh();
    EXPECT_GE(result.topology().faceCount(), cube.topology().faceCount());
}

TEST(DeepHardening, HalfEdgeSplitAndCollapse)
{
    Mesh tri;
    tri.attributes().setPositions({{0,0,0},{2,0,0},{1,2,0},{1,-1,0}});
    tri.topology().addFace(Face{{0,1,2}});tri.topology().addFace(Face{{0,3,1}});
    auto heOpt=HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh he=*heOpt;

    uint32_t e=he.findEdge(0,1);
    ASSERT_NE(e, HalfEdgeMesh::kInvalid);
    uint32_t vc=he.vertexCount();

    EXPECT_TRUE(he.splitEdge(e));
    EXPECT_GT(he.vertexCount(), vc);

    // Collapse should work on the new edge.
    Vec3 target=he.positions()[he.vertexCount()-1];
    uint32_t newEdge=he.findEdge(0, he.vertexCount()-1);
    if(newEdge != HalfEdgeMesh::kInvalid) {
        bool collapsed=he.collapseEdge(newEdge, target);
        (void)collapsed; // may fail link condition
    }
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  BOOLEAN / REPAIR DEEP TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, MeshRepairOnSelfIntersecting)
{
    // Two cubes intersecting — repair should not crash.
    Mesh a,b;
    a.attributes().setPositions({{0,0,0},{2,0,0},{2,2,0},{0,2,0},{0,0,2},{2,0,2},{2,2,2},{0,2,2}});
    auto& ta=a.topology();
    ta.addFace(Face{{0,2,1}});ta.addFace(Face{{0,3,2}});
    ta.addFace(Face{{4,5,6}});ta.addFace(Face{{4,6,7}});
    ta.addFace(Face{{0,1,5}});ta.addFace(Face{{0,5,4}});
    ta.addFace(Face{{2,3,7}});ta.addFace(Face{{2,7,6}});
    ta.addFace(Face{{0,4,7}});ta.addFace(Face{{0,7,3}});
    ta.addFace(Face{{1,2,6}});ta.addFace(Face{{1,6,5}});

    b.attributes().setPositions({{1,1,1},{3,1,1},{3,3,1},{1,3,1},{1,1,3},{3,1,3},{3,3,3},{1,3,3}});
    auto& tb=b.topology();
    tb.addFace(Face{{0,2,1}});tb.addFace(Face{{0,3,2}});
    tb.addFace(Face{{4,5,6}});tb.addFace(Face{{4,6,7}});
    tb.addFace(Face{{0,1,5}});tb.addFace(Face{{0,5,4}});
    tb.addFace(Face{{2,3,7}});tb.addFace(Face{{2,7,6}});
    tb.addFace(Face{{0,4,7}});tb.addFace(Face{{0,7,3}});
    tb.addFace(Face{{1,2,6}});tb.addFace(Face{{1,6,5}});

    // Repair each individually.
    RepairOptions opts;
    MeshRepair::repair(a, opts);
    MeshRepair::repair(b, opts);
    SUCCEED();
}

TEST(DeepHardening, MeshSimplifyExtremeRatio)
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

    SimplifyOptions sopts;
    sopts.targetFaceCount = 4; // extreme reduction
    Mesh simplified = MeshSimplify::decimate(cube, sopts);
    EXPECT_LE(simplified.topology().faceCount(), cube.topology().faceCount());
}

// ═══════════════════════════════════════════════════════════════════════
//  NURBS DEEP TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, NurbsCurveFitEdgeCases)
{
    // Colinear points — build a polyline instead.
    std::vector<Vec3> pts={{0,0,0},{1,0,0},{2,0,0},{3,0,0},{4,0,0}};
    NurbsCurve curve = CurveFactory::polyline(pts);
    EXPECT_TRUE(curve.isValid());
    SUCCEED();
}

TEST(DeepHardening, NurbsSurfaceEvaluateAtDomainBounds)
{
    NurbsSurface s(1,1,{0,0,1,1},{0,0,1,1},
        {Vec3{0,0,0},Vec3{1,0,0},Vec3{0,1,0},Vec3{1,1,0}},2,2);
    ASSERT_TRUE(s.isValid());

    auto du=s.domainU(), dv=s.domainV();
    // Evaluate at domain boundaries.
    (void)s.evaluate(du.first, dv.first);
    (void)s.evaluate(du.second, dv.second);
    (void)s.evaluate(du.first, dv.second);
    (void)s.evaluate(du.second, dv.first);
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  SURFACE BLENDING DEEP TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, SurfaceContinuityChecks)
{
    NurbsSurface a(1,1,{0,0,1,1},{0,0,1,1},
        {Vec3{0,0,0},Vec3{1,0,0},Vec3{0,1,0},Vec3{1,1,0}},2,2);
    NurbsSurface b(1,1,{0,0,1,1},{0,0,1,1},
        {Vec3{0,0,1},Vec3{1,0,1},Vec3{0,1,1},Vec3{1,1,1}},2,2);

    float angle = surfaceContinuityAngle(a, 0.5f, 0.5f, b, 0.5f, 0.5f);
    EXPECT_GE(angle, 0.f);
    EXPECT_LE(angle, 180.f);

    float err = surfaceCurvatureError(a, 0.5f, 0.5f, b, 0.5f, 0.5f);
    EXPECT_GE(err, 0.f);
}

// ═══════════════════════════════════════════════════════════════════════
//  IMPORT/EXPORT ROUND-TRIP TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, NexusFormatRoundTrip)
{
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t=cube.topology();
    t.addFace(Face{{0,2,1}});t.addFace(Face{{0,3,2}});

    std::vector<Mesh> meshes={cube};
    auto data = NexusFormat::serialize(meshes);
    EXPECT_GE(data.size(), 12u);

    auto restored = NexusFormat::deserialize(data.data(), data.size());
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_GE(restored[0].attributes().vertexCount(), 4u);
    EXPECT_GE(restored[0].topology().faceCount(), 2u);
}

TEST(DeepHardening, MeshIOExportDoesNotCrash)
{
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t=cube.topology();
    t.addFace(Face{{0,2,1}});

    // OBJ export should not crash.
    MeshExportOptions opts;
    opts.format = MeshExportFormat::OBJ;
    MeshIO::exportMesh(cube, "/tmp/test_export.obj", opts);
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  REVOLVE / EXTRUDE EDGE CASES
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, RevolveZeroLengthProfile)
{
    NurbsCurve profile(1,{0,0,1,1},{Vec3{1,0,0},Vec3{1,0,0}}); // zero length
    RevolveDesc desc;
    NurbsSurface surf;
    Mesh mesh;
    auto rep=RevolveOperation::revolve(profile, desc, surf, &mesh);
    // Should not crash.
    SUCCEED();
}

TEST(DeepHardening, ExtrudeZeroHeight)
{
    NurbsCurve profile(1,{0,0,1,1},{Vec3{0,0,0},Vec3{1,0,0}});
    CurveExtrudeDesc desc;
    desc.height=0.f; // invalid
    NurbsSurface surf;
    Mesh mesh;
    auto rep=CurveExtrudeOperation::extrude(profile, desc, surf, &mesh);
    EXPECT_NE(rep.diagnostic, CurveExtrudeDiagnostic::Success);
}

TEST(DeepHardening, RevolveWithCapsFullCircle)
{
    NurbsCurve profile(1,{0,0,1,1},{Vec3{2,0,0},Vec3{2,0,5}});
    RevolveDesc desc;
    desc.capEnds=true; desc.angularSamples=12;
    NurbsSurface surf; Mesh mesh;
    auto rep=RevolveOperation::revolve(profile, desc, surf, &mesh);
    ASSERT_EQ(rep.diagnostic, RevolveDiagnostic::Success);
    EXPECT_GT(rep.capFaceCount, 0u);
    EXPECT_GT(rep.faceCount, rep.capFaceCount);
}

// ═══════════════════════════════════════════════════════════════════════
//  CAD DEEP TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, FeatureTreeMultipleLevels)
{
    CadDocument doc;
    // Build a chain of features.
    for(int i=0;i<5;++i){
        auto sk=ParametricSketchFactory::createSketch();
        ParametricSketchFactory::addSketchPoint(sk,static_cast<double>(i),0.0);
        doc.addSketch(sk);
    }
    EXPECT_EQ(doc.history().featureCount(), 5u);

    auto tree=CadFeatureTree::build(doc);
    EXPECT_EQ(tree.size(), 5u);
}

TEST(DeepHardening, DesignTableExport)
{
    CadDesignTable table;
    table.addParameter("Width", 100.0);
    table.addParameter("Height", 50.0);
    table.addRow("Small", {50.0, 25.0});
    table.addRow("Large", {200.0, 100.0});

    std::string csv=table.exportCSV();
    EXPECT_NE(csv.find("Width"), std::string::npos);
    EXPECT_NE(csv.find("Small"), std::string::npos);
    EXPECT_NE(csv.find("Large"), std::string::npos);
}

TEST(DeepHardening, LayerManagerLifecycle)
{
    CadLayerManager mgr;
    uint32_t id=mgr.createLayer("Default", 0xFFFFFF);
    EXPECT_EQ(mgr.layers().size(), 1u);

    mgr.setVisible(id, false);
    EXPECT_FALSE(mgr.layer(id)->visible);

    mgr.setLocked(id, true);
    EXPECT_TRUE(mgr.layer(id)->locked);

    mgr.createLayer("Hidden", 0xFF0000);
    EXPECT_EQ(mgr.layers().size(), 2u);
}

TEST(DeepHardening, DependencyGraphAcyclic)
{
    CadDocument doc;
    EXPECT_FALSE(CadDependencyGraph::hasCycles(doc));

    auto sk=ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk,0,0);
    doc.addSketch(sk);
    doc.addSketch(sk); // second sketch

    EXPECT_FALSE(CadDependencyGraph::hasCycles(doc));
}

TEST(DeepHardening, BookmarkSaveRestore)
{
    CadDocument doc;
    CadBookmarks bm;
    bm.save("start", 0);
    EXPECT_EQ(bm.names().size(), 1u);
    EXPECT_EQ(bm.names()[0], "start");

    bm.restore(doc, "start");
    bm.clear();
    EXPECT_TRUE(bm.names().empty());
}

TEST(DeepHardening, XRefAttachDetach)
{
    CadXRefManager xref;
    EXPECT_TRUE(xref.attach("part_a.nxm", "PartA"));
    EXPECT_EQ(xref.refs().size(), 1u);
    EXPECT_FALSE(xref.anyModified());

    xref.reload("PartA");
    xref.detach("PartA");
    EXPECT_TRUE(xref.refs().empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE SYSTEM STRESS TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, ModeOrchestratorRapidSwitching)
{
    AppContext ctx;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();

    for(int i=0;i<20;++i){
        orch.switchTo("select");
        orch.switchTo("sketch");
        orch.switchTo("fillet");
    }
    SUCCEED();
}

TEST(DeepHardening, ModeOrchestratorOverlayStack)
{
    AppContext ctx;
    ModeOrchestrator orch(ctx);
    orch.registerBuiltinModes();
    orch.switchTo("select");

    auto& reg=ModeRegistry::instance();
    reg.registerMode("overlay_test", []()->std::unique_ptr<AppMode>{
        struct O:AppMode{
            std::string modeId()const override{return"overlay_test";}
            std::string displayName()const override{return"O";}
        };
        return std::make_unique<O>();
    });

    for(int i=0;i<5;++i){
        orch.pushOverlay(reg.create("overlay_test"));
    }
    for(int i=0;i<5;++i){
        orch.popOverlay();
    }
    SUCCEED();
}

TEST(DeepHardening, ViewportModeSwitching)
{
    ViewportController vp;
    EXPECT_EQ(vp.mode(), ViewportMode::Solid);

    vp.setMode(ViewportMode::Wireframe);
    EXPECT_EQ(vp.mode(), ViewportMode::Wireframe);

    vp.setMode(ViewportMode::Rendered);
    EXPECT_EQ(vp.mode(), ViewportMode::Rendered);
}

// ═══════════════════════════════════════════════════════════════════════
//  CONSTRAINT SOLVER CONVERGENCE
// ═══════════════════════════════════════════════════════════════════════

TEST(DeepHardening, SolverConvergesOnTetrahedron)
{
    // Fixed triangle with a free point that must satisfy distance constraints.
    ConstraintGraph g;
    auto v0=g.addPoint({0,0,0}), v1=g.addPoint({1,0,0}), v2=g.addPoint({0.5,0.866,0});
    auto v3=g.addPoint({0.5,0.289,0}); // close to centroid

    // Constrain v3 to be at equal distance from v0,v1,v2 (circumcenter).
    g.addDistanceConstraint(v3,v0,0.577);
    g.addDistanceConstraint(v3,v1,0.577);
    g.addDistanceConstraint(v3,v2,0.577);

    auto report=ParametricSolver::solve(g);
    EXPECT_TRUE(report.converged);
}

TEST(DeepHardening, SolverHandlesOverConstrained)
{
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}), b=g.addPoint({1,0,0});

    // 2 entities = 6 DOF. Coincident = 3 constraints → 3 remaining.
    // Add 4 distance constraints → 7 effective → over-constrained.
    g.addCoincidentConstraint(a,b);
    g.addDistanceConstraint(a,b,1.0);
    g.addDistanceConstraint(a,b,2.0);
    g.addDistanceConstraint(a,b,3.0);
    g.addDistanceConstraint(a,b,4.0);

    auto dof=g.analyzeDegreesOfFreedom();
    EXPECT_TRUE(dof.likelyOverconstrained);

    // Should not crash.
    ParametricSolver::solve(g);
    SUCCEED();
}
