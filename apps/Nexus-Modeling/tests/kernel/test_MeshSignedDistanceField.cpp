#include <gtest/gtest.h>

#include <nexus/geometry/MeshSignedDistanceField.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>

namespace {

using namespace nexus::geometry;

static Mesh makeClosedBox()
{
    auto box = primitives::makeBox(2.f, 2.f, 2.f);
    (void)box.topology().triangulate();
    return box;
}

TEST(MeshSignedDistanceField, BuildsForClosedMesh)
{
    auto mesh = makeClosedBox();
    ASSERT_TRUE(mesh.isValid());

    SDFOptions opts;
    opts.resolution = {32, 32, 32};
    opts.origin = {-2.f, -2.f, -2.f};
    opts.cellSize = {4.f / 32.f, 4.f / 32.f, 4.f / 32.f};

    auto result = MeshSignedDistanceField::compute(mesh, opts);
    EXPECT_EQ(result.resolution[0], 32u);
    EXPECT_EQ(result.resolution[1], 32u);
    EXPECT_EQ(result.resolution[2], 32u);
    EXPECT_GT(result.distances.size(), 0u);
}

TEST(MeshSignedDistanceField, CenterIsInside)
{
    auto mesh = makeClosedBox();
    ASSERT_TRUE(mesh.isValid());

    SDFOptions opts;
    opts.resolution = {16, 16, 16};
    opts.origin = {-2.f, -2.f, -2.f};
    opts.cellSize = {4.f / 16.f, 4.f / 16.f, 4.f / 16.f};

    auto result = MeshSignedDistanceField::compute(mesh, opts);

    size_t cx = 8, cy = 8, cz = 8;
    size_t rx = result.resolution[0];
    size_t ry = result.resolution[1];
    size_t idx = cz * rx * ry + cy * rx + cx;

    ASSERT_LT(idx, result.distances.size());
    EXPECT_LT(result.distances[idx], 0.f);
}

TEST(MeshSignedDistanceField, FarOutsideIsPositive)
{
    auto mesh = makeClosedBox();
    ASSERT_TRUE(mesh.isValid());

    SDFOptions opts;
    opts.resolution = {16, 16, 16};
    opts.origin = {2.f, 2.f, 2.f};
    opts.cellSize = {4.f / 16.f, 4.f / 16.f, 4.f / 16.f};

    auto result = MeshSignedDistanceField::compute(mesh, opts);

    size_t cx = 8, cy = 8, cz = 8;
    size_t rx = result.resolution[0];
    size_t ry = result.resolution[1];
    size_t idx = cz * rx * ry + cy * rx + cx;

    ASSERT_LT(idx, result.distances.size());
    EXPECT_GT(result.distances[idx], 0.f);
}

TEST(MeshSignedDistanceField, UnsignedDistanceAlwaysNonNegative)
{
    auto mesh = makeClosedBox();
    ASSERT_TRUE(mesh.isValid());

    SDFOptions opts;
    opts.resolution = {8, 8, 8};
    opts.origin = {-2.f, -2.f, -2.f};
    opts.cellSize = {4.f / 8.f, 4.f / 8.f, 4.f / 8.f};

    auto result = MeshSignedDistanceField::compute(mesh, opts);
    EXPECT_FALSE(result.distances.empty());

    for (float d : result.distances) {
        EXPECT_GE(std::fabs(d), 0.f);
    }
}

TEST(MeshSignedDistanceField, EmptyMeshFails)
{
    Mesh empty;

    SDFOptions opts;
    opts.resolution = {8, 8, 8};

    auto result = MeshSignedDistanceField::compute(empty, opts);
    EXPECT_TRUE(result.distances.empty());
}

TEST(MeshSignedDistanceField, ZeroResolutionFails)
{
    auto mesh = makeClosedBox();
    ASSERT_TRUE(mesh.isValid());

    SDFOptions opts;
    opts.resolution = {0, 0, 0};

    auto result = MeshSignedDistanceField::compute(mesh, opts);
    EXPECT_TRUE(result.distances.empty());
}

} // namespace
