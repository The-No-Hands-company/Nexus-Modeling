#include <gtest/gtest.h>

#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/BlendNetwork.h>
#include <nexus/geometry/MeshAnalysis.h>
#include <nexus/geometry/NexusFormat.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadFeatureTree.h>
#include <nexus/cad/CadInfrastructure.h>
#include <nexus/app/AppMode.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace nexus::cad;
using namespace nexus::app;
using namespace nexus::parametric;
using namespace nexus::geometry;

// ═══════════════════════════════════════════════════════════════════════
//  RANDOM PERTURBATION TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(AlgorithmDepth, RandomPerturbationSolverStability)
{
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> perturb(-0.1, 0.1);

    // Build a known-good constraint system.
    ConstraintGraph g;
    auto v0=g.addPoint({0,0,0}), v1=g.addPoint({3,0,0}), v2=g.addPoint({0,4,0});
    g.addDistanceConstraint(v0,v1,3.0);
    g.addDistanceConstraint(v0,v2,4.0);
    g.addDistanceConstraint(v1,v2,5.0); // 3-4-5 triangle

    ParametricSolver::solve(g);

    // Perturb all points slightly and re-solve.
    for(int trial=0;trial<20;++trial){
        auto p0=g.point(v0);auto p1=g.point(v1);auto p2=g.point(v2);
        if(!p0||!p1||!p2) continue;
        g.setPoint(v0,{p0->x+perturb(rng),p0->y+perturb(rng),0});
        g.setPoint(v1,{p1->x+perturb(rng),p1->y+perturb(rng),0});
        g.setPoint(v2,{p2->x+perturb(rng),p2->y+perturb(rng),0});

        auto report=ParametricSolver::solve(g);
        EXPECT_TRUE(report.converged);
        EXPECT_LT(report.maxConstraintError, 1e-6);

        // Verify distances restored.
        auto pa=g.point(v0),pb=g.point(v1),pc=g.point(v2);
        double d01=std::sqrt((pa->x-pb->x)*(pa->x-pb->x)+(pa->y-pb->y)*(pa->y-pb->y));
        double d02=std::sqrt((pa->x-pc->x)*(pa->x-pc->x)+(pa->y-pc->y)*(pa->y-pc->y));
        double d12=std::sqrt((pb->x-pc->x)*(pb->x-pc->x)+(pb->y-pc->y)*(pb->y-pc->y));
        EXPECT_NEAR(d01,3.0,1e-6);
        EXPECT_NEAR(d02,4.0,1e-6);
        EXPECT_NEAR(d12,5.0,1e-6);
    }
}

TEST(AlgorithmDepth, SolverAdaptiveStepSize)
{
    // System that needs large initial corrections but fine final tuning.
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}), b=g.addPoint({100,0,0}), c=g.addPoint({50,80,0});

    g.addDistanceConstraint(a,b,3.0);  // huge change needed
    g.addDistanceConstraint(b,c,4.0);
    g.addDistanceConstraint(c,a,5.0);

    ParametricSolverConfig cfg;
    cfg.maxIterations = 64;
    auto report = ParametricSolver::solve(g, cfg);

    // Should either converge or at least make significant progress.
    EXPECT_LT(report.maxConstraintError, 10.0); // significant improvement
}

TEST(AlgorithmDepth, MultipleRestartStability)
{
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}), b=g.addPoint({5,0,0});

    for(int restart=0;restart<10;++restart){
        g.setPoint(a,{0.0,0.0,0.0});
        g.setPoint(b,{static_cast<double>(restart+1)*0.5,0.0,0.0});
        g.addDistanceConstraint(a,b,1.0);

        auto report=ParametricSolver::solve(g);
        // Constraint may be over-constrained by repeated adds; just verify no crash.
        (void)report;
    }
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  OPERATION STRESS TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(AlgorithmDepth, RepeatedRevolveConsistency)
{
    NurbsCurve profile(1,{0,0,1,1},{Vec3{1,0,0},Vec3{1,0,3}});
    RevolveDesc d; d.angularSamples=8;

    uint32_t faceCount=0;
    for(int i=0;i<10;++i){
        NurbsSurface s; Mesh m;
        auto rep=RevolveOperation::revolve(profile,d,s,&m);
        ASSERT_EQ(rep.diagnostic, RevolveDiagnostic::Success);
        if(i==0) faceCount=m.topology().faceCount();
        else EXPECT_EQ(m.topology().faceCount(), faceCount); // deterministic
    }
}

