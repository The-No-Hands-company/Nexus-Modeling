// Null-backend tests for Varifocal Holographic Display Stacking (v0.25 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct VarifocalHolographicFixture : public ::testing::Test {
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

TEST_F(VarifocalHolographicFixture, VarifocalHolographicSettingsDefaultDisabled) {
    VarifocalHolographicSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicSettingsDefaultFocalLayers) {
    VarifocalHolographicSettings s;
    EXPECT_EQ(s.focalLayers, 8u);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicSettingsDefaultFocalRangeNear) {
    VarifocalHolographicSettings s;
    EXPECT_FLOAT_EQ(s.focalRangeNear, 0.3f);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicSettingsDefaultFocalRangeFar) {
    VarifocalHolographicSettings s;
    EXPECT_FLOAT_EQ(s.focalRangeFar, 3.0f);
}

TEST_F(VarifocalHolographicFixture, SetAndGetVarifocalHolographicSettings) {
    Renderer r(*ctx, *sc);
    VarifocalHolographicSettings s;
    s.enabled        = true;
    s.focalLayers    = 12;
    s.focalRangeNear = 0.5f;
    s.focalRangeFar  = 5.0f;
    r.setVarifocalHolographicSettings(s);
    EXPECT_TRUE     (r.varifocalHolographicSettings().enabled);
    EXPECT_EQ       (r.varifocalHolographicSettings().focalLayers,    12u);
    EXPECT_FLOAT_EQ (r.varifocalHolographicSettings().focalRangeNear, 0.5f);
    EXPECT_FLOAT_EQ (r.varifocalHolographicSettings().focalRangeFar,  5.0f);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    VarifocalHolographicSettings s;
    s.enabled     = true;
    s.focalLayers = 8;
    r.setVarifocalHolographicSettings(s);
    RendererSettings rs;
    rs.enableVarifocalHolographic = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.varifocalHolographicActive);
    EXPECT_EQ   (stats.varifocalHolographicLayerCount, 0u);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    VarifocalHolographicSettings s;
    s.enabled     = true;
    s.focalLayers = 8;
    r.setVarifocalHolographicSettings(s);
    RendererSettings rs;
    rs.enableVarifocalHolographic = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.varifocalHolographicActive);
    EXPECT_EQ  (stats.varifocalHolographicLayerCount, 8u);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicLayerCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    VarifocalHolographicSettings s;
    s.enabled     = true;
    s.focalLayers = 16;
    r.setVarifocalHolographicSettings(s);
    RendererSettings rs;
    rs.enableVarifocalHolographic = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.varifocalHolographicLayerCount, 16u);
}

TEST_F(VarifocalHolographicFixture, VarifocalHolographicInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    VarifocalHolographicSettings s;
    s.enabled     = false;
    s.focalLayers = 8;
    r.setVarifocalHolographicSettings(s);
    RendererSettings rs;
    rs.enableVarifocalHolographic = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.varifocalHolographicActive);
}
