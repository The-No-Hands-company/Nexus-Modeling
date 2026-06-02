// Null-backend tests for Time-Resolved Phosphorescence Decay (v0.20 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct PhosphorescenceDecayFixture : public ::testing::Test {
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

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecaySettingsDefaultDisabled) {
    PhosphorescenceDecaySettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecaySettingsDefaultDecayFrames) {
    PhosphorescenceDecaySettings s;
    EXPECT_EQ(s.decayFrames, 30u);
}

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecaySettingsDefaultDecayExponent) {
    PhosphorescenceDecaySettings s;
    EXPECT_FLOAT_EQ(s.decayExponent, 2.f);
}

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecaySettingsDefaultAccumulate) {
    PhosphorescenceDecaySettings s;
    EXPECT_TRUE(s.accumulate);
}

TEST_F(PhosphorescenceDecayFixture, SetAndGetPhosphorescenceDecaySettings) {
    Renderer r(*ctx, *sc);
    PhosphorescenceDecaySettings s;
    s.enabled       = true;
    s.decayFrames   = 60;
    s.decayExponent = 3.f;
    s.accumulate    = false;
    r.setPhosphorescenceDecaySettings(s);
    EXPECT_TRUE     (r.phosphorescenceDecaySettings().enabled);
    EXPECT_EQ       (r.phosphorescenceDecaySettings().decayFrames,   60u);
    EXPECT_FLOAT_EQ (r.phosphorescenceDecaySettings().decayExponent, 3.f);
    EXPECT_FALSE    (r.phosphorescenceDecaySettings().accumulate);
}

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecayGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PhosphorescenceDecaySettings s;
    s.enabled     = true;
    s.decayFrames = 30;
    r.setPhosphorescenceDecaySettings(s);
    RendererSettings rs;
    rs.enablePhosphorescenceDecay = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.phosphorescenceDecayActive);
    EXPECT_EQ   (stats.phosphorescenceDecayFrames, 0u);
}

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecayGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    PhosphorescenceDecaySettings s;
    s.enabled     = true;
    s.decayFrames = 30;
    r.setPhosphorescenceDecaySettings(s);
    RendererSettings rs;
    rs.enablePhosphorescenceDecay = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.phosphorescenceDecayActive);
    EXPECT_EQ  (stats.phosphorescenceDecayFrames, 30u);
}

TEST_F(PhosphorescenceDecayFixture, DecayFramesReflectedInStat) {
    Renderer r(*ctx, *sc);
    PhosphorescenceDecaySettings s;
    s.enabled     = true;
    s.decayFrames = 120;
    r.setPhosphorescenceDecaySettings(s);
    RendererSettings rs;
    rs.enablePhosphorescenceDecay = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.phosphorescenceDecayFrames, 120u);
}

TEST_F(PhosphorescenceDecayFixture, PhosphorescenceDecayInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PhosphorescenceDecaySettings s;
    s.enabled     = false;
    s.decayFrames = 30;
    r.setPhosphorescenceDecaySettings(s);
    RendererSettings rs;
    rs.enablePhosphorescenceDecay = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.phosphorescenceDecayActive);
}
