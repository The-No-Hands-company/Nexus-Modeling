// Null-backend tests for Fluorescence / Phosphorescence Simulation (v0.19 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct FluorescenceFixture : public ::testing::Test {
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

TEST_F(FluorescenceFixture, FluorescenceSettingsDefaultDisabled) {
    FluorescenceSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(FluorescenceFixture, FluorescenceSettingsDefaultFluorescenceScale) {
    FluorescenceSettings s;
    EXPECT_FLOAT_EQ(s.fluorescenceScale, 1.f);
}

TEST_F(FluorescenceFixture, FluorescenceSettingsDefaultPhosphorescenceDecay) {
    FluorescenceSettings s;
    EXPECT_FLOAT_EQ(s.phosphorescenceDecay, 0.1f);
}

TEST_F(FluorescenceFixture, FluorescenceSettingsDefaultEmissionBands) {
    FluorescenceSettings s;
    EXPECT_EQ(s.emissionBands, 4u);
}

TEST_F(FluorescenceFixture, SetAndGetFluorescenceSettings) {
    Renderer r(*ctx, *sc);
    FluorescenceSettings s;
    s.enabled               = true;
    s.fluorescenceScale     = 0.75f;
    s.phosphorescenceDecay  = 0.05f;
    s.emissionBands         = 8;
    r.setFluorescenceSettings(s);
    EXPECT_TRUE     (r.fluorescenceSettings().enabled);
    EXPECT_FLOAT_EQ (r.fluorescenceSettings().fluorescenceScale,    0.75f);
    EXPECT_FLOAT_EQ (r.fluorescenceSettings().phosphorescenceDecay, 0.05f);
    EXPECT_EQ       (r.fluorescenceSettings().emissionBands,        8u);
}

TEST_F(FluorescenceFixture, FluorescenceGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    FluorescenceSettings s;
    s.enabled       = true;
    s.emissionBands = 4;
    r.setFluorescenceSettings(s);
    RendererSettings rs;
    rs.enableFluorescence = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.fluorescenceActive);
    EXPECT_EQ   (stats.fluorescenceEmissionBands, 0u);
}

TEST_F(FluorescenceFixture, FluorescenceGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    FluorescenceSettings s;
    s.enabled       = true;
    s.emissionBands = 4;
    r.setFluorescenceSettings(s);
    RendererSettings rs;
    rs.enableFluorescence = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.fluorescenceActive);
    EXPECT_EQ  (stats.fluorescenceEmissionBands, 4u);
}

TEST_F(FluorescenceFixture, FluorescenceEmissionBandsReflectedInStat) {
    Renderer r(*ctx, *sc);
    FluorescenceSettings s;
    s.enabled       = true;
    s.emissionBands = 16;
    r.setFluorescenceSettings(s);
    RendererSettings rs;
    rs.enableFluorescence = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.fluorescenceEmissionBands, 16u);
}

TEST_F(FluorescenceFixture, FluorescenceInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    FluorescenceSettings s;
    s.enabled       = false;
    s.emissionBands = 4;
    r.setFluorescenceSettings(s);
    RendererSettings rs;
    rs.enableFluorescence = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.fluorescenceActive);
}
