// Null-backend tests for Bidirectional Path Tracing (BDPT) (v0.18 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct BDPTFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain>    sc;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        SwapchainDesc sd{};
        sd.extent = { 640, 480 };
        sc = ctx->createSwapchain(sd);
        ASSERT_NE(sc, nullptr);
    }

    FrameStats renderOnce(Renderer& r) {
        SceneGraph scene;
        Camera cam;
        r.beginFrame();
        r.render(cam, scene);
        r.endFrame();
        return r.lastFrameStats();
    }
};

} // namespace

TEST_F(BDPTFixture, BDPTSettingsDefaultDisabled) {
    BDPTSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(BDPTFixture, BDPTSettingsDefaultPathLengths) {
    BDPTSettings s;
    EXPECT_EQ(s.lightPathLength, 4u);
    EXPECT_EQ(s.eyePathLength,   4u);
}

TEST_F(BDPTFixture, BDPTSettingsDefaultMIS) {
    BDPTSettings s;
    EXPECT_TRUE(s.mis);
}

TEST_F(BDPTFixture, SetAndGetBDPTSettings) {
    Renderer r(*ctx, *sc);
    BDPTSettings s;
    s.enabled         = true;
    s.lightPathLength = 6;
    s.eyePathLength   = 5;
    s.mis             = false;
    r.setBDPTSettings(s);
    EXPECT_TRUE (r.bdptSettings().enabled);
    EXPECT_EQ   (r.bdptSettings().lightPathLength, 6u);
    EXPECT_EQ   (r.bdptSettings().eyePathLength,   5u);
    EXPECT_FALSE(r.bdptSettings().mis);
}

TEST_F(BDPTFixture, BDPTGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    BDPTSettings s;
    s.enabled = true;
    r.setBDPTSettings(s);
    RendererSettings rs;
    rs.enableBDPT = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.bdptActive);
    EXPECT_EQ   (stats.bdptConnectionCount, 0u);
}

TEST_F(BDPTFixture, BDPTGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    BDPTSettings s;
    s.enabled         = true;
    s.lightPathLength = 4;
    s.eyePathLength   = 4;
    r.setBDPTSettings(s);
    RendererSettings rs;
    rs.enableBDPT = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.bdptActive);
    EXPECT_GT  (stats.bdptConnectionCount, 0u);
}

TEST_F(BDPTFixture, BDPTConnectionCountReflectsPathLengths) {
    Renderer r(*ctx, *sc);
    BDPTSettings s;
    s.enabled         = true;
    s.lightPathLength = 2;
    s.eyePathLength   = 2;
    r.setBDPTSettings(s);
    RendererSettings rs;
    rs.enableBDPT = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    // connections = width × height × lightPathLength × eyePathLength
    const uint32_t expected = 640u * 480u * 2u * 2u;
    EXPECT_EQ(stats.bdptConnectionCount, expected);
}

TEST_F(BDPTFixture, BDPTInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    BDPTSettings s;
    s.enabled = false;
    r.setBDPTSettings(s);
    RendererSettings rs;
    rs.enableBDPT = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.bdptActive);
}
