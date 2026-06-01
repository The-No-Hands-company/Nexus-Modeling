// Null-backend tests for the screen-space contact shadows pass (v0.11 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct ContactShadowFixture : public ::testing::Test {
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

TEST_F(ContactShadowFixture, ContactShadowSettingsDefaultValues) {
    ContactShadowSettings cs;
    EXPECT_FALSE   (cs.enabled);
    EXPECT_FLOAT_EQ(cs.rayLength,   0.5f);
    EXPECT_EQ      (cs.sampleCount, 16u);
    EXPECT_FLOAT_EQ(cs.thickness,   0.05f);
}

TEST_F(ContactShadowFixture, SetAndGetContactShadowSettings) {
    Renderer r(*ctx, *sc);
    ContactShadowSettings cs;
    cs.enabled     = true;
    cs.rayLength   = 1.2f;
    cs.sampleCount = 32;
    cs.thickness   = 0.1f;
    r.setContactShadowSettings(cs);
    EXPECT_TRUE    (r.contactShadowSettings().enabled);
    EXPECT_FLOAT_EQ(r.contactShadowSettings().rayLength,   1.2f);
    EXPECT_EQ      (r.contactShadowSettings().sampleCount, 32u);
    EXPECT_FLOAT_EQ(r.contactShadowSettings().thickness,   0.1f);
}

TEST_F(ContactShadowFixture, EnableContactShadowsFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableContactShadows);
}

TEST_F(ContactShadowFixture, ContactShadowsActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableContactShadows = true;
    r.applySettings(s);
    ContactShadowSettings cs;
    cs.enabled = true;
    r.setContactShadowSettings(cs);
    EXPECT_TRUE(renderOnce(r).contactShadowsActive);
}

TEST_F(ContactShadowFixture, ContactShadowsInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableContactShadows = false;
    r.applySettings(s);
    ContactShadowSettings cs;
    cs.enabled = true;
    r.setContactShadowSettings(cs);
    EXPECT_FALSE(renderOnce(r).contactShadowsActive);
}

TEST_F(ContactShadowFixture, ContactShadowsInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableContactShadows = true;
    r.applySettings(s);
    ContactShadowSettings cs;
    cs.enabled = false;
    r.setContactShadowSettings(cs);
    EXPECT_FALSE(renderOnce(r).contactShadowsActive);
}

TEST_F(ContactShadowFixture, ContactShadowRayCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableContactShadows = true;
    r.applySettings(s);
    ContactShadowSettings cs;
    cs.enabled = true;
    r.setContactShadowSettings(cs);
    EXPECT_GT(renderOnce(r).contactShadowRayCount, 0u);
}

TEST_F(ContactShadowFixture, ContactShadowRayCountEqualsResolution) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableContactShadows = true;
    r.applySettings(s);
    ContactShadowSettings cs;
    cs.enabled = true;
    r.setContactShadowSettings(cs);
    // 640 × 480 = 307200
    EXPECT_EQ(renderOnce(r).contactShadowRayCount, 640u * 480u);
}

TEST_F(ContactShadowFixture, ContactShadowRayCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableContactShadows = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).contactShadowRayCount, 0u);
}
