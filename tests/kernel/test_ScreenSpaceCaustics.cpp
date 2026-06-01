// Null-backend tests for the screen-space caustics pass (v0.16 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct CausticsFixture : public ::testing::Test {
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

TEST_F(CausticsFixture, CausticsSettingsDefaultValues) {
    CausticsSettings cs;
    EXPECT_FALSE   (cs.enabled);
    EXPECT_EQ      (cs.sampleCount,  32u);
    EXPECT_FLOAT_EQ(cs.intensity,    0.5f);
    EXPECT_FLOAT_EQ(cs.searchRadius, 0.1f);
}

TEST_F(CausticsFixture, SetAndGetCausticsSettings) {
    Renderer r(*ctx, *sc);
    CausticsSettings cs;
    cs.enabled      = true;
    cs.sampleCount  = 64;
    cs.intensity    = 0.8f;
    cs.searchRadius = 0.2f;
    r.setCausticsSettings(cs);
    EXPECT_TRUE    (r.causticsSettings().enabled);
    EXPECT_EQ      (r.causticsSettings().sampleCount,  64u);
    EXPECT_FLOAT_EQ(r.causticsSettings().intensity,    0.8f);
    EXPECT_FLOAT_EQ(r.causticsSettings().searchRadius, 0.2f);
}

TEST_F(CausticsFixture, EnableCausticsFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableCaustics);
}

TEST_F(CausticsFixture, CausticsActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCaustics = true;
    r.applySettings(s);
    CausticsSettings cs;
    cs.enabled = true;
    r.setCausticsSettings(cs);
    EXPECT_TRUE(renderOnce(r).causticsActive);
}

TEST_F(CausticsFixture, CausticsInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCaustics = false;
    r.applySettings(s);
    CausticsSettings cs;
    cs.enabled = true;
    r.setCausticsSettings(cs);
    EXPECT_FALSE(renderOnce(r).causticsActive);
}

TEST_F(CausticsFixture, CausticsInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCaustics = true;
    r.applySettings(s);
    CausticsSettings cs;
    cs.enabled = false;
    r.setCausticsSettings(cs);
    EXPECT_FALSE(renderOnce(r).causticsActive);
}

TEST_F(CausticsFixture, CausticsSampleCountReflectsResolutionAndSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCaustics = true;
    r.applySettings(s);
    CausticsSettings cs;
    cs.enabled     = true;
    cs.sampleCount = 16;
    r.setCausticsSettings(cs);
    EXPECT_EQ(renderOnce(r).causticsSampleCount, 640u * 480u * 16u);
}

TEST_F(CausticsFixture, CausticsSampleCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCaustics = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).causticsSampleCount, 0u);
}
