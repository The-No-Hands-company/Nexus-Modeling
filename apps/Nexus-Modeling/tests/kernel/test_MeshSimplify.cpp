#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshSimplify.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshSimplify, TargetAboveCurrentDoesNothing)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const size_t originalFaces = box.topology().faceCount();
    ASSERT_GT(originalFaces, 0u);

    SimplifyOptions opts;
    opts.targetFaceCount = 1000;
    Mesh result = MeshSimplify::decimate(box, opts);

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.topology().faceCount(), originalFaces);
}

TEST(MeshSimplify, TargetBelowCurrentReturnsValidCopy)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const size_t originalFaces = box.topology().faceCount();

    SimplifyOptions opts;
    opts.targetFaceCount = 2;
    Mesh result = MeshSimplify::decimate(box, opts);

    EXPECT_TRUE(result.isValid());
    // aggressivedecimation is clamped to a safe minimum; result may have fewer faces
    EXPECT_LE(result.topology().faceCount(), originalFaces);
}

TEST(MeshSimplify, TargetZeroIsHandled)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    SimplifyOptions opts;
    opts.targetFaceCount = 0;
    Mesh result = MeshSimplify::decimate(box, opts);

    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(MeshSimplify, HandlesEmptyGracefully)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    SimplifyOptions opts;
    opts.targetFaceCount = 10;
    Mesh result = MeshSimplify::decimate(empty, opts);

    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.topology().faceCount(), 0u);
    EXPECT_EQ(result.attributes().vertexCount(), 0u);
}
