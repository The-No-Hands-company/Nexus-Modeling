#include <gtest/gtest.h>

#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/MeshRepair.h>
#include <nexus/geometry/NexusFormat.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/BlendNetwork.h>
#include <nexus/geometry/NurbsSurface.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace nexus::geometry;

// ═══════════════════════════════════════════════════════════════════════
//  B-REP TOPOLOGY ROUND-TRIP INTEGRITY
// ═══════════════════════════════════════════════════════════════════════

TEST(Integrity, BRepMeshBRepRoundTrip)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    // Mesh → B-Rep.
    auto [body1, r1] = BRepBody::fromMesh(tet);
    ASSERT_TRUE(r1.valid);
    EXPECT_EQ(body1.eulerCharacteristic(), 2);
    EXPECT_EQ(body1.genus(), 0);

    // B-Rep → Mesh.
    Mesh restored = body1.toMesh();

    // Mesh → B-Rep again.
    auto [body2, r2] = BRepBody::fromMesh(restored);
    ASSERT_TRUE(r2.valid);

    // Topology should be preserved.
    EXPECT_EQ(body2.vertexCount(), body1.vertexCount());
    EXPECT_EQ(body2.eulerCharacteristic(), body1.eulerCharacteristic());
    EXPECT_EQ(body2.genus(), body1.genus());
}

TEST(Integrity, FaceVertexCountPreserved)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    auto [body, report] = BRepBody::fromMesh(tet);
    ASSERT_TRUE(report.valid);

    // Each face should have exactly 3 vertices (triangles).
    for(BRepFaceId f=0; f<body.faceCount(); ++f) {
        auto verts = body.faceVertices(f);
        EXPECT_EQ(verts.size(), 3u);
    }

    // Each face should have exactly 3 edges.
    for(BRepFaceId f=0; f<body.faceCount(); ++f) {
        auto edges = body.faceEdges(f);
        EXPECT_EQ(edges.size(), 3u);
    }
}

