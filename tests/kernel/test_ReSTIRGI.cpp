// Null-backend tests for the ReSTIR GI spatiotemporal resampling pass (v0.15 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct ReSTIRFixture : public ::testing::Test {
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

TEST_F(ReSTIRFixture, ReSTIRSettingsDefaultValues) {
    ReSTIRSettings rs;
    EXPECT_FALSE   (rs.enabled);
    EXPECT_TRUE    (rs.spatialReuse);
    EXPECT_TRUE    (rs.temporalReuse);
    EXPECT_EQ      (rs.reservoirSize,  8u);
    EXPECT_FLOAT_EQ(rs.clampThreshold, 10.f);
}

TEST_F(ReSTIRFixture, SetAndGetReSTIRSettings) {
    Renderer r(*ctx, *sc);
    ReSTIRSettings rs;
    rs.enabled        = true;
    rs.spatialReuse   = false;
    rs.temporalReuse  = true;
    rs.reservoirSize  = 16;
    rs.clampThreshold = 5.f;
    r.setReSTIRSettings(rs);
    EXPECT_TRUE    (r.reSTIRSettings().enabled);
    EXPECT_FALSE   (r.reSTIRSettings().spatialReuse);
    EXPECT_TRUE    (r.reSTIRSettings().temporalReuse);
    EXPECT_EQ      (r.reSTIRSettings().reservoirSize,  16u);
    EXPECT_FLOAT_EQ(r.reSTIRSettings().clampThreshold, 5.f);
}

TEST_F(ReSTIRFixture, EnableReSTIRFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableReSTIR);
}

TEST_F(ReSTIRFixture, ReSTIRActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIR = true;
    r.applySettings(s);
    ReSTIRSettings rs;
    rs.enabled = true;
    r.setReSTIRSettings(rs);
    EXPECT_TRUE(renderOnce(r).restirActive);
}

TEST_F(ReSTIRFixture, ReSTIRInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIR = false;
    r.applySettings(s);
    ReSTIRSettings rs;
    rs.enabled = true;
    r.setReSTIRSettings(rs);
    EXPECT_FALSE(renderOnce(r).restirActive);
}

TEST_F(ReSTIRFixture, ReSTIRInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIR = true;
    r.applySettings(s);
    ReSTIRSettings rs;
    rs.enabled = false;
    r.setReSTIRSettings(rs);
    EXPECT_FALSE(renderOnce(r).restirActive);
}

TEST_F(ReSTIRFixture, ReservoirCountReflectsResolutionAndSize) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIR = true;
    r.applySettings(s);
    ReSTIRSettings rs;
    rs.enabled       = true;
    rs.reservoirSize = 4;
    r.setReSTIRSettings(rs);
    // 640 × 480 × 4 = 1228800
    EXPECT_EQ(renderOnce(r).restirReservoirCount, 640u * 480u * 4u);
}

TEST_F(ReSTIRFixture, ReservoirCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIR = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).restirReservoirCount, 0u);
}
