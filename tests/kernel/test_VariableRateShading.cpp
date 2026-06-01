// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Variable-Rate Shading (v0.7 Track 1)
//
//  Verifies RendererSettings::enableVRS, ShadingRate enum, defaultShadingRate,
//  and FrameStats::vrsActive. All tests run on Null backend in headless CI
//  where caps().variableRateShading == false, so vrsActive is always false.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct VRSFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain> sc;

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

// ── Enum surface ──────────────────────────────────────────────────────────────

TEST_F(VRSFixture, ShadingRateEnumValuesAreDistinct) {
    EXPECT_NE(static_cast<int>(ShadingRate::Rate1x1), static_cast<int>(ShadingRate::Rate2x2));
    EXPECT_NE(static_cast<int>(ShadingRate::Rate2x2), static_cast<int>(ShadingRate::Rate4x4));
    EXPECT_NE(static_cast<int>(ShadingRate::Rate1x2), static_cast<int>(ShadingRate::Rate2x1));
}

TEST_F(VRSFixture, ShadingRate1x1IsZero) {
    // API-freeze guard: Rate1x1 must remain 0 (the reset / default value).
    EXPECT_EQ(static_cast<int>(ShadingRate::Rate1x1), 0);
}

// ── Settings API ──────────────────────────────────────────────────────────────

TEST_F(VRSFixture, DefaultVRSSettingIsDisabled) {
    Renderer renderer(*ctx, *sc);
    EXPECT_FALSE(renderer.settings().enableVRS);
}

TEST_F(VRSFixture, DefaultShadingRateIs2x2) {
    Renderer renderer(*ctx, *sc);
    EXPECT_EQ(renderer.settings().defaultShadingRate, ShadingRate::Rate2x2);
}

TEST_F(VRSFixture, VRSSettingsRoundTrip) {
    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.enableVRS          = true;
    s.defaultShadingRate = ShadingRate::Rate4x4;
    renderer.applySettings(s);
    EXPECT_TRUE(renderer.settings().enableVRS);
    EXPECT_EQ(renderer.settings().defaultShadingRate, ShadingRate::Rate4x4);
}

// ── Null backend behaviour ────────────────────────────────────────────────────

TEST_F(VRSFixture, VRSActiveDefaultsToFalse) {
    Renderer renderer(*ctx, *sc);
    FrameStats stats = renderOnce(renderer);
    EXPECT_FALSE(stats.vrsActive);
}

TEST_F(VRSFixture, VRSFlagEnabledButCapsAbsentKeepsVRSInactive) {
    // On Null backend variableRateShading cap is always false, so vrsActive
    // must remain false even when the flag is set.
    EXPECT_FALSE(ctx->device().caps().variableRateShading);

    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.enableVRS = true;
    renderer.applySettings(s);

    FrameStats stats = renderOnce(renderer);
    EXPECT_FALSE(stats.vrsActive);
}

TEST_F(VRSFixture, VRSActiveResetEachFrame) {
    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.enableVRS = true;
    renderer.applySettings(s);

    FrameStats f1 = renderOnce(renderer);
    FrameStats f2 = renderOnce(renderer);
    // Both frames must independently reflect the cap-gated state (false on Null).
    EXPECT_EQ(f1.vrsActive, f2.vrsActive);
}

TEST_F(VRSFixture, DisablingVRSMidSessionHasImmediateEffect) {
    Renderer renderer(*ctx, *sc);
    RendererSettings s = renderer.settings();
    s.enableVRS = true;
    renderer.applySettings(s);
    renderOnce(renderer);  // frame 1 (vrsActive==false on Null)

    s.enableVRS = false;
    renderer.applySettings(s);
    FrameStats f2 = renderOnce(renderer);
    EXPECT_FALSE(f2.vrsActive);
}

}  // namespace
