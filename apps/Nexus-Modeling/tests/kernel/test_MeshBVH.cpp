#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>

#include <cmath>
#include <limits>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshBVH, BuildSucceedsForValidQuad)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);

    EXPECT_TRUE(bvh.isValid());
    EXPECT_FALSE(bvh.nodes().empty());
    EXPECT_FALSE(bvh.tris().empty());
}

TEST(MeshBVH, RayFromFrontHitsQuad)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {0.f, 3.f, 0.f};
    ray.direction = {0.f, -1.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_LT(hit.t, std::numeric_limits<float>::max());
    EXPECT_NEAR(hit.t, 3.f, 1e-4f);
}

TEST(MeshBVH, RayParallelToQuadMisses)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {2.f, 2.f, 0.f};
    ray.direction = {1.f, 0.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_EQ(hit.t, std::numeric_limits<float>::max());
    EXPECT_EQ(hit.triangleIndex, 0u);
}

TEST(MeshBVH, RayFromOtherSideHitsBackFace)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {0.f, -3.f, 0.f};
    ray.direction = {0.f, 1.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_LT(hit.t, std::numeric_limits<float>::max());
    EXPECT_NEAR(hit.t, 3.f, 1e-4f);
}

TEST(MeshBVH, EmptyMeshBuildFails)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    MeshBVH bvh;
    bvh.build(empty);

    EXPECT_FALSE(bvh.isValid());
    EXPECT_TRUE(bvh.nodes().empty());
    EXPECT_TRUE(bvh.tris().empty());
}

TEST(MeshBVH, HitPropertiesValidForFrontRay)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {0.f, 3.f, 0.f};
    ray.direction = {0.f, -1.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_LT(hit.t, std::numeric_limits<float>::max());
    EXPECT_EQ(hit.triangleIndex, 0u);
    EXPECT_GE(hit.u, 0.f);
    EXPECT_LE(hit.u, 1.f);
    EXPECT_GE(hit.v, 0.f);
    EXPECT_LE(hit.v, 1.f);
    EXPECT_LE(hit.u + hit.v, 1.f + 1e-5f);
}

TEST(MeshBVH, NodeHierarchyIsConsistent)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvh;
    bvh.build(box);
    ASSERT_TRUE(bvh.isValid());

    const auto& nodes = bvh.nodes();
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        if (n.isLeaf) {
            EXPECT_GE(n.triCount, 0);
            EXPECT_GE(n.firstTri, 0);
            continue;
        }
        // Internal node: children must exist and be within this node's bounds
        if (n.leftChild >= 0) {
            const auto& left = nodes[static_cast<size_t>(n.leftChild)];
            EXPECT_GE(left.min.x, n.min.x - 1e-5f);
            EXPECT_GE(left.min.y, n.min.y - 1e-5f);
            EXPECT_GE(left.min.z, n.min.z - 1e-5f);
            EXPECT_LE(left.max.x, n.max.x + 1e-5f);
            EXPECT_LE(left.max.y, n.max.y + 1e-5f);
            EXPECT_LE(left.max.z, n.max.z + 1e-5f);
        }
        if (n.leftChild >= 0 && n.leftChild + 1 < static_cast<int32_t>(nodes.size())) {
            const auto& right = nodes[static_cast<size_t>(n.leftChild + 1)];
            EXPECT_GE(right.min.x, n.min.x - 1e-5f);
            EXPECT_GE(right.min.y, n.min.y - 1e-5f);
            EXPECT_GE(right.min.z, n.min.z - 1e-5f);
            EXPECT_LE(right.max.x, n.max.x + 1e-5f);
            EXPECT_LE(right.max.y, n.max.y + 1e-5f);
            EXPECT_LE(right.max.z, n.max.z + 1e-5f);
        }
    }
}

TEST(MeshBVH, AllTrianglesReachable)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvh;
    bvh.build(box);
    ASSERT_TRUE(bvh.isValid());

    std::vector<bool> covered(bvh.tris().size(), false);
    for (const auto& n : bvh.nodes()) {
        if (!n.isLeaf) continue;
        for (int32_t j = 0; j < n.triCount; ++j) {
            int32_t idx = n.firstTri + j;
            ASSERT_GE(idx, 0);
            ASSERT_LT(static_cast<size_t>(idx), covered.size());
            covered[static_cast<size_t>(idx)] = true;
        }
    }
    for (size_t i = 0; i < covered.size(); ++i) {
        EXPECT_TRUE(covered[i]) << "triangle " << i << " not in any BVH leaf";
    }
}

TEST(MeshBVH, ClosestPointFindsQuery)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvh;
    bvh.build(box);
    ASSERT_TRUE(bvh.isValid());

    nexus::render::Vec3 query{2.0f, 2.0f, 2.0f};
    ClosestPointHit hit = bvh.closestPoint(query);
    EXPECT_TRUE(hit.valid);
    EXPECT_GT(hit.distanceSquared, 0.0f);
    EXPECT_LT(hit.distanceSquared, std::numeric_limits<float>::max());
}

TEST(MeshBVH, DeterministicBuild)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvhA, bvhB;
    bvhA.build(box);
    bvhB.build(box);

    ASSERT_EQ(bvhA.nodes().size(), bvhB.nodes().size());
    ASSERT_EQ(bvhA.tris().size(), bvhB.tris().size());

    for (size_t i = 0; i < bvhA.nodes().size(); ++i) {
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].min.x, bvhB.nodes()[i].min.x);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].min.y, bvhB.nodes()[i].min.y);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].min.z, bvhB.nodes()[i].min.z);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].max.x, bvhB.nodes()[i].max.x);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].max.y, bvhB.nodes()[i].max.y);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].max.z, bvhB.nodes()[i].max.z);
    }
}
