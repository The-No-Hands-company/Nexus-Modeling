#include <gtest/gtest.h>

#include <nexus/geometry/RailLoft.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/Mesh.h>

#include <vector>

namespace {

using namespace nexus::geometry;

static NurbsCurve makeXRail(float y)
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{0.f, y, 0.f},
            Vec3{4.f, y, 0.f},
        });
}

TEST(RailLoft, ParallelRailsProduceMeshWithFaces)
{
    auto rail0 = makeXRail(0.f);
    auto rail1 = makeXRail(2.f);

    RailLoft::ProfileLoop profile;
    profile.points = {
        Vec3{0.f, 0.f,  0.f},
        Vec3{0.f, 2.f,  0.f},
        Vec3{0.f, 2.f,  1.f},
        Vec3{0.f, 0.f,  1.f},
    };

    const Mesh result = RailLoft::loft(rail0, rail1, profile);

    EXPECT_GT(result.attributes().vertexCount(), 0u);
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(RailLoft, ProducesValidMesh)
{
    auto rail0 = makeXRail(0.f);
    auto rail1 = makeXRail(1.f);

    RailLoft::ProfileLoop profile;
    profile.points = {
        Vec3{0.f,  0.f,  0.f},
        Vec3{0.f,  1.f,  0.f},
        Vec3{0.f,  1.f,  0.5f},
        Vec3{0.f,  0.f,  0.5f},
    };

    const Mesh result = RailLoft::loft(rail0, rail1, profile);

    EXPECT_TRUE(result.isValid());
}

} // namespace
