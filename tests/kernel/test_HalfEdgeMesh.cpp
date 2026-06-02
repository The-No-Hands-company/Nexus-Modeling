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
    Face f;
    f.indices = {0, 1, 2};
    m.topology().addFace(std::move(f));
    return m;
}

static Mesh makeQuad()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    m.attributes() = std::move(attrs);
    Face f;
    f.indices = {0, 1, 2, 3};
    m.topology().addFace(std::move(f));
    return m;
}

// Two triangles sharing an edge: v0-v1-v2 and v0-v2-v3.
static Mesh makeTwoTris()
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

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(HalfEdgeMesh, FromSingleTriangle)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    EXPECT_EQ(hem->faceCount(), 1u);
    EXPECT_EQ(hem->vertexCount(), 3u);
    EXPECT_EQ(hem->halfEdgeCount(), 3u);
}

TEST(HalfEdgeMesh, FromTwoTrianglesSharedEdge)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTris());
    ASSERT_TRUE(hem.has_value());
    EXPECT_EQ(hem->faceCount(), 2u);
    EXPECT_EQ(hem->halfEdgeCount(), 6u);
}

TEST(HalfEdgeMesh, TwinLinksCorrect)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTris());
    ASSERT_TRUE(hem.has_value());
    // Count half-edges that have a valid twin.
    uint32_t twinCount = 0;
    for (const auto& he : hem->halfEdges()) {
        if (he.twin != kHEInvalid) {
            ++twinCount;
        }
    }
    EXPECT_EQ(twinCount, 2u); // shared edge has exactly one pair of twins
}

// ─── Topology predicates ──────────────────────────────────────────────────────

TEST(HalfEdgeMesh, SingleTriangleIsNotClosed)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    EXPECT_FALSE(hem->isClosed());
}

TEST(HalfEdgeMesh, SingleTriangleIsManifold)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    EXPECT_TRUE(hem->isManifold());
}

TEST(HalfEdgeMesh, SingleTriangleIsTriangulated)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    EXPECT_TRUE(hem->isTriangulated());
}

TEST(HalfEdgeMesh, QuadFaceNotTriangulated)
{
    auto hem = HalfEdgeMesh::fromMesh(makeQuad());
    ASSERT_TRUE(hem.has_value());
    EXPECT_FALSE(hem->isTriangulated());
}

// ─── Adjacency ────────────────────────────────────────────────────────────────

TEST(HalfEdgeMesh, FaceVerticesTriangle)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto verts = hem->faceVertices(0);
    ASSERT_EQ(verts.size(), 3u);
    // All indices 0,1,2 must appear.
    std::sort(verts.begin(), verts.end());
    EXPECT_EQ(verts[0], 0u);
    EXPECT_EQ(verts[1], 1u);
    EXPECT_EQ(verts[2], 2u);
}

TEST(HalfEdgeMesh, BoundaryLoopSingleTriangle)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto loops = hem->boundaryLoops();
    ASSERT_EQ(loops.size(), 1u);
    EXPECT_EQ(loops[0].size(), 3u);
}

// ─── Edge flip ────────────────────────────────────────────────────────────────

TEST(HalfEdgeMesh, FlipEdgeMaintainsHalfEdgeCount)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTris());
    ASSERT_TRUE(hem.has_value());
    const uint32_t heBefore = hem->halfEdgeCount();
    // Find a half-edge with a valid twin.
    uint32_t sharedHE = kHEInvalid;
    for (uint32_t i = 0; i < hem->halfEdgeCount(); ++i) {
        if (hem->halfEdges()[i].twin != kHEInvalid) {
            sharedHE = i;
            break;
        }
    }
    ASSERT_NE(sharedHE, kHEInvalid);
    EXPECT_TRUE(hem->flipEdge(sharedHE));
    EXPECT_EQ(hem->halfEdgeCount(), heBefore);
    EXPECT_EQ(hem->faceCount(), 2u);
}

TEST(HalfEdgeMesh, FlipBoundaryEdgeFails)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    // Every half-edge is a boundary edge.
    EXPECT_FALSE(hem->flipEdge(0));
}

// ─── Edge split ───────────────────────────────────────────────────────────────

TEST(HalfEdgeMesh, SplitEdgeAddsVertex)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    const uint32_t vBefore = hem->vertexCount();
    const uint32_t newV = hem->splitEdge(0);
    EXPECT_NE(newV, kHEInvalid);
    EXPECT_EQ(hem->vertexCount(), vBefore + 1);
}

// ─── Edge collapse ────────────────────────────────────────────────────────────

TEST(HalfEdgeMesh, CollapseEdgeTwoTriangles)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTris());
    ASSERT_TRUE(hem.has_value());
    // Find a half-edge with a valid twin (the shared interior edge).
    uint32_t sharedHE = kHEInvalid;
    for (uint32_t i = 0; i < hem->halfEdgeCount(); ++i) {
        if (hem->halfEdges()[i].twin != kHEInvalid) {
            sharedHE = i;
            break;
        }
    }
    ASSERT_NE(sharedHE, kHEInvalid);
    const uint32_t survivor = hem->collapseEdge(sharedHE);
    EXPECT_NE(survivor, kHEInvalid);
}

TEST(HalfEdgeMesh, CollapseBoundaryEdgeSingleTriangle)
{
    // Collapsing a boundary edge of a single triangle is a degenerate case.
    // The link condition should be satisfied (1 shared neighbour on boundary).
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    const uint32_t survivor = hem->collapseEdge(0);
    // May succeed or fail depending on link condition; just verify no crash.
    (void)survivor;
}

TEST(HalfEdgeMesh, CollapseInvalidIndexReturnsSentinel)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    EXPECT_EQ(hem->collapseEdge(9999u), kHEInvalid);
}
