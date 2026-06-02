#include <nexus/geometry/SubdivisionSurface.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace nexus::geometry;

// ─── Mesh builders ────────────────────────────────────────────────────────────

static Mesh makeTriangle()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({{0,0,0},{1,0,0},{0,1,0}});
    m.attributes() = std::move(attrs);
    Face f; f.indices = {0,1,2};
    m.topology().addFace(std::move(f));
    return m;
}

static Mesh makeTwoTris()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({{0,0,0},{1,0,0},{0.5f,1,0},{1.5f,0,0}});
    m.attributes() = std::move(attrs);
    Face f0; f0.indices = {0,1,2};
    Face f1; f1.indices = {1,3,2};
    m.topology().addFace(std::move(f0));
    m.topology().addFace(std::move(f1));
    return m;
}

static Mesh makeQuad()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({{0,0,0},{1,0,0},{1,1,0},{0,1,0}});
    m.attributes() = std::move(attrs);
    Face f; f.indices = {0,1,2,3};
    m.topology().addFace(std::move(f));
    return m;
}

// ─── Loop subdivision tests ───────────────────────────────────────────────────

TEST(SubdivisionSurface, LoopRejectsNonTriangulated)
{
    auto hem = HalfEdgeMesh::fromMesh(makeQuad());
    ASSERT_TRUE(hem.has_value());
    EXPECT_FALSE(SubdivisionSurface::loopSubdivide(*hem).has_value());
}

TEST(SubdivisionSurface, LoopSingleTriangleLevel1HasFourFaces)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->faceCount(), 4u);
}

TEST(SubdivisionSurface, LoopSingleTriangleLevel1HasSixVertices)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    // 3 original + 3 edge midpoints = 6
    EXPECT_EQ(sub->vertexCount(), 6u);
}

TEST(SubdivisionSurface, LoopTwoTrianglesLevel1HasEightFaces)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTris());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->faceCount(), 8u); // 2 tris × 4
}

TEST(SubdivisionSurface, LoopLevel2HasSixteenFaces)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {2, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->faceCount(), 16u); // 1 * 4^2
}

TEST(SubdivisionSurface, LoopLevel0ReturnsCopy)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {0, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->faceCount(), 1u);
}

TEST(SubdivisionSurface, LoopOutputIsTriangulated)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTwoTris());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_TRUE(sub->isTriangulated());
}

TEST(SubdivisionSurface, LoopBoundaryVerticesFixedWithCrease)
{
    // With smoothBoundary=false, original boundary vertices keep their positions.
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::loopSubdivide(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    // Corner (0,0,0) must appear in the output.
    bool found = false;
    for (const auto& v : sub->vertices()) {
        if (v.outgoing == kHEInvalid) { continue; }
        if (std::abs(v.position.x) < 1e-5f && std::abs(v.position.y) < 1e-5f)
            found = true;
    }
    EXPECT_TRUE(found);
}

// ─── Catmull-Clark tests ──────────────────────────────────────────────────────

TEST(SubdivisionSurface, CatmullClarkQuadLevel1HasFourFaces)
{
    auto hem = HalfEdgeMesh::fromMesh(makeQuad());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::catmullClark(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    // One quad face → 4 quads
    EXPECT_EQ(sub->faceCount(), 4u);
}

TEST(SubdivisionSurface, CatmullClarkTriangleLevel1HasThreeFaces)
{
    auto hem = HalfEdgeMesh::fromMesh(makeTriangle());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::catmullClark(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    // One triangle face → 3 quads
    EXPECT_EQ(sub->faceCount(), 3u);
}

TEST(SubdivisionSurface, CatmullClarkLevel0ReturnsCopy)
{
    auto hem = HalfEdgeMesh::fromMesh(makeQuad());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::catmullClark(*hem, {0, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(sub->faceCount(), 1u);
}

TEST(SubdivisionSurface, CatmullClarkOutputIsManifold)
{
    auto hem = HalfEdgeMesh::fromMesh(makeQuad());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::catmullClark(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    EXPECT_TRUE(sub->isManifold());
}

TEST(SubdivisionSurface, CatmullClarkFacePointIsAverageCringe)
{
    // For a single quad with vertices at (0,0), (2,0), (2,2), (0,2),
    // the face point must be the centroid (1,1,0) in the output.
    auto hem = HalfEdgeMesh::fromMesh(makeQuad());
    ASSERT_TRUE(hem.has_value());
    auto sub = SubdivisionSurface::catmullClark(*hem, {1, false});
    ASSERT_TRUE(sub.has_value());
    // The face point (1,1,0) must appear among output vertices.
    // The quad was (0,0),(1,0),(1,1),(0,1) → centroid (0.5, 0.5, 0).
    bool found = false;
    for (const auto& v : sub->vertices()) {
        if (std::abs(v.position.x - 0.5f) < 1e-4f &&
            std::abs(v.position.y - 0.5f) < 1e-4f &&
            std::abs(v.position.z)         < 1e-4f) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}
