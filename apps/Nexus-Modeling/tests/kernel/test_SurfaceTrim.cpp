#include <gtest/gtest.h>

#include <nexus/geometry/SurfaceTrim.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec2 = nexus::geometry::Vec2;

static TrimCurve2D makeRectangleCurve(float u0, float v0, float u1, float v1) {
    TrimCurve2D c;
    c.points = {
        {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}, {u0, v0},
    };
    c.closed = true;
    return c;
}

} // namespace nexus::geometry

using namespace nexus::geometry;

TEST(SurfaceTrim, SurfaceTrimEmptyByDefault) {
    SurfaceTrim st;
    EXPECT_TRUE(st.isEmpty());
    EXPECT_EQ(st.curveCount(), 0u);
}

TEST(SurfaceTrim, AddingLoopMakesNonEmpty) {
    SurfaceTrim st;
    TrimLoop loop;
    loop.curves.push_back(makeRectangleCurve(0.f, 0.f, 1.f, 1.f));
    st.addLoop(loop);
    EXPECT_FALSE(st.isEmpty());
    EXPECT_EQ(st.curveCount(), 1u);
}

TEST(SurfaceTrim, IsEmptyDetectsEmptyLoops) {
    SurfaceTrim empty;
    EXPECT_TRUE(empty.isEmpty());

    SurfaceTrim nonEmpty;
    TrimLoop loop;
    loop.curves.push_back(makeRectangleCurve(0.f, 0.f, 1.f, 1.f));
    nonEmpty.addLoop(loop);
    EXPECT_FALSE(nonEmpty.isEmpty());
}

TEST(SurfaceTrim, CurveCountReturnsCorrectCount) {
    SurfaceTrim st;
    EXPECT_EQ(st.curveCount(), 0u);

    {
        TrimLoop loop1;
        loop1.curves.push_back(makeRectangleCurve(0.f, 0.f, 0.5f, 0.5f));
        st.addLoop(loop1);
        EXPECT_EQ(st.curveCount(), 1u);
    }

    {
        TrimLoop loop2;
        loop2.curves.push_back(makeRectangleCurve(0.5f, 0.5f, 1.f, 1.f));
        loop2.curves.push_back(makeRectangleCurve(0.f, 0.5f, 0.5f, 1.f));
        st.addLoop(loop2);
        EXPECT_EQ(st.curveCount(), 3u);
    }
}

TEST(SurfaceTrim, ContainsPointWithEmptyLoopsReturnsTrue) {
    SurfaceTrim st;
    EXPECT_TRUE(st.containsPoint(0.5f, 0.5f));
}

TEST(SurfaceTrim, ContainsPointInsideOuterLoop) {
    SurfaceTrim st;
    TrimLoop outer;
    outer.curves.push_back(makeRectangleCurve(0.1f, 0.1f, 0.9f, 0.9f));
    outer.isOuterLoop = true;
    st.addLoop(outer);

    EXPECT_TRUE(st.containsPoint(0.5f, 0.5f));
    EXPECT_FALSE(st.containsPoint(0.05f, 0.05f));
    EXPECT_FALSE(st.containsPoint(0.95f, 0.95f));
}

TEST(SurfaceTrim, ContainsPointWithInnerHole) {
    SurfaceTrim st;

    TrimLoop outer;
    outer.curves.push_back(makeRectangleCurve(0.0f, 0.0f, 1.0f, 1.0f));
    outer.isOuterLoop = true;
    st.addLoop(outer);

    TrimLoop hole;
    hole.curves.push_back(makeRectangleCurve(0.3f, 0.3f, 0.7f, 0.7f));
    hole.isOuterLoop = false;
    st.addLoop(hole);

    EXPECT_TRUE(st.containsPoint(0.1f, 0.1f));
    EXPECT_FALSE(st.containsPoint(0.5f, 0.5f));
}

