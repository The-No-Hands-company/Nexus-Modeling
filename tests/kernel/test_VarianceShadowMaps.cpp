// Null-backend tests for the variance shadow map pass (v0.14 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct VSMFixture : public ::testing::Test {
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

TEST_F(VSMFixture, VSMSettingsDefaultValues) {
    VSMSettings vsm;
    EXPECT_FALSE   (vsm.enabled);
    EXPECT_FLOAT_EQ(vsm.blurRadius,          2.f);
    EXPECT_FLOAT_EQ(vsm.lightBleedReduction, 0.2f);
    EXPECT_FLOAT_EQ(vsm.minVariance,         1e-5f);
}

TEST_F(VSMFixture, SetAndGetVSMSettings) {
    Renderer r(*ctx, *sc);
    VSMSettings vsm;
    vsm.enabled              = true;
    vsm.blurRadius           = 3.f;
    vsm.lightBleedReduction  = 0.3f;
    vsm.minVariance          = 1e-4f;
    r.setVSMSettings(vsm);
    EXPECT_TRUE    (r.vsmSettings().enabled);
    EXPECT_FLOAT_EQ(r.vsmSettings().blurRadius,          3.f);
    EXPECT_FLOAT_EQ(r.vsmSettings().lightBleedReduction, 0.3f);
    EXPECT_FLOAT_EQ(r.vsmSettings().minVariance,         1e-4f);
}

TEST_F(VSMFixture, EnableVSMFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableVSM);
}

TEST_F(VSMFixture, VSMActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled = true;
    r.setVSMSettings(vsm);
    EXPECT_TRUE(renderOnce(r).vsmActive);
}

TEST_F(VSMFixture, VSMInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = false;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled = true;
    r.setVSMSettings(vsm);
    EXPECT_FALSE(renderOnce(r).vsmActive);
}

TEST_F(VSMFixture, VSMInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled = false;
    r.setVSMSettings(vsm);
    EXPECT_FALSE(renderOnce(r).vsmActive);
}

TEST_F(VSMFixture, VSMCascadeCountAtLeastOneWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled = true;
    r.setVSMSettings(vsm);
    EXPECT_GE(renderOnce(r).vsmCascadeCount, 1u);
}

TEST_F(VSMFixture, VSMCascadeCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).vsmCascadeCount, 0u);
}
