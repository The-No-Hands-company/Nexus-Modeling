#include <gtest/gtest.h>

#include <nexus/geometry/MeshVoxelize.h>
#include <nexus/geometry/Mesh.h>

#include <algorithm>
#include <cmath>

namespace {

using namespace nexus::geometry;

static size_t countOccupied(const VoxelGrid& grid)
{
    return static_cast<size_t>(std::count(grid.occupancy.begin(),
                                          grid.occupancy.end(), true));
}

static Mesh makeLargeTriangle()
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.1f, 0.5f, 0.1f},
        {3.9f, 0.5f, 0.1f},
        {0.1f, 0.5f, 3.9f},
    });
    Face f;
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    return mesh;
}

TEST(MeshVoxelize, ProducesGridWithOccupiedCells)
{
    auto mesh = makeLargeTriangle();
    ASSERT_TRUE(mesh.isValid());

    VoxelizeOptions opts;
    opts.resolution = {4, 1, 4};
    opts.origin = {0.f, 0.f, 0.f};
    opts.voxelSize = {1.f, 1.f, 1.f};

    auto grid = MeshVoxelize::voxelize(mesh, opts);
    size_t occupied = countOccupied(grid);
    EXPECT_GT(occupied, 0u);
}

TEST(MeshVoxelize, SurfaceCellsAreMarked)
{
    auto mesh = makeLargeTriangle();
    ASSERT_TRUE(mesh.isValid());

    VoxelizeOptions opts;
    opts.resolution = {16, 1, 16};
    opts.origin = {0.f, 0.f, 0.f};
    opts.voxelSize = {4.f / 16.f, 1.f, 4.f / 16.f};

    auto grid = MeshVoxelize::voxelize(mesh, opts);
    size_t occupied = countOccupied(grid);
    EXPECT_GT(occupied, 5u);
}

TEST(MeshVoxelize, FarCornerEmptyWithSufficientPadding)
{
    auto mesh = makeLargeTriangle();
    ASSERT_TRUE(mesh.isValid());

    VoxelizeOptions opts;
    opts.resolution = {6, 2, 6};
    opts.origin = {0.f, 0.f, 0.f};
    opts.voxelSize = {1.f, 1.f, 1.f};

    auto grid = MeshVoxelize::voxelize(mesh, opts);
    EXPECT_FALSE(grid.get(5, 1, 5));
}

TEST(MeshVoxelize, EmptyMeshFails)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    auto grid = MeshVoxelize::voxelize(empty);
    EXPECT_EQ(grid.resolution[0], 32u);
    size_t occupied = countOccupied(grid);
    EXPECT_EQ(occupied, 0u);
}

} // namespace
