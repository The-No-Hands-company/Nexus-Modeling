// Null-backend tests for the Image-Based Lighting compute pass (v0.10 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct IBLFixture : public ::testing::Test {
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

TEST_F(IBLFixture, IBLSettingsDefaultValues) {
    IBLSettings ibl;
    EXPECT_FALSE   (ibl.enabled);
    EXPECT_FLOAT_EQ(ibl.intensity,     1.f);
    EXPECT_FLOAT_EQ(ibl.diffuseScale,  1.f);
    EXPECT_FLOAT_EQ(ibl.specularScale, 1.f);
    EXPECT_EQ      (ibl.mipLevels,     6u);
}

TEST_F(IBLFixture, SetAndGetIBLSettings) {
    Renderer r(*ctx, *sc);
    IBLSettings ibl;
    ibl.enabled       = true;
    ibl.intensity     = 0.5f;
    ibl.diffuseScale  = 0.8f;
    ibl.specularScale = 1.2f;
    ibl.mipLevels     = 8;
    r.setIBLSettings(ibl);
    EXPECT_TRUE    (r.iblSettings().enabled);
    EXPECT_FLOAT_EQ(r.iblSettings().intensity,     0.5f);
    EXPECT_FLOAT_EQ(r.iblSettings().diffuseScale,  0.8f);
    EXPECT_FLOAT_EQ(r.iblSettings().specularScale, 1.2f);
    EXPECT_EQ      (r.iblSettings().mipLevels,     8u);
}

TEST_F(IBLFixture, EnableIBLFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableIBL);
}

TEST_F(IBLFixture, IBLActiveWhenEnabledAndSettingsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableIBL = true;
    r.applySettings(s);
    IBLSettings ibl;
    ibl.enabled = true;
    r.setIBLSettings(ibl);
    EXPECT_TRUE(renderOnce(r).iblActive);
}

TEST_F(IBLFixture, IBLInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableIBL = false;
    r.applySettings(s);
    IBLSettings ibl;
    ibl.enabled = true;
    r.setIBLSettings(ibl);
    EXPECT_FALSE(renderOnce(r).iblActive);
}

TEST_F(IBLFixture, IBLInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableIBL = true;
    r.applySettings(s);
    IBLSettings ibl;
    ibl.enabled = false;
    r.setIBLSettings(ibl);
    EXPECT_FALSE(renderOnce(r).iblActive);
}

TEST_F(IBLFixture, IBLMipLevelsStoredInStats) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableIBL = true;
    r.applySettings(s);
    IBLSettings ibl;
    ibl.enabled   = true;
    ibl.mipLevels = 4;
    r.setIBLSettings(ibl);
    EXPECT_EQ(renderOnce(r).iblMipLevels, 4u);
}

TEST_F(IBLFixture, IBLSettingsRoundTripIndependent) {
    Renderer r(*ctx, *sc);
    IBLSettings a;
    a.intensity = 0.25f;
    r.setIBLSettings(a);
    IBLSettings b;
    b.intensity = 2.f;
    r.setIBLSettings(b);
    EXPECT_FLOAT_EQ(r.iblSettings().intensity, 2.f);
}
