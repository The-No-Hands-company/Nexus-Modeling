#include <nexus/geometry/MeshVoxelize.h>
#include <gtest/gtest.h>

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

static Mesh makeCube() {
    Mesh m;
    float s = 0.5f;
    m.attributes().setPositions({
        {-s,-s,-s},{ s,-s,-s},{ s,-s, s},{-s,-s, s},
        {-s, s,-s},{ s, s,-s},{ s, s, s},{-s, s, s}
    });
    uint32_t faces[12][3] = {
        {0,2,1},{0,3,2},{4,5,6},{4,6,7},
        {0,1,5},{0,5,4},{2,3,7},{2,7,6},
        {0,4,7},{0,7,3},{1,2,6},{1,6,5}
    };
    for (auto& f : faces) m.topology().addFace(Face{{f[0],f[1],f[2]}});
    return m;
}

TEST(MeshVoxelize, ProducesGrid) {
    MeshVoxelizeOptions opts; opts.resolution = 16;
    auto res = MeshVoxelize::voxelize(makeCube(), opts);
    ASSERT_TRUE(res.ok);
    EXPECT_GT(res.grid.nx, 0u);
    EXPECT_GT(res.grid.occupiedCount(), 0u);
}

TEST(MeshVoxelize, CenterIsOccupied) {
    MeshVoxelizeOptions opts; opts.resolution = 16;
    auto res = MeshVoxelize::voxelize(makeCube(), opts);
    ASSERT_TRUE(res.ok);
    uint32_t cx = res.grid.nx / 2, cy = res.grid.ny / 2, cz = res.grid.nz / 2;
    EXPECT_TRUE(res.grid.occupied(cx, cy, cz));
}

TEST(MeshVoxelize, FarCornerEmpty) {
    MeshVoxelizeOptions opts; opts.resolution = 16; opts.padding = 0.0f;
    auto res = MeshVoxelize::voxelize(makeCube(), opts);
    ASSERT_TRUE(res.ok);
    EXPECT_FALSE(res.grid.occupied(0, 0, 0));
}

TEST(MeshVoxelize, EmptyMeshFails) {
    Mesh empty; empty.attributes().setPositions({});
    auto res = MeshVoxelize::voxelize(empty);
    EXPECT_FALSE(res.ok);
}
