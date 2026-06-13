#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshDisplace.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshDisplace, ScalarDisplacementShiftsVerticesAlongNormal)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();
    const size_t vc = origPos.size();

    Mesh result = MeshDisplace::displaceByScalar(box,
        [](const nexus::render::Vec3&) -> float { return 0.5f; });

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.attributes().vertexCount(), vc);

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < vc; ++i) {
        float distSq = (newPos[i].x - origPos[i].x) * (newPos[i].x - origPos[i].x) +
                       (newPos[i].y - origPos[i].y) * (newPos[i].y - origPos[i].y) +
                       (newPos[i].z - origPos[i].z) * (newPos[i].z - origPos[i].z);
        EXPECT_GT(distSq, 0.f);
    }
}

TEST(MeshDisplace, GlobalScaleAppliedCorrectly)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();

    DisplaceOptions optsSmall;
    optsSmall.magnitude = 0.1f;
    Mesh resultSmall = MeshDisplace::displaceByScalar(box,
        [](const nexus::render::Vec3&) -> float { return 1.f; }, optsSmall);

    DisplaceOptions optsLarge;
    optsLarge.magnitude = 1.f;
    Mesh resultLarge = MeshDisplace::displaceByScalar(box,
        [](const nexus::render::Vec3&) -> float { return 1.f; }, optsLarge);

    const auto& posSmall = resultSmall.attributes().positions();
    const auto& posLarge = resultLarge.attributes().positions();

    float maxDisplacementSmall = 0.f;
    float maxDisplacementLarge = 0.f;
    for (size_t i = 0; i < origPos.size(); ++i) {
        float dSmall = (posSmall[i].x - origPos[i].x) * (posSmall[i].x - origPos[i].x) +
                       (posSmall[i].y - origPos[i].y) * (posSmall[i].y - origPos[i].y) +
                       (posSmall[i].z - origPos[i].z) * (posSmall[i].z - origPos[i].z);
        float dLarge = (posLarge[i].x - origPos[i].x) * (posLarge[i].x - origPos[i].x) +
                       (posLarge[i].y - origPos[i].y) * (posLarge[i].y - origPos[i].y) +
                       (posLarge[i].z - origPos[i].z) * (posLarge[i].z - origPos[i].z);
        maxDisplacementSmall = std::max(maxDisplacementSmall, dSmall);
        maxDisplacementLarge = std::max(maxDisplacementLarge, dLarge);
    }
    EXPECT_GT(maxDisplacementLarge, maxDisplacementSmall);
}

TEST(MeshDisplace, EmptyMeshReturnsInvalid)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    Mesh result = MeshDisplace::displaceByScalar(empty,
        [](const nexus::render::Vec3&) -> float { return 1.f; });

    EXPECT_FALSE(result.isValid());
}

TEST(MeshDisplace, VectorDisplacementShiftsVertices)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();
    const size_t vc = origPos.size();

    Mesh result = MeshDisplace::displaceByVector(box,
        [](const nexus::render::Vec3& p) -> nexus::render::Vec3 {
            return {0.f, 0.5f, 0.f};
        });

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.attributes().vertexCount(), vc);

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < vc; ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x, 1e-4f);
        EXPECT_GT(newPos[i].y, origPos[i].y);
        EXPECT_NEAR(newPos[i].z, origPos[i].z, 1e-4f);
    }
}

TEST(MeshDisplace, VectorGlobalScale)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();

    DisplaceOptions opts;
    opts.magnitude = 2.f;

    Mesh result = MeshDisplace::displaceByVector(box,
        [](const nexus::render::Vec3&) -> nexus::render::Vec3 {
            return {1.f, 0.f, 0.f};
        }, opts);

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x + 2.f, 1e-4f);
    }
}

TEST(MeshDisplace, RecomputeNormalsWorks)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    ASSERT_TRUE(box.computeVertexNormals());

    DisplaceOptions opts;
    opts.recomputeNormals = true;

    Mesh result = MeshDisplace::displaceByScalar(box,
        [](const nexus::render::Vec3&) -> float { return 0.1f; }, opts);

    EXPECT_TRUE(result.attributes().hasNormals());
    for (const auto& n : result.attributes().normals()) {
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        EXPECT_NEAR(len, 1.f, 1e-4f);
    }
}
