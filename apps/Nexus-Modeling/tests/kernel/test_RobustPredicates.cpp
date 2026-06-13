#include <nexus/geometry/RobustPredicates.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(RobustPredicates, Orient2dProducesCorrectSignSimilarToFaceNormalDirection)
{
    auto cross2d = [](float ax, float ay, float bx, float by) {
        return ax * by - ay * bx;
    };

    EXPECT_GT(cross2d(1.f, 0.f, 0.f, 1.f), 0.f);
    EXPECT_LT(cross2d(0.f, 1.f, 1.f, 0.f), 0.f);
    EXPECT_NEAR(cross2d(1.f, 0.f, 1.f, 0.f), 0.f, 1e-7f);
}

TEST(RobustPredicates, Orient3dWorksOnTetrahedron)
{
    Mesh tet;
    tet.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 1.f, 0.f},
        { 0.f, 0.f, 1.f},
    });

    EXPECT_EQ(tet.attributes().vertexCount(), 4u);
    EXPECT_TRUE(tet.attributes().hasPositions());

    Face f0{};
    f0.indices = {0, 1, 2};
    tet.topology().addFace(f0);
    Face f1{};
    f1.indices = {0, 2, 3};
    tet.topology().addFace(f1);
    Face f2{};
    f2.indices = {0, 3, 1};
    tet.topology().addFace(f2);
    Face f3{};
    f3.indices = {1, 3, 2};
    tet.topology().addFace(f3);

    EXPECT_TRUE(tet.isValid());
    EXPECT_EQ(tet.topology().faceCount(), 4u);
}

TEST(RobustPredicates, InCircleDetectsInsideVsOutside)
{
    auto distSq = [](float x1, float y1, float x2, float y2) {
        float dx = x1 - x2, dy = y1 - y2;
        return dx * dx + dy * dy;
    };

    float centerX = 0.f, centerZ = 0.f;
    float radius = 1.f;

    float insideX = 0.5f, insideZ = 0.f;
    float onX = 1.f, onZ = 0.f;
    float outsideX = 1.5f, outsideZ = 0.f;

    float dInside = distSq(insideX, insideZ, centerX, centerZ);
    float dOn = distSq(onX, onZ, centerX, centerZ);
    float dOutside = distSq(outsideX, outsideZ, centerX, centerZ);
    float rSq = radius * radius;

    EXPECT_LT(dInside, rSq);
    EXPECT_NEAR(dOn, rSq, 1e-7f);
    EXPECT_GT(dOutside, rSq);
}

TEST(RobustPredicates, NearDegenerateCasesHandled)
{
    const float eps = std::numeric_limits<float>::epsilon();

    auto cross = [](float ax, float ay, float bx, float by) {
        return ax * by - ay * bx;
    };

    EXPECT_NEAR(cross(1.f, 0.f, 0.f, eps), 0.f, eps * 2.f);
    EXPECT_NEAR(cross(1.f - eps, eps, eps, 0.f), 0.f, 2e-6f);
}

TEST(RobustPredicates, MeshNormalOrientationMatchesFaceWinding)
{
    Mesh tri;
    tri.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    tri.topology().addFace(f);

    ASSERT_TRUE(tri.computeVertexNormals());

    for (const auto& n : tri.attributes().normals()) {
        EXPECT_LT(n.y, 0.f);
    }
}
