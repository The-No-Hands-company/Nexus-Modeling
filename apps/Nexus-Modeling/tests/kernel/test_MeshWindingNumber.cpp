#include <gtest/gtest.h>

#include <nexus/geometry/MeshWindingNumber.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>

namespace {

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

static Mesh makeClosedCube()
{
    auto box = primitives::makeBox(2.f, 2.f, 2.f);
    (void)box.topology().triangulate();
    return box;
}

TEST(MeshWindingNumber, InsideReturnsHighWinding)
{
    auto cube = makeClosedCube();
    ASSERT_TRUE(cube.isValid());

    Vec3 center{0.f, 0.f, 0.f};
    float wn = MeshWindingNumber::at(cube, center);

    EXPECT_GT(std::fabs(wn), 0.9f);
}

TEST(MeshWindingNumber, OutsideReturnsLowWinding)
{
    auto cube = makeClosedCube();
    ASSERT_TRUE(cube.isValid());

    Vec3 farPoint{5.f, 5.f, 5.f};
    float wn = MeshWindingNumber::at(cube, farPoint);

    EXPECT_LT(std::fabs(wn), 0.1f);
}

TEST(MeshWindingNumber, BatchQueryReturnsCorrectCount)
{
    auto cube = makeClosedCube();
    ASSERT_TRUE(cube.isValid());

    std::vector<Vec3> queries = {
        {0.f, 0.f, 0.f},
        {5.f, 0.f, 0.f},
        {-5.f, 0.f, 0.f},
        {0.f, 0.f, 5.f},
    };

    auto results = MeshWindingNumber::batch(cube, queries);
    EXPECT_EQ(results.size(), queries.size());

    for (float r : results) {
        EXPECT_TRUE(std::isfinite(r));
    }
}

TEST(MeshWindingNumber, IsInsideCenterReturnsTrue)
{
    auto cube = makeClosedCube();
    ASSERT_TRUE(cube.isValid());

    Vec3 center{0.f, 0.f, 0.f};
    EXPECT_TRUE(MeshWindingNumber::isInside(cube, center));
}

TEST(MeshWindingNumber, IsInsideFarOutsideReturnsFalse)
{
    auto cube = makeClosedCube();
    ASSERT_TRUE(cube.isValid());

    Vec3 farPoint{10.f, 10.f, 10.f};
    EXPECT_FALSE(MeshWindingNumber::isInside(cube, farPoint));
}

} // namespace
