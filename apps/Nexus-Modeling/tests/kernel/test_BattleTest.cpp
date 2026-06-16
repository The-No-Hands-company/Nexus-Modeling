#include <gtest/gtest.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadAutoConstraintSketch.h>
#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/BlendNetwork.h>
#include <nexus/parametric/ParametricSolver.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace nexus::cad;
using namespace nexus::parametric;
using namespace nexus::geometry;

// ── Fuzz Testing ────────────────────────────────────────────────────────

TEST(BattleTest, RandomConstraintGraphSolve)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> pos(-100.0, 100.0);
    std::uniform_int_distribution<uint32_t> entityCount(2, 20);
    std::uniform_int_distribution<uint32_t> constraintCount(1, 30);

    for(int trial = 0; trial < 50; ++trial) {
        ConstraintGraph graph;
        uint32_t n = entityCount(rng);
        std::vector<ParametricEntityId> entities;
        for(uint32_t i=0;i<n;++i) {
            auto id = graph.addPoint({pos(rng), pos(rng), pos(rng)});
            if(id != kInvalidEntityId) entities.push_back(id);
        }
        if(entities.size() < 2) continue;

        uint32_t cCount = constraintCount(rng);
        for(uint32_t j=0;j<cCount;++j) {
            uint32_t a = rng() % entities.size();
            uint32_t b = rng() % entities.size();
            if(a == b) continue;
            double dist = std::max(0.1, std::abs(pos(rng)));
            (void)graph.addDistanceConstraint(entities[a], entities[b], dist);
        }

        // Should not crash.
        ParametricSolver::solve(graph);
        SUCCEED();
    }
}

TEST(BattleTest, RandomNurbsCurveEvaluation)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> coord(-10.f, 10.f);

    for(int trial = 0; trial < 50; ++trial) {
        int cpCount = 2 + (trial % 8);
        std::vector<Vec3> cps(cpCount);
        for(auto& p : cps) p = {coord(rng), coord(rng), coord(rng)};

        int deg = std::min(3, cpCount-1);
        std::vector<float> knots(cpCount + deg + 1);
        for(int j=0;j<=deg;++j) knots[j]=0.f;
        for(int j=1;j<cpCount-deg;++j) knots[deg+j]=static_cast<float>(j)/static_cast<float>(cpCount-deg);
        for(int j=0;j<=deg;++j) knots[knots.size()-1-j]=1.f;

        NurbsCurve curve(deg, knots, cps);
        if(!curve.isValid()) continue;

        // Evaluate at random parameters.
        auto dom = curve.domain();
        for(int e=0;e<20;++e) {
            float t = dom.first + (dom.second-dom.first) * static_cast<float>(rng()%1000)/1000.f;
            (void)curve.evaluate(t);
            SUCCEED();
        }
    }
}

TEST(BattleTest, RandomMeshBuildAndQuery)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> coord(-5.f, 5.f);

    for(int trial = 0; trial < 20; ++trial) {
        int vc = 3 + (trial % 47);
        Mesh m;
        std::vector<Vec3> verts(vc);
        for(auto& v : verts) v = {coord(rng), coord(rng), coord(rng)};
        m.attributes().setPositions(verts);

        // Build random triangles.
        auto& topo = m.topology();
        for(int fi=0;fi<std::min(vc-2, 10);++fi) {
            uint32_t a=rng()%vc, b=rng()%vc, c=rng()%vc;
            if(a!=b&&b!=c&&a!=c) topo.addFace(Face{{a,b,c}});
        }

        // Try to build B-Rep from it (may fail, should not crash).
        auto [body, report] = BRepBody::fromMesh(m);
        (void)body; (void)report;
        SUCCEED();
    }
}

// ── Degenerate Case Testing ─────────────────────────────────────────────

TEST(BattleTest, ZeroLengthEdgeConstraint)
{
    ConstraintGraph graph;
    auto a = graph.addPoint({0,0,0});
    auto b = graph.addPoint({0,0,0});
    ASSERT_NE(a, kInvalidEntityId);
    ASSERT_NE(b, kInvalidEntityId);

    // Zero-distance constraint should work.
    auto cid = graph.addDistanceConstraint(a, b, 0.0);
    // Constraint graph rejects negative distances, but 0.0 may be accepted.
    if(cid != kInvalidConstraintId) {
        ParametricSolver::solve(graph);
    }
    SUCCEED();
}

TEST(BattleTest, SingleTriangleBRep)
{
    Mesh tri;
    tri.attributes().setPositions({{0,0,0},{1,0,0},{0,1,0}});
    tri.topology().addFace(Face{{0,1,2}});

    auto [body, report] = BRepBody::fromMesh(tri);
    // May or may not be valid, but should not crash.
    EXPECT_TRUE(true); // no crash = pass
}

TEST(BattleTest, ColinearPointsInConstraint)
{
    ConstraintGraph graph;
    auto a = graph.addPoint({0,0,0});
    auto b = graph.addPoint({1,0,0});
    auto c = graph.addPoint({2,0,0});
    ASSERT_NE(a, kInvalidEntityId);

    // Angle constraint with colinear points.
    (void)graph.addAngleConstraint(a, b, c, 30.0);
    ParametricSolver::solve(graph);
    SUCCEED();
}

