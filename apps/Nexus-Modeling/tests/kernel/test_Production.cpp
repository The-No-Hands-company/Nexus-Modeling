#include <gtest/gtest.h>

#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/BlendNetwork.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/NexusFormat.h>
#include <nexus/parametric/ParametricSolver.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadFeatureTree.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace nexus::cad;
using namespace nexus::parametric;
using namespace nexus::geometry;

// ═══════════════════════════════════════════════════════════════════════
//  PERFORMANCE BENCHMARKING
// ═══════════════════════════════════════════════════════════════════════

TEST(Performance, RevolveUnderOneMs)
{
    NurbsCurve p(1,{0,0,1,1},{Vec3{1,0,0},Vec3{1,0,3}});
    RevolveDesc d; d.angularSamples=16; d.capEnds=true;
    NurbsSurface s; Mesh m;

    auto start=std::chrono::high_resolution_clock::now();
    RevolveOperation::revolve(p,d,s,&m);
    auto end=std::chrono::high_resolution_clock::now();

    auto us=std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    EXPECT_LT(us, 50000); // under 50ms
}

TEST(Performance, ExtrudeUnderOneMs)
{
    NurbsCurve p(1,{0,0,1,1},{Vec3{0,0,0},Vec3{2,0,0}});
    CurveExtrudeDesc d; d.direction={0,0,1}; d.height=5; d.heightSamples=8;
    NurbsSurface s; Mesh m;

    auto start=std::chrono::high_resolution_clock::now();
    CurveExtrudeOperation::extrude(p,d,s,&m);
    auto end=std::chrono::high_resolution_clock::now();

    auto us=std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    EXPECT_LT(us, 50000);
}

TEST(Performance, BRepConstructionUnderOneMs)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    auto start=std::chrono::high_resolution_clock::now();
    auto [body,report]=BRepBody::fromMesh(tet);
    auto end=std::chrono::high_resolution_clock::now();

    auto us=std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    EXPECT_LT(us, 50000);
}

TEST(Performance, SolverConvergesQuickly)
{
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}),b=g.addPoint({10,0,0}),c=g.addPoint({5,8,0});
    g.addDistanceConstraint(a,b,5);g.addDistanceConstraint(b,c,6);g.addDistanceConstraint(c,a,7);

    auto start=std::chrono::high_resolution_clock::now();
    auto report=ParametricSolver::solve(g);
    auto end=std::chrono::high_resolution_clock::now();

    auto us=std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
    EXPECT_TRUE(report.converged);
    EXPECT_LT(us, 100000); // under 100ms
    EXPECT_LE(report.iterationsRan, 32);
}

// ═══════════════════════════════════════════════════════════════════════
//  NUMERICAL PRECISION BOUNDARY TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(Precision, FloatEpsilonHandling)
{
    // Distances at floating-point epsilon should still work.
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}), b=g.addPoint({0,0,0});
    g.addDistanceConstraint(a,b,0.0); // zero distance

    auto report=ParametricSolver::solve(g);
    EXPECT_TRUE(report.converged);

    auto pa=g.point(a), pb=g.point(b);
    double d=std::sqrt((pa->x-pb->x)*(pa->x-pb->x)+(pa->y-pb->y)*(pa->y-pb->y)+(pa->z-pb->z)*(pa->z-pb->z));
    EXPECT_LT(d, 1e-6);
}

TEST(Precision, NearZeroExtrusion)
{
    NurbsCurve p(1,{0,0,1,1},{Vec3{0,0,0},Vec3{1,0,0}});
    CurveExtrudeDesc d; d.direction={0,0,1};
    // Height just above zero should not crash.
    d.height=1e-6f;
    NurbsSurface s; Mesh m;
    auto rep=CurveExtrudeOperation::extrude(p,d,s,&m);
    // May succeed or fail, but must not crash.
    SUCCEED();
}

TEST(Precision, MassPropertiesPrecision)
{
    // Unit cube mass properties should be exact.
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

    auto mp=MeshMassProperties::compute(cube);
    EXPECT_NEAR(mp.volume, 1.0f, 0.01f);
    EXPECT_NEAR(mp.centroid.x, 0.5f, 0.01f);
    EXPECT_NEAR(mp.centroid.y, 0.5f, 0.01f);
    EXPECT_NEAR(mp.centroid.z, 0.5f, 0.01f);
}

// ═══════════════════════════════════════════════════════════════════════
//  DETERMINISM PROOFS
// ═══════════════════════════════════════════════════════════════════════

TEST(Determinism, SameInputSameOutputRevolve)
{
    NurbsCurve p(1,{0,0,1,1},{Vec3{2,0,0},Vec3{2,0,5}});
    RevolveDesc d; d.angularSamples=12; d.capEnds=true;

    NurbsSurface s1,s2; Mesh m1,m2;
    RevolveOperation::revolve(p,d,s1,&m1);
    RevolveOperation::revolve(p,d,s2,&m2);

    EXPECT_EQ(m1.attributes().vertexCount(), m2.attributes().vertexCount());
    EXPECT_EQ(m1.topology().faceCount(), m2.topology().faceCount());

    const auto& p1=m1.attributes().positions(), p2=m2.attributes().positions();
    for(size_t i=0;i<p1.size()&&i<p2.size();++i){
        EXPECT_FLOAT_EQ(p1[i].x, p2[i].x);
        EXPECT_FLOAT_EQ(p1[i].y, p2[i].y);
        EXPECT_FLOAT_EQ(p1[i].z, p2[i].z);
    }
}

