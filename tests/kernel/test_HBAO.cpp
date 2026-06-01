// Null-backend tests for the HBAO+ pass (v0.14 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct HBAOFixture : public ::testing::Test {
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

TEST_F(HBAOFixture, HBAOSettingsDefaultValues) {
    HBAOSettings h;
    EXPECT_FALSE   (h.enabled);
    EXPECT_FLOAT_EQ(h.radius,     0.5f);
    EXPECT_FLOAT_EQ(h.angleBias,  0.1f);
    EXPECT_EQ      (h.sliceCount, 4u);
    EXPECT_EQ      (h.stepCount,  4u);
}

TEST_F(HBAOFixture, SetAndGetHBAOSettings) {
    Renderer r(*ctx, *sc);
    HBAOSettings h;
    h.enabled    = true;
    h.radius     = 1.f;
    h.angleBias  = 0.05f;
    h.sliceCount = 8;
    h.stepCount  = 6;
    r.setHBAOSettings(h);
    EXPECT_TRUE    (r.hbaoSettings().enabled);
    EXPECT_FLOAT_EQ(r.hbaoSettings().radius,     1.f);
    EXPECT_FLOAT_EQ(r.hbaoSettings().angleBias,  0.05f);
    EXPECT_EQ      (r.hbaoSettings().sliceCount, 8u);
    EXPECT_EQ      (r.hbaoSettings().stepCount,  6u);
}

TEST_F(HBAOFixture, EnableHBAOFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableHBAO);
}

TEST_F(HBAOFixture, HBAOActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableHBAO = true;
    r.applySettings(s);
    HBAOSettings h;
    h.enabled = true;
    r.setHBAOSettings(h);
    EXPECT_TRUE(renderOnce(r).hbaoActive);
}

TEST_F(HBAOFixture, HBAOInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableHBAO = false;
    r.applySettings(s);
    HBAOSettings h;
    h.enabled = true;
    r.setHBAOSettings(h);
    EXPECT_FALSE(renderOnce(r).hbaoActive);
}

TEST_F(HBAOFixture, HBAOInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableHBAO = true;
    r.applySettings(s);
    HBAOSettings h;
    h.enabled = false;
    r.setHBAOSettings(h);
    EXPECT_FALSE(renderOnce(r).hbaoActive);
}

TEST_F(HBAOFixture, HBAOSampleCountReflectsSlicesAndSteps) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableHBAO = true;
    r.applySettings(s);
    HBAOSettings h;
    h.enabled    = true;
    h.sliceCount = 4;
    h.stepCount  = 8;
    r.setHBAOSettings(h);
    // 640 × 480 × 4 × 8 = 9830400
    EXPECT_EQ(renderOnce(r).hbaoSampleCount, 640u * 480u * 4u * 8u);
}

TEST_F(HBAOFixture, HBAOSampleCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableHBAO = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).hbaoSampleCount, 0u);
}
