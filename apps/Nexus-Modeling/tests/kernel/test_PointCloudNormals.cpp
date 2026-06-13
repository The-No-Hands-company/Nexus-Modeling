#include <gtest/gtest.h>

#include <nexus/geometry/PointCloudNormals.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

static std::vector<Vec3> makePlanarCloud(int32_t n)
{
    std::vector<Vec3> points;
    points.reserve(static_cast<size_t>(n * n));
    for (int32_t i = 0; i < n; ++i) {
        for (int32_t j = 0; j < n; ++j) {
            float x = -5.f + 10.f * static_cast<float>(i) / static_cast<float>(n - 1);
            float y = -5.f + 10.f * static_cast<float>(j) / static_cast<float>(n - 1);
            points.push_back({x, y, 0.f});
        }
    }
    return points;
}

TEST(PointCloudNormals, PlanarCloudRecoversZNormals)
{
    auto points = makePlanarCloud(8);
    ASSERT_GE(points.size(), 3u);

    PointCloudNormalsOptions opts;
    opts.orientToViewpoint = false;

    auto result = PointCloudNormals::estimate(points, opts);
    ASSERT_EQ(result.normals.size(), points.size());

    for (const auto& n : result.normals) {
        EXPECT_NEAR(std::fabs(n.z), 1.f, 0.2f);
        EXPECT_NEAR(n.x, 0.f, 0.2f);
        EXPECT_NEAR(n.y, 0.f, 0.2f);
    }
}

TEST(PointCloudNormals, ViewpointFlipsNormalsTowardCamera)
{
    auto points = makePlanarCloud(8);
    ASSERT_GE(points.size(), 3u);

    PointCloudNormalsOptions opts;
    opts.viewpoint = {0.f, 0.f, 10.f};
    opts.orientToViewpoint = true;

    auto result = PointCloudNormals::estimate(points, opts);
    ASSERT_EQ(result.normals.size(), points.size());

    for (const auto& n : result.normals) {
        Vec3 toView = opts.viewpoint - points[0];
        if (toView.length() > 1e-6f) {
            EXPECT_GE(n.dot({0.f, 0.f, 1.f}), -0.1f);
        }
    }
}

TEST(PointCloudNormals, TooFewPointsFails)
{
    std::vector<Vec3> points = {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}};
    ASSERT_LT(points.size(), 3u);

    auto result = PointCloudNormals::estimate(points);
    EXPECT_TRUE(result.normals.empty());
}

TEST(PointCloudNormals, DefaultOptionsWork)
{
    auto points = makePlanarCloud(4);
    ASSERT_GE(points.size(), 3u);

    auto result = PointCloudNormals::estimate(points);
    ASSERT_EQ(result.normals.size(), points.size());

    for (const auto& n : result.normals) {
        EXPECT_TRUE(std::isfinite(n.x));
        EXPECT_TRUE(std::isfinite(n.y));
        EXPECT_TRUE(std::isfinite(n.z));
    }
}

} // namespace
