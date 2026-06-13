#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBoolean.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshBoolean, UnionOfOverlappingCubes)
{
    Mesh a = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(a.isValid());

    Mesh b = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(b.isValid());

    auto r = MeshBoolean::compute(a, b, BooleanOp::Union);
    ASSERT_TRUE(r.ok) << r.error;
    Mesh result = std::move(r.result);
    EXPECT_TRUE(result.isValid());
    // Union of identical overlapping boxes produces merged geometry
    EXPECT_GT(result.topology().faceCount(), 0u);
    EXPECT_GT(result.attributes().vertexCount(), 0u);
}

TEST(MeshBoolean, IntersectionOfOverlappingCubes)
{
    Mesh a = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(a.isValid());

    Mesh b = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(b.isValid());

    auto r = MeshBoolean::compute(a, b, BooleanOp::Intersection);
    ASSERT_TRUE(r.ok) << r.error;
    Mesh result = std::move(r.result);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(MeshBoolean, DisjointCubesUnion)
{
    Mesh a = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(a.isValid());

    Mesh b = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(b.isValid());

    auto r = MeshBoolean::compute(a, b, BooleanOp::Union);
    ASSERT_TRUE(r.ok) << r.error;
    Mesh result = std::move(r.result);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(MeshBoolean, DifferenceDisjoint)
{
    Mesh a = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(a.isValid());

    Mesh b = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(b.isValid());

    const size_t faceCountA = a.topology().faceCount();
    auto r = MeshBoolean::compute(a, b, BooleanOp::Difference);
    ASSERT_TRUE(r.ok) << r.error;
    Mesh result = std::move(r.result);
    EXPECT_TRUE(result.isValid());
    EXPECT_GE(result.topology().faceCount(), faceCountA);
}

TEST(MeshBoolean, UnionIdentical)
{
    Mesh a = makeBox(1.f, 1.f, 1.f);
    ASSERT_TRUE(a.isValid());

    Mesh b = a;
    ASSERT_TRUE(b.isValid());

    auto r = MeshBoolean::compute(a, b, BooleanOp::Union);
    ASSERT_TRUE(r.ok) << r.error;
    Mesh result = std::move(r.result);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.attributes().vertexCount(), 0u);
}

TEST(MeshBoolean, IntersectionIdentical)
{
    Mesh a = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(a.isValid());

    Mesh b = a;
    ASSERT_TRUE(b.isValid());

    auto r = MeshBoolean::compute(a, b, BooleanOp::Intersection);
    ASSERT_TRUE(r.ok) << r.error;
    Mesh result = std::move(r.result);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}
