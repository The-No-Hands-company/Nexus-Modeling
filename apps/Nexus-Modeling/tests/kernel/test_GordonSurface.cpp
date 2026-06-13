#include <gtest/gtest.h>

#include <nexus/geometry/GordonSurface.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/Mesh.h>

#include <vector>

namespace {

using namespace nexus::geometry;

static NurbsCurve lineCurve(float x0, float y0, float x1, float y1)
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{x0, y0, 0.f},
            Vec3{x1, y1, 0.f},
        });
}

TEST(GordonSurface, BuildFromCrossingCurvesProducesMeshWithFaces)
{
    GordonSurface::CurveFamily uCurves;
    uCurves.curves = {
        lineCurve(0.f, 0.f, 3.f, 0.f),
        lineCurve(0.f, 1.f, 3.f, 1.f),
        lineCurve(0.f, 2.f, 3.f, 2.f),
    };

    GordonSurface::CurveFamily vCurves;
    vCurves.curves = {
        lineCurve(0.f, 0.f, 0.f, 2.f),
        lineCurve(1.f, 0.f, 1.f, 2.f),
        lineCurve(2.f, 0.f, 2.f, 2.f),
    };

    GordonSurfaceOptions opts;
    opts.samplesU = 16;
    opts.samplesV = 16;

    const Mesh result = GordonSurface::build(uCurves, vCurves, opts);

    EXPECT_GT(result.attributes().vertexCount(), 0u);
    EXPECT_GT(result.topology().faceCount(), 0u);
    EXPECT_TRUE(result.isValid());
}

TEST(GordonSurface, ProducesFlatPlaneWhenCurvesAreInSamePlane)
{
    GordonSurface::CurveFamily uCurves;
    uCurves.curves = {
        lineCurve(0.f, 0.f, 4.f, 0.f),
        lineCurve(0.f, 2.f, 4.f, 2.f),
        lineCurve(0.f, 4.f, 4.f, 4.f),
    };

    GordonSurface::CurveFamily vCurves;
    vCurves.curves = {
        lineCurve(0.f, 0.f, 0.f, 4.f),
        lineCurve(2.f, 0.f, 2.f, 4.f),
        lineCurve(4.f, 0.f, 4.f, 4.f),
    };

    const Mesh result = GordonSurface::build(uCurves, vCurves);

    ASSERT_GT(result.attributes().vertexCount(), 0u);
    for (const auto& p : result.attributes().positions()) {
        EXPECT_NEAR(p.z, 0.f, 1e-4f);
    }
}

TEST(GordonSurface, EmptyCurveListFails)
{
    GordonSurface::CurveFamily empty;
    EXPECT_TRUE(empty.curves.empty());

    const Mesh result = GordonSurface::build(empty, empty);

    EXPECT_EQ(result.topology().faceCount(), 0u);
    EXPECT_EQ(result.attributes().vertexCount(), 0u);
}

} // namespace
