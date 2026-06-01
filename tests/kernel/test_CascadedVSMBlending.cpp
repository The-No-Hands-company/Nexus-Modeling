// Null-backend tests for cascaded VSM blend weights extension (v0.15 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct CascadedVSMFixture : public ::testing::Test {
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

TEST_F(CascadedVSMFixture, VSMSettingsHaveCascadeBlendDefaults) {
    VSMSettings vsm;
    EXPECT_FLOAT_EQ(vsm.cascadeBlendRange, 0.1f);
    EXPECT_FLOAT_EQ(vsm.perCascadeWeights[0], 1.f);
    EXPECT_FLOAT_EQ(vsm.perCascadeWeights[1], 1.f);
    EXPECT_FLOAT_EQ(vsm.perCascadeWeights[2], 1.f);
    EXPECT_FLOAT_EQ(vsm.perCascadeWeights[3], 1.f);
}

TEST_F(CascadedVSMFixture, SetAndGetCascadeBlendFields) {
    Renderer r(*ctx, *sc);
    VSMSettings vsm;
    vsm.enabled              = true;
    vsm.cascadeBlendRange    = 0.2f;
    vsm.perCascadeWeights[0] = 1.0f;
    vsm.perCascadeWeights[1] = 0.8f;
    vsm.perCascadeWeights[2] = 0.6f;
    vsm.perCascadeWeights[3] = 0.4f;
    r.setVSMSettings(vsm);
    EXPECT_FLOAT_EQ(r.vsmSettings().cascadeBlendRange,    0.2f);
    EXPECT_FLOAT_EQ(r.vsmSettings().perCascadeWeights[1], 0.8f);
    EXPECT_FLOAT_EQ(r.vsmSettings().perCascadeWeights[3], 0.4f);
}

TEST_F(CascadedVSMFixture, VSMBlendedCascadeCountZeroWhenVSMInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).vsmBlendedCascadeCount, 0u);
}

TEST_F(CascadedVSMFixture, VSMBlendedCascadeCountZeroWithNoCascades) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled           = true;
    vsm.cascadeBlendRange = 0.1f;
    r.setVSMSettings(vsm);
    // No shadow contract set → cascadeCount = 0 → clamped to 1 → no boundaries
    EXPECT_EQ(renderOnce(r).vsmBlendedCascadeCount, 0u);
}

TEST_F(CascadedVSMFixture, VSMBlendedCascadeCountZeroWhenBlendRangeIsZero) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled           = true;
    vsm.cascadeBlendRange = 0.f; // disabled
    r.setVSMSettings(vsm);
    EXPECT_EQ(renderOnce(r).vsmBlendedCascadeCount, 0u);
}

TEST_F(CascadedVSMFixture, VSMActiveStillTrueWithBlendExtension) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled           = true;
    vsm.cascadeBlendRange = 0.15f;
    r.setVSMSettings(vsm);
    EXPECT_TRUE(renderOnce(r).vsmActive);
}

TEST_F(CascadedVSMFixture, PerCascadeWeightsRoundTripAllFour) {
    Renderer r(*ctx, *sc);
    VSMSettings vsm;
    vsm.perCascadeWeights[0] = 0.9f;
    vsm.perCascadeWeights[1] = 0.7f;
    vsm.perCascadeWeights[2] = 0.5f;
    vsm.perCascadeWeights[3] = 0.3f;
    r.setVSMSettings(vsm);
    EXPECT_FLOAT_EQ(r.vsmSettings().perCascadeWeights[0], 0.9f);
    EXPECT_FLOAT_EQ(r.vsmSettings().perCascadeWeights[1], 0.7f);
    EXPECT_FLOAT_EQ(r.vsmSettings().perCascadeWeights[2], 0.5f);
    EXPECT_FLOAT_EQ(r.vsmSettings().perCascadeWeights[3], 0.3f);
}

TEST_F(CascadedVSMFixture, VSMCascadeCountUnaffectedByBlendExtension) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableVSM = true;
    r.applySettings(s);
    VSMSettings vsm;
    vsm.enabled           = true;
    vsm.cascadeBlendRange = 0.2f;
    r.setVSMSettings(vsm);
    // cascadeCount still reflects shadow contract (1 when no contract set)
    EXPECT_GE(renderOnce(r).vsmCascadeCount, 1u);
}