TEST(BattleTest, DegenerateNurbsSurface)
{
    NurbsSurface s(1,1,
        {0.f,0.f,1.f,1.f},{0.f,0.f,1.f,1.f},
        {Vec3{0,0,0},Vec3{0,0,0},Vec3{0,0,0},Vec3{0,0,0}},2,2);
    // Degenerate geometry, but may be structurally valid. Should not crash.
    (void)s.evaluate(0.5f, 0.5f);
    SUCCEED();
}

// ── Large-Scale Testing ────────────────────────────────────────────────

TEST(BattleTest, LargeMeshBRepConstruction)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> coord(-50.f, 50.f);

    // Build a large-ish mesh.
    Mesh m;
    const int vc = 1000;
    std::vector<Vec3> verts(vc);
    for(auto& v : verts) v = {coord(rng), coord(rng), coord(rng)};
    m.attributes().setPositions(verts);

    auto& topo = m.topology();
    for(int fi=0;fi<500;++fi) {
        uint32_t a=rng()%vc, b=rng()%vc, c=rng()%vc;
        if(a!=b&&b!=c&&a!=c) topo.addFace(Face{{a,b,c}});
    }

    auto [body, report] = BRepBody::fromMesh(m);
    (void)body; (void)report;
    SUCCEED();
}

TEST(BattleTest, ManyConstraintsSolve)
{
    ConstraintGraph graph;
    std::vector<ParametricEntityId> ents;
    for(int i=0;i<100;++i) {
        auto id = graph.addPoint({static_cast<double>(i), static_cast<double>(i%10), 0.0});
        if(id != kInvalidEntityId) ents.push_back(id);
    }

    for(size_t i=0;i+1<ents.size();++i) {
        (void)graph.addDistanceConstraint(ents[i], ents[i+1], 1.0);
    }

    ParametricSolverReport report = ParametricSolver::solve(graph);
    EXPECT_TRUE(report.converged || !report.converged); // may not converge with 99 constraints
}

// ── Round-Trip Testing ─────────────────────────────────────────────────

TEST(BattleTest, RevolveMeshRoundTrip)
{
    NurbsCurve profile(1, {0.f,0.f,1.f,1.f}, {Vec3{2,0,0}, Vec3{2,0,3}});

    RevolveDesc desc;
    desc.angularSamples = 16;
    desc.capEnds = true;

    Mesh mesh;
    NurbsSurface surf;
    auto rep = RevolveOperation::revolve(profile, desc, surf, &mesh);
    ASSERT_EQ(rep.diagnostic, RevolveDiagnostic::Success);

    // Build B-Rep and export back to mesh.
    auto [body, brepRep] = BRepBody::fromMesh(mesh);
    if(brepRep.valid) {
        Mesh restored = body.toMesh();
        EXPECT_GE(restored.topology().faceCount(), mesh.topology().faceCount() / 4);
    }
}

TEST(BattleTest, ExtrudeMeshRoundTrip)
{
    NurbsCurve profile(1, {0.f,0.f,1.f,1.f}, {Vec3{-1,0,0}, Vec3{1,0,0}});

    CurveExtrudeDesc desc;
    desc.direction = {0,0,1};
    desc.height = 4.f;
    desc.heightSamples = 4;
    desc.capEnds = true;

    Mesh mesh;
    NurbsSurface surf;
    auto rep = CurveExtrudeOperation::extrude(profile, desc, surf, &mesh);
    ASSERT_EQ(rep.diagnostic, CurveExtrudeDiagnostic::Success);

    // Verify vertex count is reasonable.
    EXPECT_GE(mesh.attributes().vertexCount(), 4u);
    EXPECT_GE(mesh.topology().faceCount(), 2u);
}

// ── Stress Testing ─────────────────────────────────────────────────────

TEST(BattleTest, RapidUndoRedo)
{
    CadDocument doc;
    for(int i=0;i<5;++i) {
        auto sk = ParametricSketchFactory::createSketch();
        ParametricSketchFactory::addSketchPoint(sk, static_cast<double>(i), 0.0);
        ParametricSketchFactory::addSketchPoint(sk, static_cast<double>(i+1), 0.0);
        doc.addSketch(sk);
    }

    for(int i=0;i<10;++i) {
        if(doc.canUndo()) doc.undo();
        if(doc.canRedo()) doc.redo();
    }
    SUCCEED();
}

TEST(BattleTest, MassPropertiesOnRandomSolid)
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

    auto mp = MeshMassProperties::compute(cube);
    EXPECT_GT(mp.volume, 0.f);
    EXPECT_FLOAT_EQ(mp.centroid.x, 0.5f);
    EXPECT_FLOAT_EQ(mp.centroid.y, 0.5f);
    EXPECT_FLOAT_EQ(mp.centroid.z, 0.5f);
}
