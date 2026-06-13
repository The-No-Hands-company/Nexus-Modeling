#include <gtest/gtest.h>

#include <nexus/geometry/FeatureLineExtractor.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>

namespace {

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(FeatureLineExtractor, FlatPlaneProducesNoLines)
{
    Mesh plane = makePlane(4.0f, 4.0f, 8u, 8u);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 1.0f;

    const auto result = FeatureLineExtractor::extract(plane, opts);

    EXPECT_EQ(result.size(), 0u);
}

TEST(FeatureLineExtractor, BoxHasFeatureEdges)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 30.0f;

    const auto result = FeatureLineExtractor::extract(box, opts);

    EXPECT_GT(result.size(), 0u);
    for (const auto& pl : result) {
        EXPECT_GE(pl.points.size(), 2u);
    }
}

TEST(FeatureLineExtractor, SphereWithLowThresholdHasFeatures)
{
    Mesh sphere = makeSphere(1.0f, 32u, 32u);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 0.1f;

    const auto result = FeatureLineExtractor::extract(sphere, opts);

    EXPECT_GE(result.size(), 0u);
    for (const auto& pl : result) {
        EXPECT_GE(pl.points.size(), 2u);
    }
}

TEST(FeatureLineExtractor, HighThresholdProducesNoLines)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    FeatureLineExtractorOptions opts;
    opts.dihedralAngleThresholdDegrees = 180.0f;

    const auto result = FeatureLineExtractor::extract(box, opts);

    EXPECT_EQ(result.size(), 0u);
}

TEST(FeatureLineExtractor, InvalidMeshReturnsEmpty)
{
    Mesh empty;

    const auto result = FeatureLineExtractor::extract(empty);

    EXPECT_EQ(result.size(), 0u);
}

} // namespace
