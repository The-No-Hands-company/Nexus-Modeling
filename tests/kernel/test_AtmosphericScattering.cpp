// Null-backend tests for the atmospheric scattering LUT pass (v0.13 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct AtmosphericScatteringFixture : public ::testing::Test {
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

TEST_F(AtmosphericScatteringFixture, AtmosphericSettingsDefaultValues) {
    AtmosphericScatteringSettings as;
    EXPECT_FALSE   (as.enabled);
    EXPECT_FLOAT_EQ(as.rayleighScale,  1.f);
    EXPECT_FLOAT_EQ(as.mieScale,       1.f);
    EXPECT_FLOAT_EQ(as.sunZenithAngle, 45.f);
    EXPECT_EQ      (as.lutSize,        256u);
}

TEST_F(AtmosphericScatteringFixture, SetAndGetAtmosphericSettings) {
    Renderer r(*ctx, *sc);
    AtmosphericScatteringSettings as;
    as.enabled        = true;
    as.rayleighScale  = 0.8f;
    as.mieScale       = 1.2f;
    as.sunZenithAngle = 30.f;
    as.lutSize        = 128;
    r.setAtmosphericScatteringSettings(as);
    EXPECT_TRUE    (r.atmosphericScatteringSettings().enabled);
    EXPECT_FLOAT_EQ(r.atmosphericScatteringSettings().rayleighScale,  0.8f);
    EXPECT_FLOAT_EQ(r.atmosphericScatteringSettings().mieScale,       1.2f);
    EXPECT_FLOAT_EQ(r.atmosphericScatteringSettings().sunZenithAngle, 30.f);
    EXPECT_EQ      (r.atmosphericScatteringSettings().lutSize,        128u);
}

TEST_F(AtmosphericScatteringFixture, EnableAtmosphericFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableAtmosphericScattering);
}

TEST_F(AtmosphericScatteringFixture, AtmosphericActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAtmosphericScattering = true;
    r.applySettings(s);
    AtmosphericScatteringSettings as;
    as.enabled = true;
    r.setAtmosphericScatteringSettings(as);
    EXPECT_TRUE(renderOnce(r).atmosphericScatteringActive);
}

TEST_F(AtmosphericScatteringFixture, AtmosphericInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAtmosphericScattering = false;
    r.applySettings(s);
    AtmosphericScatteringSettings as;
    as.enabled = true;
    r.setAtmosphericScatteringSettings(as);
    EXPECT_FALSE(renderOnce(r).atmosphericScatteringActive);
}

TEST_F(AtmosphericScatteringFixture, AtmosphericInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAtmosphericScattering = true;
    r.applySettings(s);
    AtmosphericScatteringSettings as;
    as.enabled = false;
    r.setAtmosphericScatteringSettings(as);
    EXPECT_FALSE(renderOnce(r).atmosphericScatteringActive);
}

TEST_F(AtmosphericScatteringFixture, AtmosphericLUTSizeReflectsSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAtmosphericScattering = true;
    r.applySettings(s);
    AtmosphericScatteringSettings as;
    as.enabled = true;
    as.lutSize = 512;
    r.setAtmosphericScatteringSettings(as);
    EXPECT_EQ(renderOnce(r).atmosphericLUTSize, 512u);
}

TEST_F(AtmosphericScatteringFixture, AtmosphericLUTSizeZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableAtmosphericScattering = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).atmosphericLUTSize, 0u);
}
