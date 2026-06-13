#include <gtest/gtest.h>

#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static Mesh makeCubeMesh(float size)
{
    auto box = primitives::makeBox(size, size, size);
    (void)box.topology().triangulate();
    return box;
}

TEST(MeshMassProperties, UnitCubeVolumeApproxOne)
{
    auto cube = makeCubeMesh(1.f);
    ASSERT_TRUE(cube.isValid());
    auto mp = MeshMassProperties::compute(cube);
    EXPECT_NEAR(mp.volume, 1.f, 0.1f);
}

TEST(MeshMassProperties, CentroidAtOriginForCenteredCube)
{
    auto cube = makeCubeMesh(2.f);
    ASSERT_TRUE(cube.isValid());
    auto mp = MeshMassProperties::compute(cube);
    EXPECT_NEAR(mp.centroid.x, 0.f, 0.1f);
    EXPECT_NEAR(mp.centroid.y, 0.f, 0.1f);
    EXPECT_NEAR(mp.centroid.z, 0.f, 0.1f);
}

TEST(MeshMassProperties, PrincipalMomentsSortedDescending)
{
    auto cube = makeCubeMesh(2.f);
    ASSERT_TRUE(cube.isValid());
    auto mp = MeshMassProperties::compute(cube);
    EXPECT_GE(mp.principalMoments[0], mp.principalMoments[1]);
    EXPECT_GE(mp.principalMoments[1], mp.principalMoments[2]);
}

TEST(MeshMassProperties, EmptyMeshNotValid)
{
    Mesh empty;
    auto mp = MeshMassProperties::compute(empty);
    EXPECT_FLOAT_EQ(mp.volume, 0.f);
}

} // namespace