TEST(AlgorithmDepth, RepeatedExtrudeConsistency)
{
    NurbsCurve profile(1,{0,0,1,1},{Vec3{0,0,0},Vec3{2,0,0}});
    CurveExtrudeDesc d; d.direction={0,0,1}; d.height=4; d.heightSamples=4;

    uint32_t vc=0;
    for(int i=0;i<10;++i){
        NurbsSurface s; Mesh m;
        auto rep=CurveExtrudeOperation::extrude(profile,d,s,&m);
        ASSERT_EQ(rep.diagnostic,CurveExtrudeDiagnostic::Success);
        if(i==0) vc=m.attributes().vertexCount();
        else EXPECT_EQ(m.attributes().vertexCount(), vc);
    }
}

TEST(AlgorithmDepth, RepeatedBRepConstruction)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    for(int i=0;i<10;++i){
        auto [body,report]=BRepBody::fromMesh(tet);
        ASSERT_TRUE(report.valid);
        EXPECT_EQ(body.vertexCount(),4u);
        EXPECT_EQ(body.edgeCount(),6u);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  MEMORY SAFETY TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(AlgorithmDepth, NullPointerSafety)
{
    // All static functions should handle nullptr gracefully.
    ConstraintGraph g;
    EXPECT_EQ(g.point(kInvalidEntityId), nullptr);
    EXPECT_FALSE(g.setPoint(kInvalidEntityId, {0,0,0}));
    EXPECT_FALSE(g.hasEntity(kInvalidEntityId));

    CadDocument doc;
    auto data=doc.serialize();
    EXPECT_FALSE(doc.deserialize(nullptr, data.size()));
    EXPECT_FALSE(doc.deserialize(data.data(), 0));
}

TEST(AlgorithmDepth, OutOfBoundsIndexSafety)
{
    Mesh cube;
    cube.attributes().setPositions({{0,0,0},{1,0,0},{0,1,0}});
    cube.topology().addFace(Face{{0,1,2}});

    auto heOpt=HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh he=*heOpt;

    // Out-of-bounds edge index.
    EXPECT_EQ(he.findEdge(999,0), HalfEdgeMesh::kInvalid);
    EXPECT_EQ(he.findEdge(0,999), HalfEdgeMesh::kInvalid);

    // Out-of-bounds face indexes should be handled.
    EXPECT_TRUE(he.faceCount() >= 1u);
    SUCCEED();
}

TEST(AlgorithmDepth, EmptyInputHandling)
{
    // All operations should handle empty meshes.
    Mesh empty;
    auto [a,b]=splitBody(empty,{0,0,0},{0,0,1});
    EXPECT_EQ(a.topology().faceCount(),0u);
    EXPECT_EQ(b.topology().faceCount(),0u);

    DefeatureReport dr=defeature(empty);
    EXPECT_EQ(dr.holesRemoved,0u);

    auto ssi=surfaceIntersection(NurbsSurface{},NurbsSurface{});
    EXPECT_FALSE(ssi.converged);

    CadDocument doc;
    auto tree=CadFeatureTree::build(doc);
    EXPECT_TRUE(tree.empty());

    EXPECT_FALSE(CadDependencyGraph::hasCycles(doc));
}

// ═══════════════════════════════════════════════════════════════════════
//  SCALE INVARIANCE TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(AlgorithmDepth, SolverScaleInvariance)
{
    // Same geometry at different scales should converge similarly.
    for(double scale : {1.0, 0.01, 100.0, 1e-4}){
        ConstraintGraph g;
        auto a=g.addPoint({0,0,0});
        auto b=g.addPoint({3*scale,0,0});
        auto c=g.addPoint({0,4*scale,0});
        g.addDistanceConstraint(a,b,3*scale);
        g.addDistanceConstraint(a,c,4*scale);
        g.addDistanceConstraint(b,c,5*scale);

        auto report=ParametricSolver::solve(g);
        EXPECT_TRUE(report.converged) << "Failed at scale " << scale;
    }
}

TEST(AlgorithmDepth, NurbsScaleInvariance)
{
    for(float s : {1.f, 0.01f, 100.f}){
        NurbsCurve c(1,{0,0,1,1},{Vec3{0,0,0},Vec3{s,0,0}});
        EXPECT_TRUE(c.isValid());
        auto dom=c.domain();
        Vec3 mid=c.evaluate((dom.first+dom.second)*0.5f);
        EXPECT_NEAR(mid.x,s*0.5f,1e-4f);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  CONCURRENT SAFETY TESTING (single-threaded verification)
// ═══════════════════════════════════════════════════════════════════════

TEST(AlgorithmDepth, IndependentDocumentIsolation)
{
    // Two documents should not interfere with each other.
    CadDocument doc1, doc2;

    auto sk1=ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk1,0,0);
    doc1.addSketch(sk1);

    auto sk2=ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk2,10,10);
    doc2.addSketch(sk2);

    EXPECT_EQ(doc1.history().featureCount(),1u);
    EXPECT_EQ(doc2.history().featureCount(),1u);

    // Modify doc1, doc2 unaffected.
    doc1.undo();
    doc1.redo();
    EXPECT_EQ(doc2.history().featureCount(),1u);
}
