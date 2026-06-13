#include <gtest/gtest.h>

#include <nexus/geometry/QuadRemesh.h>
#include <nexus/geometry/Mesh.h>

namespace {

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(QuadRemesh, RemeshProducesMeshWithFacesFromValidSurface)
{
    Mesh sphere = makeSphere(1.0f, 12, 12);

    QuadRemeshOptions opts;
    opts.resolutionU = 8;
    opts.resolutionV = 8;

    const Mesh result = QuadRemesh::remesh(sphere, opts);

    EXPECT_GT(result.attributes().vertexCount(), 0u);
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(QuadRemesh, GridSizeDeterminesVertexCount)
{
    Mesh plane = makePlane(4.0f, 4.0f, 4u, 4u);

    QuadRemeshOptions opts16;
    opts16.resolutionU = 16;
    opts16.resolutionV = 16;

    QuadRemeshOptions opts8;
    opts8.resolutionU = 8;
    opts8.resolutionV = 8;

    const Mesh coarse = QuadRemesh::remesh(plane, opts8);
    const Mesh fine   = QuadRemesh::remesh(plane, opts16);

    EXPECT_GT(fine.attributes().vertexCount(), coarse.attributes().vertexCount());
}

TEST(QuadRemesh, EmptySurfaceFails)
{
    Mesh empty;

    const Mesh result = QuadRemesh::remesh(empty);

    EXPECT_EQ(result.attributes().vertexCount(), 0u);
    EXPECT_EQ(result.topology().faceCount(), 0u);
}

} // namespace
