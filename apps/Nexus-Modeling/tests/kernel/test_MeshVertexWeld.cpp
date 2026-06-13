#include <gtest/gtest.h>

#include <nexus/geometry/MeshVertexWeld.h>
#include <nexus/geometry/Mesh.h>

namespace {

using namespace nexus::geometry;

static Mesh makeMeshWithDuplicates()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},   // 0 - unique
        {1.f, 0.f, 0.f},   // 1
        {0.f, 1.f, 0.f},   // 2
        {1.f, 0.f, 0.f},   // 3 - duplicate of 1
        {0.f, 1.f, 0.f},   // 4 - duplicate of 2
    });
    Face f1;
    f1.indices = {0, 1, 2};
    mesh.topology().addFace(f1);
    Face f2;
    f2.indices = {0, 3, 4};
    mesh.topology().addFace(f2);
    return mesh;
}

static Mesh makeMeshAllUnique()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    Face f;
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    return mesh;
}

TEST(MeshVertexWeld, ReducesCountWithDuplicates)
{
    auto mesh = makeMeshWithDuplicates();
    ASSERT_TRUE(mesh.isValid());
    size_t origCount = mesh.attributes().vertexCount();
    EXPECT_EQ(origCount, 5u);
    Mesh welded = MeshVertexWeld::weld(mesh);
    ASSERT_TRUE(welded.isValid());
    EXPECT_LT(welded.attributes().vertexCount(), origCount);
}

TEST(MeshVertexWeld, AllUniqueStaysSame)
{
    auto mesh = makeMeshAllUnique();
    ASSERT_TRUE(mesh.isValid());
    size_t origCount = mesh.attributes().vertexCount();
    Mesh welded = MeshVertexWeld::weld(mesh);
    ASSERT_TRUE(welded.isValid());
    EXPECT_EQ(welded.attributes().vertexCount(), origCount);
}

TEST(MeshVertexWeld, CountUniqueReturnsCorrectCount)
{
    auto mesh = makeMeshWithDuplicates();
    ASSERT_TRUE(mesh.isValid());
    size_t unique = MeshVertexWeld::countUnique(mesh);
    EXPECT_EQ(unique, 3u);
}

TEST(MeshVertexWeld, RebuildsValidMeshWithAllUnique)
{
    auto mesh = makeMeshWithDuplicates();
    ASSERT_TRUE(mesh.isValid());
    Mesh welded = MeshVertexWeld::weld(mesh);
    ASSERT_TRUE(welded.isValid());
    size_t unique = MeshVertexWeld::countUnique(welded);
    EXPECT_EQ(unique, welded.attributes().vertexCount());
}

} // namespace