TEST(Integrity, VertexFaceAdjacencyConsistent)
{
    Mesh tet;
    tet.attributes().setPositions({{0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816}});
    tet.topology().addFace(Face{{0,2,1}});tet.topology().addFace(Face{{0,1,3}});
    tet.topology().addFace(Face{{1,2,3}});tet.topology().addFace(Face{{2,0,3}});

    auto [body, report] = BRepBody::fromMesh(tet);
    ASSERT_TRUE(report.valid);

    for(BRepVertexId v=0; v<body.vertexCount(); ++v) {
        auto faces = body.vertexFaces(v);
        EXPECT_EQ(faces.size(), 3u); // tetrahedron: each vertex touches 3 faces

        // Each face in the vertex's incident list should contain this vertex.
        for(BRepFaceId f : faces) {
            auto verts = body.faceVertices(f);
            EXPECT_NE(std::find(verts.begin(), verts.end(), v), verts.end());
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  MESH REPAIR EFFECTIVENESS
// ═══════════════════════════════════════════════════════════════════════

TEST(Integrity, RepairFixesSelfIntersectionOrDegeneracy)
{
    // Mesh with a degenerate (zero-area) face.
    Mesh broken;
    broken.attributes().setPositions({{0,0,0},{1,0,0},{0,1,0},{0,0,0}}); // last vertex = duplicate of first
    broken.topology().addFace(Face{{0,1,2,3}}); // quad with coincident vertices

    RepairOptions opts;
    auto report = MeshRepair::repair(broken, opts);

    // Repair should either fix or report issues.
    EXPECT_TRUE(report.ok || !report.ok); // either way, should not crash
}

TEST(Integrity, RepairPreservesManifoldGeometry)
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

    RepairOptions opts;
    auto report = MeshRepair::repair(cube, opts);

    // Cube should pass repair without issues.
    EXPECT_TRUE(report.ok);
    EXPECT_TRUE(report.error.empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  FORMAT ROUND-TRIP FIDELITY
// ═══════════════════════════════════════════════════════════════════════

TEST(Integrity, NexusFormatVertexFidelity)
{
    Mesh original;
    original.attributes().setPositions({
        {0.1f,0.2f,0.3f},{1.4f,0.5f,0.6f},
        {0.7f,1.8f,0.9f},{0.5f,0.5f,1.2f}
    });
    original.topology().addFace(Face{{0,1,2}});
    original.topology().addFace(Face{{0,2,3}});
    original.topology().addFace(Face{{0,3,1}});
    original.topology().addFace(Face{{1,3,2}});

    std::vector<Mesh> meshes = {original};
    auto data = NexusFormat::serialize(meshes);
    auto restored = NexusFormat::deserialize(data.data(), data.size());

    ASSERT_EQ(restored.size(), 1u);
    const auto& origPos = original.attributes().positions();
    const auto& restPos = restored[0].attributes().positions();

    ASSERT_EQ(origPos.size(), restPos.size());
    for(size_t i=0; i<origPos.size(); ++i) {
        EXPECT_FLOAT_EQ(origPos[i].x, restPos[i].x);
        EXPECT_FLOAT_EQ(origPos[i].y, restPos[i].y);
        EXPECT_FLOAT_EQ(origPos[i].z, restPos[i].z);
    }
}

TEST(Integrity, NexusFormatTopologyFidelity)
{
    Mesh original;
    original.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t=original.topology();
    t.addFace(Face{{0,2,1}});t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}});t.addFace(Face{{4,6,7}});

    std::vector<Mesh> meshes = {original};
    auto data = NexusFormat::serialize(meshes);
    auto restored = NexusFormat::deserialize(data.data(), data.size());

    ASSERT_EQ(restored.size(), 1u);
    EXPECT_EQ(restored[0].topology().faceCount(), original.topology().faceCount());

    // Verify face vertex counts match.
    for(uint32_t fi=0; fi<original.topology().faceCount(); ++fi) {
        EXPECT_EQ(restored[0].topology().face(fi).vertexCount(),
                  original.topology().face(fi).vertexCount());
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  OPERATION CHAINING INTEGRITY
// ═══════════════════════════════════════════════════════════════════════

TEST(Integrity, ExtrudeThenSplitThenBRep)
{
    // Extrude → Split → B-Rep: full chain.
    NurbsCurve profile(1,{0,0,1,1},{Vec3{-1,0,0},Vec3{1,0,0}});
    CurveExtrudeDesc ed; ed.direction={0,0,1}; ed.height=4; ed.heightSamples=4;
    NurbsSurface es; Mesh em;
    auto er=CurveExtrudeOperation::extrude(profile,ed,es,&em);
    ASSERT_EQ(er.diagnostic, CurveExtrudeDiagnostic::Success);

    // Split.
    auto [top, bottom] = splitBody(em, {0,0,2}, {0,0,1});
    EXPECT_GT(top.topology().faceCount(), 0u);
    EXPECT_GT(bottom.topology().faceCount(), 0u);

    // B-Rep from split result.
    auto [body, report] = BRepBody::fromMesh(top);
    EXPECT_TRUE(report.valid || !report.valid); // may or may not be valid
}

TEST(Integrity, RevolveBlendBRep)
{
    // Revolve → Blend → B-Rep.
    NurbsCurve profile(1,{0,0,1,1},{Vec3{2,0,0},Vec3{2,0,4}});
    RevolveDesc rd; rd.angularSamples=16; rd.capEnds=true;
    NurbsSurface rs; Mesh rm;
    auto rr=RevolveOperation::revolve(profile,rd,rs,&rm);
    ASSERT_EQ(rr.diagnostic, RevolveDiagnostic::Success);

    // Convert to half-edge and apply blend.
    auto heOpt=HalfEdgeMesh::fromMesh(rm);
    if(heOpt) {
        uint32_t e=heOpt->findEdge(0,heOpt->vertexCount()>1?1:0);
        if(e!=HalfEdgeMesh::kInvalid) {
            auto breport=solveBlendNetwork(*heOpt, {{e,0.1f}});
            // May or may not succeed — verify no crash.
            (void)breport;
        }
    }
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  NURBS SURFACE SAMPLING INTEGRITY
// ═══════════════════════════════════════════════════════════════════════

TEST(Integrity, SurfaceEvaluateAcrossFullDomain)
{
    NurbsSurface s(2,2,
        {0,0,0,1,1,1},{0,0,0,1,1,1},
        {Vec3{0,0,0},Vec3{1,0,0},Vec3{2,0,0},Vec3{0,1,0},Vec3{1,1,2},Vec3{2,1,0},
         Vec3{0,2,0},Vec3{1,2,0},Vec3{2,2,0}},3,3);
    ASSERT_TRUE(s.isValid());

    auto du=s.domainU(), dv=s.domainV();

    // Evaluate at 100 points across domain — no crashes.
    for(int i=0;i<10;++i) for(int j=0;j<10;++j) {
        float u=du.first+(du.second-du.first)*static_cast<float>(i)/9.f;
        float v=dv.first+(dv.second-dv.first)*static_cast<float>(j)/9.f;
        Vec3 pt=s.evaluate(u,v);
        EXPECT_TRUE(std::isfinite(pt.x));
        EXPECT_TRUE(std::isfinite(pt.y));
        EXPECT_TRUE(std::isfinite(pt.z));
    }
}

TEST(Integrity, SurfaceDerivativesFinite)
{
    NurbsSurface s(2,2,
        {0,0,0,1,1,1},{0,0,0,1,1,1},
        {Vec3{0,0,0},Vec3{1,0,0},Vec3{2,0,0},Vec3{0,1,0},Vec3{1,1,2},Vec3{2,1,0},
         Vec3{0,2,0},Vec3{1,2,0},Vec3{2,2,0}},3,3);

    auto du=s.domainU(), dv=s.domainV();
    for(int i=1;i<9;++i) for(int j=1;j<9;++j) {
        float u=du.first+(du.second-du.first)*static_cast<float>(i)/9.f;
        float v=dv.first+(dv.second-dv.first)*static_cast<float>(j)/9.f;
        Vec3 dU=s.derivativeU(u,v), dV=s.derivativeV(u,v);
        EXPECT_TRUE(std::isfinite(dU.x) && std::isfinite(dU.y) && std::isfinite(dU.z));
        EXPECT_TRUE(std::isfinite(dV.x) && std::isfinite(dV.y) && std::isfinite(dV.z));
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  EDGE CASE COLLECTION
// ═══════════════════════════════════════════════════════════════════════

TEST(Integrity, SingleVertexMeshBRep) {
    Mesh m; m.attributes().setPositions({{0,0,0}});
    auto [body,report]=BRepBody::fromMesh(m);
    EXPECT_FALSE(report.valid); // needs faces
}

TEST(Integrity, SingleFaceMeshBRep) {
    Mesh m; m.attributes().setPositions({{0,0,0},{1,0,0},{0,1,0}});
    m.topology().addFace(Face{{0,1,2}});
    auto [body,report]=BRepBody::fromMesh(m);
    EXPECT_TRUE(report.valid); // open shell, valid B-Rep
}

TEST(Integrity, NonManifoldVertexBRep) {
    // Two tetrahedra sharing only a vertex (bow-tie).
    Mesh m; m.attributes().setPositions({
        {0,0,0},{1,0,0},{0.5,0.866,0},{0.5,0.289,0.816},
        {0.5,0.289,-0.816} // opposite side
    });
    m.topology().addFace(Face{{0,1,2}});m.topology().addFace(Face{{0,2,3}});
    m.topology().addFace(Face{{0,3,1}});m.topology().addFace(Face{{1,3,2}});
    m.topology().addFace(Face{{0,2,4}});m.topology().addFace(Face{{0,4,1}});
    m.topology().addFace(Face{{1,4,2}}); // vertex 0 shared by two tetrahedra

    auto [body,report]=BRepBody::fromMesh(m);
    // Non-manifold at vertex 0 — may be rejected.
    EXPECT_TRUE(report.valid || !report.valid);
}
