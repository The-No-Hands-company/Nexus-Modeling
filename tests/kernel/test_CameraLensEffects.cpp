// Null-backend tests for the camera lens effects pass (v0.16 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct CameraLensFixture : public ::testing::Test {
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

TEST_F(CameraLensFixture, CameraLensSettingsDefaultValues) {
    CameraLensSettings cl;
    EXPECT_FALSE   (cl.chromaticAberrationEnabled);
    EXPECT_FLOAT_EQ(cl.aberrationStrength, 0.005f);
    EXPECT_FALSE   (cl.filmGrainEnabled);
    EXPECT_FLOAT_EQ(cl.grainIntensity, 0.04f);
    EXPECT_FLOAT_EQ(cl.grainSize,      1.5f);
}

TEST_F(CameraLensFixture, SetAndGetCameraLensSettings) {
    Renderer r(*ctx, *sc);
    CameraLensSettings cl;
    cl.chromaticAberrationEnabled = true;
    cl.aberrationStrength         = 0.01f;
    cl.filmGrainEnabled           = true;
    cl.grainIntensity             = 0.08f;
    cl.grainSize                  = 2.f;
    r.setCameraLensSettings(cl);
    EXPECT_TRUE    (r.cameraLensSettings().chromaticAberrationEnabled);
    EXPECT_FLOAT_EQ(r.cameraLensSettings().aberrationStrength, 0.01f);
    EXPECT_TRUE    (r.cameraLensSettings().filmGrainEnabled);
    EXPECT_FLOAT_EQ(r.cameraLensSettings().grainIntensity, 0.08f);
    EXPECT_FLOAT_EQ(r.cameraLensSettings().grainSize,      2.f);
}

TEST_F(CameraLensFixture, EnableCameraLensFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableCameraLens);
}

TEST_F(CameraLensFixture, CameraLensActiveWithAberrationOnly) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCameraLens = true;
    r.applySettings(s);
    CameraLensSettings cl;
    cl.chromaticAberrationEnabled = true;
    cl.filmGrainEnabled           = false;
    r.setCameraLensSettings(cl);
    const auto stats = renderOnce(r);
    EXPECT_TRUE(stats.cameraLensActive);
    EXPECT_EQ  (stats.cameraLensPassCount, 1u);
}

TEST_F(CameraLensFixture, CameraLensActiveWithGrainOnly) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCameraLens = true;
    r.applySettings(s);
    CameraLensSettings cl;
    cl.chromaticAberrationEnabled = false;
    cl.filmGrainEnabled           = true;
    r.setCameraLensSettings(cl);
    const auto stats = renderOnce(r);
    EXPECT_TRUE(stats.cameraLensActive);
    EXPECT_EQ  (stats.cameraLensPassCount, 1u);
}

TEST_F(CameraLensFixture, CameraLensTwoPassesWhenBothEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCameraLens = true;
    r.applySettings(s);
    CameraLensSettings cl;
    cl.chromaticAberrationEnabled = true;
    cl.filmGrainEnabled           = true;
    r.setCameraLensSettings(cl);
    EXPECT_EQ(renderOnce(r).cameraLensPassCount, 2u);
}

TEST_F(CameraLensFixture, CameraLensInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCameraLens = false;
    r.applySettings(s);
    CameraLensSettings cl;
    cl.chromaticAberrationEnabled = true;
    cl.filmGrainEnabled           = true;
    r.setCameraLensSettings(cl);
    EXPECT_FALSE(renderOnce(r).cameraLensActive);
}

TEST_F(CameraLensFixture, CameraLensInactiveWhenBothSubEffectsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCameraLens = true;
    r.applySettings(s);
    CameraLensSettings cl;
    cl.chromaticAberrationEnabled = false;
    cl.filmGrainEnabled           = false;
    r.setCameraLensSettings(cl);
    EXPECT_FALSE(renderOnce(r).cameraLensActive);
}

TEST_F(CameraLensFixture, CameraLensPassCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableCameraLens = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).cameraLensPassCount, 0u);
}
