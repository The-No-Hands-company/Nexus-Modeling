// Null-backend tests for the Motion Blur compute pass (v0.9 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MotionBlurFixture : public ::testing::Test {
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

TEST_F(MotionBlurFixture, MotionBlurSettingsDefaultValues) {
    MotionBlurSettings mb;
    EXPECT_FLOAT_EQ(mb.shutterAngle,  180.f);
    EXPECT_EQ      (mb.sampleCount,   8u);
    EXPECT_FLOAT_EQ(mb.maxBlurRadius, 32.f);
}

TEST_F(MotionBlurFixture, SetAndGetMotionBlurSettings) {
    Renderer r(*ctx, *sc);
    MotionBlurSettings mb;
    mb.shutterAngle  = 270.f;
    mb.sampleCount   = 16;
    mb.maxBlurRadius = 64.f;
    r.setMotionBlurSettings(mb);
    EXPECT_FLOAT_EQ(r.motionBlurSettings().shutterAngle,  270.f);
    EXPECT_EQ      (r.motionBlurSettings().sampleCount,   16u);
    EXPECT_FLOAT_EQ(r.motionBlurSettings().maxBlurRadius, 64.f);
}

TEST_F(MotionBlurFixture, EnableMotionBlurFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableMotionBlur);
}

TEST_F(MotionBlurFixture, MotionBlurActiveWhenEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableMotionBlur = true;
    r.applySettings(s);
    EXPECT_TRUE(renderOnce(r).motionBlurActive);
}

TEST_F(MotionBlurFixture, MotionBlurInactiveWhenDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableMotionBlur = false;
    r.applySettings(s);
    EXPECT_FALSE(renderOnce(r).motionBlurActive);
}

TEST_F(MotionBlurFixture, MotionBlurSampleCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableMotionBlur = true;
    r.applySettings(s);
    MotionBlurSettings mb;
    mb.sampleCount = 4;
    r.setMotionBlurSettings(mb);
    EXPECT_GT(renderOnce(r).motionBlurSampleCount, 0u);
}

TEST_F(MotionBlurFixture, MotionBlurSampleCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableMotionBlur = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).motionBlurSampleCount, 0u);
}

TEST_F(MotionBlurFixture, MotionBlurSettingsRoundTripIndependent) {
    Renderer r(*ctx, *sc);
    MotionBlurSettings a;
    a.shutterAngle = 90.f;
    r.setMotionBlurSettings(a);
    MotionBlurSettings b;
    b.shutterAngle = 360.f;
    r.setMotionBlurSettings(b);
    EXPECT_FLOAT_EQ(r.motionBlurSettings().shutterAngle, 360.f);
}
