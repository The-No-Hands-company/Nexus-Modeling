// Null-backend tests for Dynamic NeRF (D-NeRF) time-conditioned rendering (v0.23 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct DynamicNeRFFixture : public ::testing::Test {
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

TEST_F(DynamicNeRFFixture, DynamicNeRFSettingsDefaultDisabled) {
    DynamicNeRFSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFSettingsDefaultWarpNetworkLayers) {
    DynamicNeRFSettings s;
    EXPECT_EQ(s.warpNetworkLayers, 6u);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFSettingsDefaultTimeEmbeddingDim) {
    DynamicNeRFSettings s;
    EXPECT_EQ(s.timeEmbeddingDim, 256u);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFSettingsDefaultDeformationScale) {
    DynamicNeRFSettings s;
    EXPECT_FLOAT_EQ(s.deformationScale, 1.f);
}

TEST_F(DynamicNeRFFixture, SetAndGetDynamicNeRFSettings) {
    Renderer r(*ctx, *sc);
    DynamicNeRFSettings s;
    s.enabled            = true;
    s.warpNetworkLayers  = 4;
    s.timeEmbeddingDim   = 128;
    s.deformationScale   = 0.5f;
    r.setDynamicNeRFSettings(s);
    EXPECT_TRUE     (r.dynamicNeRFSettings().enabled);
    EXPECT_EQ       (r.dynamicNeRFSettings().warpNetworkLayers,  4u);
    EXPECT_EQ       (r.dynamicNeRFSettings().timeEmbeddingDim,   128u);
    EXPECT_FLOAT_EQ (r.dynamicNeRFSettings().deformationScale,   0.5f);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    DynamicNeRFSettings s;
    s.enabled           = true;
    s.warpNetworkLayers = 6;
    r.setDynamicNeRFSettings(s);
    RendererSettings rs;
    rs.enableDynamicNeRF = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.dynamicNeRFActive);
    EXPECT_EQ   (stats.dynamicNeRFWarpPasses, 0u);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    DynamicNeRFSettings s;
    s.enabled           = true;
    s.warpNetworkLayers = 6;
    r.setDynamicNeRFSettings(s);
    RendererSettings rs;
    rs.enableDynamicNeRF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.dynamicNeRFActive);
    // warpPasses = warpNetworkLayers + 1
    EXPECT_EQ  (stats.dynamicNeRFWarpPasses, 7u);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFWarpPassesReflectLayers) {
    Renderer r(*ctx, *sc);
    DynamicNeRFSettings s;
    s.enabled           = true;
    s.warpNetworkLayers = 3;
    r.setDynamicNeRFSettings(s);
    RendererSettings rs;
    rs.enableDynamicNeRF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.dynamicNeRFWarpPasses, 4u);
}

TEST_F(DynamicNeRFFixture, DynamicNeRFInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    DynamicNeRFSettings s;
    s.enabled           = false;
    s.warpNetworkLayers = 6;
    r.setDynamicNeRFSettings(s);
    RendererSettings rs;
    rs.enableDynamicNeRF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.dynamicNeRFActive);
}
