#include <gtest/gtest.h>

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshFillet.h>

using namespace nexus::geometry;

TEST(MeshFillet, FilletOnCubeEdgeCreatesNewVertices)
{
    // Unit cube mesh.
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    });
    auto& t = cube.topology();
    t.addFace(Face{{0,2,1}}); t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}}); t.addFace(Face{{4,6,7}});
    t.addFace(Face{{0,1,5}}); t.addFace(Face{{0,5,4}});
    t.addFace(Face{{2,3,7}}); t.addFace(Face{{2,7,6}});
    t.addFace(Face{{0,4,7}}); t.addFace(Face{{0,7,3}});
    t.addFace(Face{{1,2,6}}); t.addFace(Face{{1,6,5}});

    auto heOpt = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh heMesh = *heOpt;

    // Find the edge from v0 to v1 (bottom-front edge).
    uint32_t he = heMesh.findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    FilletDesc desc;
    desc.radius = 0.1f;

    uint32_t vcBefore = heMesh.vertexCount();
    FilletReport report = MeshFilletOperation::fillet(heMesh, {he}, desc);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.edgesProcessed, 1u);
    EXPECT_GT(report.verticesAdded, 0u);
    EXPECT_GT(heMesh.vertexCount(), vcBefore);

    // After fillet, convert back to Mesh to verify topology validity.
    Mesh result = heMesh.toMesh();
    EXPECT_GT(result.topology().faceCount(), cube.topology().faceCount());
}

TEST(MeshFillet, ZeroRadiusIsNoOp)
{
    Mesh tri;
    tri.attributes().setPositions({
        {0,0,0}, {2,0,0}, {1,1,0}, {1,0,1}
    });
    tri.topology().addFace(Face{{0,1,2}});
    tri.topology().addFace(Face{{0,1,3}});

    auto heOpt = HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh heMesh = *heOpt;

    uint32_t he = heMesh.findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    FilletDesc desc;
    desc.radius = 0.f;

    uint32_t vc = heMesh.vertexCount();
    FilletReport report = MeshFilletOperation::fillet(heMesh, {he}, desc);

    EXPECT_FALSE(report.valid);
    EXPECT_EQ(heMesh.vertexCount(), vc);  // no change
}

TEST(MeshFillet, BoundaryEdgeSkipped)
{
    Mesh tri;
    tri.attributes().setPositions({
        {0,0,0}, {2,0,0}, {1,1,0}
    });
    tri.topology().addFace(Face{{0,1,2}});

    auto heOpt = HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh heMesh = *heOpt;

    // Edge (0,1) is a boundary edge.
    uint32_t he = heMesh.findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    FilletDesc desc;
    desc.radius = 0.2f;

    uint32_t vc = heMesh.vertexCount();
    FilletReport report = MeshFilletOperation::fillet(heMesh, {he}, desc);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.edgesProcessed, 0u);  // boundary, skipped
    EXPECT_EQ(heMesh.vertexCount(), vc);
}

TEST(MeshFillet, MultiSegmentFillet)
{
    Mesh tri;
    tri.attributes().setPositions({
        {0,0,0}, {2,0,0}, {1,1,0}, {1,-1,0}
    });
    tri.topology().addFace(Face{{0,1,2}});
    tri.topology().addFace(Face{{0,3,1}});

    auto heOpt = HalfEdgeMesh::fromMesh(tri);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh heMesh = *heOpt;

    uint32_t he = heMesh.findEdge(0, 1);
    ASSERT_NE(he, HalfEdgeMesh::kInvalid);

    FilletDesc desc;
    desc.radius = 0.15f;

    uint32_t vcBefore = heMesh.vertexCount();
    FilletReport report = MeshFilletOperation::fillet(heMesh, {he}, desc);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.edgesProcessed, 1u);
    EXPECT_EQ(report.verticesAdded, 1u);
    EXPECT_EQ(heMesh.vertexCount(), vcBefore + 1);
}

TEST(MeshFillet, MultipleEdges)
{
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
    });
    auto& t = cube.topology();
    t.addFace(Face{{0,2,1}}); t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}}); t.addFace(Face{{4,6,7}});
    t.addFace(Face{{0,1,5}}); t.addFace(Face{{0,5,4}});
    t.addFace(Face{{2,3,7}}); t.addFace(Face{{2,7,6}});
    t.addFace(Face{{0,4,7}}); t.addFace(Face{{0,7,3}});
    t.addFace(Face{{1,2,6}}); t.addFace(Face{{1,6,5}});

    auto heOpt = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh heMesh = *heOpt;

    uint32_t he0 = heMesh.findEdge(0, 1);
    uint32_t he1 = heMesh.findEdge(0, 4);
    ASSERT_NE(he0, HalfEdgeMesh::kInvalid);
    ASSERT_NE(he1, HalfEdgeMesh::kInvalid);

    FilletDesc desc;
    desc.radius = 0.1f;

    FilletReport report = MeshFilletOperation::fillet(heMesh, {he0, he1}, desc);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.edgesProcessed, 2u);
    EXPECT_GT(heMesh.vertexCount(), 8u);
}
