// Null-backend tests for the OIT resolve pass (v0.10 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct OITFixture : public ::testing::Test {
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

TEST_F(OITFixture, OITSettingsDefaultValues) {
    OITSettings oit;
    EXPECT_FALSE   (oit.enabled);
    EXPECT_EQ      (oit.maxLayers,   8u);
    EXPECT_FLOAT_EQ(oit.weightScale, 1.f);
}

TEST_F(OITFixture, SetAndGetOITSettings) {
    Renderer r(*ctx, *sc);
    OITSettings oit;
    oit.enabled     = true;
    oit.maxLayers   = 16;
    oit.weightScale = 0.5f;
    r.setOITSettings(oit);
    EXPECT_TRUE    (r.oitSettings().enabled);
    EXPECT_EQ      (r.oitSettings().maxLayers,   16u);
    EXPECT_FLOAT_EQ(r.oitSettings().weightScale, 0.5f);
}

TEST_F(OITFixture, EnableOITFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableOIT);
}

TEST_F(OITFixture, OITActiveWhenEnabledAndSettingsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableOIT = true;
    r.applySettings(s);
    OITSettings oit;
    oit.enabled = true;
    r.setOITSettings(oit);
    EXPECT_TRUE(renderOnce(r).oitActive);
}

TEST_F(OITFixture, OITInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableOIT = false;
    r.applySettings(s);
    OITSettings oit;
    oit.enabled = true;
    r.setOITSettings(oit);
    EXPECT_FALSE(renderOnce(r).oitActive);
}

TEST_F(OITFixture, OITInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableOIT = true;
    r.applySettings(s);
    OITSettings oit;
    oit.enabled = false;
    r.setOITSettings(oit);
    EXPECT_FALSE(renderOnce(r).oitActive);
}

TEST_F(OITFixture, OITFragmentCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableOIT = true;
    r.applySettings(s);
    OITSettings oit;
    oit.enabled   = true;
    oit.maxLayers = 4;
    r.setOITSettings(oit);
    EXPECT_GT(renderOnce(r).oitFragmentCount, 0u);
}

TEST_F(OITFixture, OITFragmentCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableOIT = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).oitFragmentCount, 0u);
}
