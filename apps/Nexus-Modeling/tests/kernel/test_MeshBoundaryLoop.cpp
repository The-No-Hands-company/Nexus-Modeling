#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBoundaryLoop.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

namespace {

Mesh makeOpenQuad()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 1.f, 0.f, 1.f},
        { 0.f, 0.f, 1.f},
    };
    Face f{};
    f.indices = {0, 1, 2, 3};
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.topology().addFace(std::move(f));
    return mesh;
}

Mesh makeClosedTetrahedron()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.f,  0.f,  0.f},
        { 1.f,  0.f,  0.f},
        { 0.f,  1.f,  0.f},
        { 0.f,  0.f,  1.f},
    };
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    {
        Face f{};
        f.indices = {0, 2, 1};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {0, 1, 3};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {1, 2, 3};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {2, 0, 3};
        mesh.topology().addFace(std::move(f));
    }
    return mesh;
}

} // namespace

TEST(MeshBoundaryLoop, OpenQuadHasBoundary)
{
    Mesh mesh = makeOpenQuad();
    ASSERT_TRUE(mesh.isValid());

    BoundaryLoopResult result = MeshBoundaryLoop::extract(mesh);

    EXPECT_EQ(result.loops.size(), 1u);
    EXPECT_EQ(result.perimeters.size(), 1u);
}

TEST(MeshBoundaryLoop, ClosedTetrahedronHasNoBoundaries)
{
    Mesh mesh = makeClosedTetrahedron();
    ASSERT_TRUE(mesh.isValid());

    BoundaryLoopResult result = MeshBoundaryLoop::extract(mesh);

    EXPECT_EQ(result.loops.size(), 0u);
    EXPECT_EQ(result.perimeters.size(), 0u);
}

TEST(MeshBoundaryLoop, PerimeterIsPositive)
{
    Mesh mesh = makeOpenQuad();
    ASSERT_TRUE(mesh.isValid());

    BoundaryLoopResult result = MeshBoundaryLoop::extract(mesh);

    ASSERT_GE(result.perimeters.size(), 1u);
    EXPECT_GT(result.perimeters[0], 0.f);
}

TEST(MeshBoundaryLoop, EmptyMeshReturnsOkWithNoLoops)
{
    Mesh empty;

    BoundaryLoopResult result = MeshBoundaryLoop::extract(empty);

    EXPECT_EQ(result.loops.size(), 0u);
    EXPECT_EQ(result.perimeters.size(), 0u);
}
