// Null-backend tests for the Tone Mapping / HDR pass (v0.9 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct ToneMappingFixture : public ::testing::Test {
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

TEST_F(ToneMappingFixture, ToneMappingSettingsDefaultValues) {
    ToneMappingSettings tm;
    EXPECT_FLOAT_EQ(tm.exposure,   1.f);
    EXPECT_FLOAT_EQ(tm.whitePoint, 1.f);
    EXPECT_EQ      (tm.operator_,  ToneMappingOperator::ACES);
}

TEST_F(ToneMappingFixture, ToneMappingOperatorEnumValuesAreDistinct) {
    EXPECT_NE(static_cast<int>(ToneMappingOperator::Linear),
              static_cast<int>(ToneMappingOperator::Reinhard));
    EXPECT_NE(static_cast<int>(ToneMappingOperator::Reinhard),
              static_cast<int>(ToneMappingOperator::ACES));
}

TEST_F(ToneMappingFixture, ToneMappingOperatorLinearIsZero) {
    // API-freeze guard: Linear must remain 0 (default/passthrough value).
    EXPECT_EQ(static_cast<int>(ToneMappingOperator::Linear), 0);
}

TEST_F(ToneMappingFixture, SetAndGetToneMappingSettings) {
    Renderer r(*ctx, *sc);
    ToneMappingSettings tm;
    tm.exposure    = 2.f;
    tm.whitePoint  = 4.f;
    tm.operator_   = ToneMappingOperator::Reinhard;
    r.setToneMappingSettings(tm);
    EXPECT_FLOAT_EQ(r.toneMappingSettings().exposure,   2.f);
    EXPECT_FLOAT_EQ(r.toneMappingSettings().whitePoint, 4.f);
    EXPECT_EQ      (r.toneMappingSettings().operator_,  ToneMappingOperator::Reinhard);
}

TEST_F(ToneMappingFixture, EnableToneMappingFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableToneMapping);
}

TEST_F(ToneMappingFixture, TonemapActiveWhenEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableToneMapping = true;
    r.applySettings(s);
    EXPECT_TRUE(renderOnce(r).tonemapActive);
}

TEST_F(ToneMappingFixture, TonemapInactiveWhenDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableToneMapping = false;
    r.applySettings(s);
    EXPECT_FALSE(renderOnce(r).tonemapActive);
}

TEST_F(ToneMappingFixture, TonemapOperatorStoredInStats) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableToneMapping = true;
    r.applySettings(s);
    ToneMappingSettings tm;
    tm.operator_ = ToneMappingOperator::ACES;
    r.setToneMappingSettings(tm);
    EXPECT_EQ(renderOnce(r).tonemapOperator, ToneMappingOperator::ACES);
}
