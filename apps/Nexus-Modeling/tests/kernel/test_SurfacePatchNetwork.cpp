#include <gtest/gtest.h>

#include <nexus/geometry/SurfacePatchNetwork.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static NurbsSurface makePatch(float offsetX, float offsetY) {
    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {offsetX + 0.f, offsetY + 0.f, 0.f}, {offsetX + 1.f, offsetY + 0.f, 0.f},
        {offsetX + 2.f, offsetY + 0.f, 0.f}, {offsetX + 3.f, offsetY + 0.f, 0.f},
        {offsetX + 0.f, offsetY + 1.f, 0.f}, {offsetX + 1.f, offsetY + 1.f, 0.f},
        {offsetX + 2.f, offsetY + 1.f, 0.f}, {offsetX + 3.f, offsetY + 1.f, 0.f},
        {offsetX + 0.f, offsetY + 2.f, 0.f}, {offsetX + 1.f, offsetY + 2.f, 0.f},
        {offsetX + 2.f, offsetY + 2.f, 0.f}, {offsetX + 3.f, offsetY + 2.f, 0.f},
        {offsetX + 0.f, offsetY + 3.f, 0.f}, {offsetX + 1.f, offsetY + 3.f, 0.f},
        {offsetX + 2.f, offsetY + 3.f, 0.f}, {offsetX + 3.f, offsetY + 3.f, 0.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

TEST(SurfacePatchNetwork, AddPatchReturnsValidIndex) {
    SurfacePatchNetwork net;
    EXPECT_EQ(net.patchCount(), 0u);

    net.addPatch(makePatch(0.f, 0.f));
    EXPECT_EQ(net.patchCount(), 1u);

    net.addPatch(makePatch(3.f, 0.f));
    EXPECT_EQ(net.patchCount(), 2u);
}

TEST(SurfacePatchNetwork, ConnectBetweenTwoPatchesSucceeds) {
    SurfacePatchNetwork net;
    net.addPatch(makePatch(0.f, 0.f));
    net.addPatch(makePatch(3.f, 0.f));
    ASSERT_EQ(net.patchCount(), 2u);

    net.connect(0, 2, 1, 0);

    auto n0 = net.neighbors(0);
    EXPECT_GE(n0.size(), 1u);
    if (!n0.empty()) {
        EXPECT_EQ(n0[0].patch, 1);
    }

    auto n1 = net.neighbors(1);
    if (!n1.empty()) {
        EXPECT_EQ(n1[0].patch, 0);
    }
}

TEST(SurfacePatchNetwork, NeighborsReturnsConnectedPatch) {
    SurfacePatchNetwork net;
    net.addPatch(makePatch(0.f, 0.f));
    net.addPatch(makePatch(3.f, 0.f));
    net.addPatch(makePatch(0.f, 3.f));
    ASSERT_EQ(net.patchCount(), 3u);

    net.connect(0, 2, 1, 0);
    net.connect(0, 3, 2, 1);

    auto n = net.neighbors(0);
    EXPECT_EQ(n.size(), 2u);
}

TEST(SurfacePatchNetwork, TessellateProducesMeshWithFaces) {
    SurfacePatchNetwork net;
    net.addPatch(makePatch(0.f, 0.f));
    ASSERT_EQ(net.patchCount(), 1u);

    Mesh result = net.tessellate(8, 8);
    ASSERT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
    EXPECT_TRUE(result.attributes().hasPositions());
}
