#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshCrossSection.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

using Vec3 = nexus::render::Vec3;

namespace {

bool allPointsOnPlane(const MeshCrossSection::Contour& c, const Vec3& origin, const Vec3& normal)
{
    for (const auto& p : c.points) {
        Vec3 d = p - origin;
        float dist = std::fabs(d.dot(normal));
        if (dist > 1e-4f) return false;
    }
    return true;
}

} // namespace

TEST(MeshCrossSection, HorizontalSliceProducesContours)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(mesh.isValid());

    Vec3 origin{0.f, 0.f, 0.f};
    Vec3 normal{0.f, 1.f, 0.f};
    std::vector<MeshCrossSection::Contour> contours =
        MeshCrossSection::slice(mesh, origin, normal);

    EXPECT_GT(contours.size(), 0u);
    for (const auto& c : contours) {
        EXPECT_GT(c.points.size(), 0u);
        EXPECT_TRUE(allPointsOnPlane(c, origin, normal));
    }
}

TEST(MeshCrossSection, AboveMeshSliceIsEmpty)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(mesh.isValid());

    std::vector<MeshCrossSection::Contour> contours =
        MeshCrossSection::slice(mesh, Vec3{0.f, 10.f, 0.f}, Vec3{0.f, 1.f, 0.f});

    EXPECT_EQ(contours.size(), 0u);
}

TEST(MeshCrossSection, ContoursAreClosed)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(mesh.isValid());

    Vec3 origin{0.f, 0.f, 0.f};
    Vec3 normal{0.f, 1.f, 0.f};
    std::vector<MeshCrossSection::Contour> contours =
        MeshCrossSection::slice(mesh, origin, normal);

    for (const auto& c : contours) {
        if (c.closed) {
            EXPECT_GT(c.points.size(), 2u);
        }
        if (c.points.size() >= 2) {
            const auto& first = c.points.front();
            const auto& last = c.points.back();
            float d = (first - last).length();
            EXPECT_LT(d, 1e-3f);
        }
    }
}