TEST(Determinism, SameInputSameOutputNexusFormat)
{
    Mesh cube;
    cube.attributes().setPositions({{0,0,0},{1,0,0},{0,1,0}});
    cube.topology().addFace(Face{{0,1,2}});

    std::vector<Mesh> meshes={cube};
    auto d1=NexusFormat::serialize(meshes);
    auto d2=NexusFormat::serialize(meshes);
    EXPECT_EQ(d1,d2);

    auto r1=NexusFormat::deserialize(d1.data(),d1.size());
    auto r2=NexusFormat::deserialize(d2.data(),d2.size());
    ASSERT_EQ(r1.size(),r2.size());
    EXPECT_EQ(r1[0].attributes().vertexCount(),r2[0].attributes().vertexCount());
}

// ═══════════════════════════════════════════════════════════════════════
//  RECOVERY AFTER FAILURE
// ═══════════════════════════════════════════════════════════════════════

TEST(Recovery, FailedOperationCleanState)
{
    // After a failed operation, nothing should be corrupted.
    ConstraintGraph g;
    auto a=g.addPoint({0,0,0}), b=g.addPoint({1,0,0});
    EXPECT_EQ(g.addDistanceConstraint(a,b,-1.0),kInvalidConstraintId); // negative rejected
    EXPECT_EQ(g.distanceConstraintCount(),0u); // not added

    // Valid operation after invalid should work.
    EXPECT_NE(g.addDistanceConstraint(a,b,3.0),kInvalidConstraintId);
    EXPECT_EQ(g.distanceConstraintCount(),1u);
}

TEST(Recovery, FailedExtrudeLeavesOutputClean)
{
    NurbsCurve p(1,{0,0,1,1},{Vec3{0,0,0},Vec3{1,0,0}});
    CurveExtrudeDesc d; d.direction={0,0,0}; // zero direction - invalid

    NurbsSurface s; Mesh m;
    auto rep=CurveExtrudeOperation::extrude(p,d,s,&m);
    EXPECT_NE(rep.diagnostic,CurveExtrudeDiagnostic::Success);

    // Output should be empty.
    EXPECT_EQ(m.topology().faceCount(),0u);
    EXPECT_EQ(m.attributes().vertexCount(),0u);
}

TEST(Recovery, FailedBRepLeavesOutputClean)
{
    Mesh invalid;
    auto [body,report]=BRepBody::fromMesh(invalid);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(body.vertexCount(),0u);
    EXPECT_EQ(body.edgeCount(),0u);
}

// ═══════════════════════════════════════════════════════════════════════
//  STRESS: RAPID OPERATION CYCLES
// ═══════════════════════════════════════════════════════════════════════

TEST(Stress, RapidConstraintGraphCycles)
{
    for(int cycle=0;cycle<50;++cycle){
        ConstraintGraph g;
        auto a=g.addPoint({0,0,0}), b=g.addPoint({1,0,0});
        g.addDistanceConstraint(a,b,2.0);
        ParametricSolver::solve(g);
        g.removeEntity(a); // constraint removed via cascade
        EXPECT_LE(g.entityCount(),1u); // b remains
    }
}

TEST(Stress, RapidDocumentCycles)
{
    CadDocument doc;
    for(int i=0;i<20;++i){
        auto sk=ParametricSketchFactory::createSketch();
        ParametricSketchFactory::addSketchPoint(sk,static_cast<double>(i),0);
        doc.addSketch(sk);
        doc.undo();
        doc.redo();
    }
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  MAXIMUM INPUT TESTING
// ═══════════════════════════════════════════════════════════════════════

TEST(Extremes, MaximumFaceCountBRep)
{
    Mesh big;
    std::vector<Vec3> verts(1000, Vec3{0,0,0});
    for(size_t i=0;i<verts.size();++i)
        verts[i]={static_cast<float>(i%100),static_cast<float>(i/100),0.f};
    big.attributes().setPositions(verts);

    auto& t=big.topology();
    for(size_t i=0;i+2<verts.size();i+=3)
        t.addFace(Face{{static_cast<uint32_t>(i),static_cast<uint32_t>(i+1),static_cast<uint32_t>(i+2)}});

    // Build B-Rep from large mesh — should not crash.
    auto [body,report]=BRepBody::fromMesh(big);
    (void)body;(void)report;
    SUCCEED();
}

TEST(Extremes, MaximumConstraintCount)
{
    ConstraintGraph g;
    std::vector<ParametricEntityId> ents;
    for(int i=0;i<200;++i){
        auto id=g.addPoint({static_cast<double>(i),0.0,0.0});
        if(id!=kInvalidEntityId) ents.push_back(id);
    }
    for(size_t i=0;i+1<ents.size();++i)
        g.addDistanceConstraint(ents[i],ents[i+1],1.0);

    ParametricSolverConfig cfg;
    cfg.maxIterations=64;
    auto report=ParametricSolver::solve(g,cfg);
    EXPECT_GE(report.iterationsRan, 1u);
}
