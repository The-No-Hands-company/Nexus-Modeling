// Null-backend tests for Neural Radiance Fields (NeRF) rendering (v0.22 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct NeRFFixture : public ::testing::Test {
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

TEST_F(NeRFFixture, NeRFSettingsDefaultDisabled) {
    NeRFSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(NeRFFixture, NeRFSettingsDefaultMarchSteps) {
    NeRFSettings s;
    EXPECT_EQ(s.marchSteps, 64u);
}

TEST_F(NeRFFixture, NeRFSettingsDefaultNetworkLayers) {
    NeRFSettings s;
    EXPECT_EQ(s.networkLayers, 8u);
}

TEST_F(NeRFFixture, NeRFSettingsDefaultPosEncodingFreqs) {
    NeRFSettings s;
    EXPECT_EQ(s.posEncodingFreqs, 10u);
}

TEST_F(NeRFFixture, SetAndGetNeRFSettings) {
    Renderer r(*ctx, *sc);
    NeRFSettings s;
    s.enabled          = true;
    s.marchSteps       = 128;
    s.networkLayers    = 4;
    s.posEncodingFreqs = 6;
    r.setNeRFSettings(s);
    EXPECT_TRUE(r.neRFSettings().enabled);
    EXPECT_EQ  (r.neRFSettings().marchSteps,       128u);
    EXPECT_EQ  (r.neRFSettings().networkLayers,    4u);
    EXPECT_EQ  (r.neRFSettings().posEncodingFreqs, 6u);
}

TEST_F(NeRFFixture, NeRFGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeRFSettings s;
    s.enabled    = true;
    s.marchSteps = 64;
    r.setNeRFSettings(s);
    RendererSettings rs;
    rs.enableNeRF = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neRFActive);
    EXPECT_EQ   (stats.neRFMarchSteps, 0u);
}

TEST_F(NeRFFixture, NeRFGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    NeRFSettings s;
    s.enabled    = true;
    s.marchSteps = 64;
    r.setNeRFSettings(s);
    RendererSettings rs;
    rs.enableNeRF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.neRFActive);
    EXPECT_EQ  (stats.neRFMarchSteps, 64u);
}

TEST_F(NeRFFixture, NeRFMarchStepsReflectedInStat) {
    Renderer r(*ctx, *sc);
    NeRFSettings s;
    s.enabled    = true;
    s.marchSteps = 256;
    r.setNeRFSettings(s);
    RendererSettings rs;
    rs.enableNeRF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.neRFMarchSteps, 256u);
}

TEST_F(NeRFFixture, NeRFInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeRFSettings s;
    s.enabled    = false;
    s.marchSteps = 64;
    r.setNeRFSettings(s);
    RendererSettings rs;
    rs.enableNeRF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neRFActive);
}
