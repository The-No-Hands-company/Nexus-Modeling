#include <nexus/geometry/MeshTopologyAnalysis.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <gtest/gtest.h>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static Mesh makeTriangle()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    m.attributes() = std::move(attrs);
    Face f; f.indices = {0, 1, 2};
    m.topology().addFace(std::move(f));
    return m;
}

// Two triangles sharing an edge: open strip.
static Mesh makeTwoTriStrip()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f, 0.f, 0.f}, // v0
        {1.f, 0.f, 0.f}, // v1
        {0.5f,1.f, 0.f}, // v2
        {1.5f,0.f, 0.f}, // v3
    });
    m.attributes() = std::move(attrs);
    Face f0; f0.indices = {0, 1, 2};
    Face f1; f1.indices = {1, 3, 2};
    m.topology().addFace(std::move(f0));
    m.topology().addFace(std::move(f1));
    return m;
}

// Tetrahedron — 4 triangles, 4 vertices, 6 edges.  χ=2, genus=0.
static Mesh makeTetrahedron()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({
        { 0.f,  0.f,  1.f}, // 0
        { 0.f,  1.f, -0.333f}, // 1
        {-0.816f,-0.5f,-0.333f}, // 2
        { 0.816f,-0.5f,-0.333f}, // 3
    });
    m.attributes() = std::move(attrs);
    Face f0; f0.indices = {0, 1, 2};
    Face f1; f1.indices = {0, 2, 3};
    Face f2; f2.indices = {0, 3, 1};
    Face f3; f3.indices = {1, 3, 2};
    m.topology().addFace(std::move(f0));
    m.topology().addFace(std::move(f1));
    m.topology().addFace(std::move(f2));
    m.topology().addFace(std::move(f3));
    return m;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(MeshTopologyAnalysis, SingleTriangleVertexEdgeFaceCount)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    EXPECT_EQ(result.vertexCount, 3u);
    EXPECT_EQ(result.edgeCount, 3u);
    EXPECT_EQ(result.faceCount, 1u);
}

TEST(MeshTopologyAnalysis, SingleTriangleEulerCharacteristic)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    // χ = 3 - 3 + 1 = 1
    EXPECT_EQ(result.eulerCharacteristic, 1);
}

TEST(MeshTopologyAnalysis, SingleTriangleBoundaryLoop)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    EXPECT_EQ(result.boundaryLoopCount, 1u);
    EXPECT_FALSE(result.isClosed);
}

TEST(MeshTopologyAnalysis, SingleTriangleOneComponent)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    EXPECT_EQ(result.connectedComponents, 1u);
}

TEST(MeshTopologyAnalysis, TwoTriangleStripEdgeCount)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTriStrip());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    // 4 vertices, 5 edges, 2 faces → χ = 4 - 5 + 2 = 1
    EXPECT_EQ(result.vertexCount, 4u);
    EXPECT_EQ(result.faceCount, 2u);
    EXPECT_EQ(result.eulerCharacteristic, 1);
}

TEST(MeshTopologyAnalysis, TwoTriangleStripOneComponent)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTriStrip());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    EXPECT_EQ(result.connectedComponents, 1u);
}

TEST(MeshTopologyAnalysis, TetrahedronIsClosed)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTetrahedron());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    EXPECT_TRUE(result.isClosed);
    EXPECT_EQ(result.boundaryLoopCount, 0u);
}

TEST(MeshTopologyAnalysis, TetrahedronEulerCharacteristic)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTetrahedron());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    // V=4, E=6, F=4 → χ = 4 - 6 + 4 = 2
    EXPECT_EQ(result.eulerCharacteristic, 2);
}

TEST(MeshTopologyAnalysis, TetrahedronGenusZero)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTetrahedron());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    // genus = (2 - χ) / 2 = (2 - 2) / 2 = 0 (sphere topology)
    EXPECT_EQ(result.genus, 0);
}

TEST(MeshTopologyAnalysis, TetrahedronIsTriangulated)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTetrahedron());
    ASSERT_TRUE(hem.has_value());
    const auto result = MeshTopologyAnalyser::analyse(*hem);
    EXPECT_TRUE(result.isTriangulated);
    EXPECT_TRUE(result.isManifold);
}
