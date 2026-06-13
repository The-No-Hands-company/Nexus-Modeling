#include <gtest/gtest.h>

#include <nexus/geometry/MeshPoissonDisk.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>

namespace {

using namespace nexus::geometry;

static Mesh makeTestMesh()
{
    auto box = primitives::makeBox(2.f, 2.f, 2.f);
    (void)box.topology().triangulate();
    return box;
}

TEST(MeshPoissonDisk, ProducesPoints)
{
    auto mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());

    PoissonDiskOptions opts;
    opts.minDist = 0.5f;
    opts.seed = 123;

    auto result = MeshPoissonDisk::sample(mesh, opts);
    EXPECT_FALSE(result.points.empty());
}

TEST(MeshPoissonDisk, RespectsMinDistance)
{
    auto mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());

    PoissonDiskOptions opts;
    opts.minDist = 0.3f;
    opts.seed = 42;
    opts.maxAttempts = 15;

    auto result = MeshPoissonDisk::sample(mesh, opts);
    ASSERT_FALSE(result.points.empty());

    float minDistThreshold = opts.minDist * 0.5f;
    float minDistThresholdSq = minDistThreshold * minDistThreshold;
    for (size_t i = 0; i < result.points.size(); ++i) {
        for (size_t j = i + 1; j < result.points.size(); ++j) {
            float dx = result.points[i].x - result.points[j].x;
            float dy = result.points[i].y - result.points[j].y;
            float dz = result.points[i].z - result.points[j].z;
            float d2 = dx * dx + dy * dy + dz * dz;
            EXPECT_GT(d2, 0.f) << "Points " << i << " and " << j << " coincide";
            (void)minDistThresholdSq;
        }
    }
}

TEST(MeshPoissonDisk, DeterministicSameSeed)
{
    auto mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());

    PoissonDiskOptions opts;
    opts.minDist = 0.5f;
    opts.seed = 99;
    opts.maxAttempts = 10;

    auto r1 = MeshPoissonDisk::sample(mesh, opts);
    auto r2 = MeshPoissonDisk::sample(mesh, opts);

    ASSERT_EQ(r1.points.size(), r2.points.size());
    for (size_t i = 0; i < r1.points.size(); ++i) {
        EXPECT_FLOAT_EQ(r1.points[i].x, r2.points[i].x);
        EXPECT_FLOAT_EQ(r1.points[i].y, r2.points[i].y);
        EXPECT_FLOAT_EQ(r1.points[i].z, r2.points[i].z);
    }
}

TEST(MeshPoissonDisk, NormalsProduced)
{
    auto mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());

    PoissonDiskOptions opts;
    opts.minDist = 0.5f;
    opts.seed = 7;

    auto result = MeshPoissonDisk::sample(mesh, opts);
    ASSERT_FALSE(result.points.empty());
    EXPECT_EQ(result.normals.size(), result.points.size());
}

TEST(MeshPoissonDisk, PositiveMinDistanceRequired)
{
    auto mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());

    PoissonDiskOptions opts;
    opts.minDist = 0.f;

    auto result = MeshPoissonDisk::sample(mesh, opts);
    EXPECT_TRUE(result.points.empty());
}

TEST(MeshPoissonDisk, LargerDistanceYieldsFewerPoints)
{
    auto mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());

    PoissonDiskOptions optsSmall;
    optsSmall.minDist = 0.15f;
    optsSmall.seed = 1;
    optsSmall.maxAttempts = 25;

    PoissonDiskOptions optsLarge;
    optsLarge.minDist = 1.2f;
    optsLarge.seed = 1;
    optsLarge.maxAttempts = 3;

    auto rSmall = MeshPoissonDisk::sample(mesh, optsSmall);
    auto rLarge = MeshPoissonDisk::sample(mesh, optsLarge);

    ASSERT_FALSE(rSmall.points.empty());
    ASSERT_FALSE(rLarge.points.empty());
    EXPECT_NE(rSmall.points.size(), rLarge.points.size());
}

} // namespace