TEST(SurfaceTrim, MultipleOuterLoopsUnion) {
    SurfaceTrim st;

    TrimLoop outerA;
    outerA.curves.push_back(makeRectangleCurve(0.0f, 0.0f, 0.3f, 0.3f));
    outerA.isOuterLoop = true;
    st.addLoop(outerA);

    TrimLoop outerB;
    outerB.curves.push_back(makeRectangleCurve(0.7f, 0.7f, 1.0f, 1.0f));
    outerB.isOuterLoop = true;
    st.addLoop(outerB);

    EXPECT_TRUE(st.containsPoint(0.15f, 0.15f));
    EXPECT_TRUE(st.containsPoint(0.85f, 0.85f));
    EXPECT_FALSE(st.containsPoint(0.5f, 0.5f));
}

TEST(SurfaceTrim, MultipleHolesExclusion) {
    SurfaceTrim st;

    TrimLoop outer;
    outer.curves.push_back(makeRectangleCurve(0.0f, 0.0f, 1.0f, 1.0f));
    outer.isOuterLoop = true;
    st.addLoop(outer);

    TrimLoop hole1;
    hole1.curves.push_back(makeRectangleCurve(0.1f, 0.1f, 0.3f, 0.3f));
    hole1.isOuterLoop = false;
    st.addLoop(hole1);

    TrimLoop hole2;
    hole2.curves.push_back(makeRectangleCurve(0.6f, 0.6f, 0.8f, 0.8f));
    hole2.isOuterLoop = false;
    st.addLoop(hole2);

    EXPECT_TRUE(st.containsPoint(0.5f, 0.5f));
    EXPECT_FALSE(st.containsPoint(0.2f, 0.2f));
    EXPECT_FALSE(st.containsPoint(0.7f, 0.7f));
}

TEST(SurfaceTrim, HoleOnlyReturnsOutside) {
    SurfaceTrim st;

    TrimLoop hole;
    hole.curves.push_back(makeRectangleCurve(0.2f, 0.2f, 0.8f, 0.8f));
    hole.isOuterLoop = false;
    st.addLoop(hole);

    EXPECT_FALSE(st.containsPoint(0.5f, 0.5f));
}

TEST(SurfaceTrim, NURBSTrimCurve) {
    SurfaceTrim st;

    std::vector<float> knots = {0.f, 0.f, 0.f, 0.25f, 0.5f, 0.75f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.0f, 0.0f, 0.f},
        {0.3f, 0.0f, 0.f},
        {0.3f, 0.3f, 0.f},
        {0.0f, 0.3f, 0.f},
        {0.0f, 0.0f, 0.f},
        {0.0f, 0.0f, 0.f},
    };
    NurbsCurve nurb(2, std::move(knots), std::move(ctl));

    TrimCurve2D curve;
    curve.nurbsCurve = nurb;
    curve.closed = true;

    TrimLoop loop;
    loop.curves.push_back(curve);
    loop.isOuterLoop = true;
    st.addLoop(loop);

    EXPECT_TRUE(st.containsPoint(0.05f, 0.05f));
    EXPECT_FALSE(st.containsPoint(0.5f, 0.5f));
}

TEST(SurfaceTrim, MixedPolylineAndNURBS) {
    SurfaceTrim st;

    TrimCurve2D polyCurve = makeRectangleCurve(0.0f, 0.0f, 1.0f, 1.0f);

    std::vector<float> knots = {0.f, 0.f, 0.f, 0.25f, 0.5f, 0.75f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.3f, 0.3f, 0.f},
        {0.7f, 0.3f, 0.f},
        {0.7f, 0.7f, 0.f},
        {0.3f, 0.7f, 0.f},
        {0.3f, 0.3f, 0.f},
        {0.3f, 0.3f, 0.f},
    };
    NurbsCurve nurb(2, std::move(knots), std::move(ctl));

    TrimCurve2D nurbCurve;
    nurbCurve.nurbsCurve = nurb;
    nurbCurve.closed = true;

    TrimLoop outer;
    outer.curves.push_back(polyCurve);
    outer.isOuterLoop = true;
    st.addLoop(outer);

    TrimLoop hole;
    hole.curves.push_back(nurbCurve);
    hole.isOuterLoop = false;
    st.addLoop(hole);

    EXPECT_TRUE(st.containsPoint(0.1f, 0.1f));
    EXPECT_FALSE(st.containsPoint(0.5f, 0.5f));
}
