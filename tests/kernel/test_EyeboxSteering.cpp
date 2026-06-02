// Null-backend tests for Eyebox / Exit-Pupil Steering (v0.24 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct EyeboxSteeringFixture : public ::testing::Test {
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

TEST_F(EyeboxSteeringFixture, EyeboxSteeringSettingsDefaultDisabled) {
    EyeboxSteeringSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringSettingsDefaultGazeUpdateHz) {
    EyeboxSteeringSettings s;
    EXPECT_FLOAT_EQ(s.gazeUpdateHz, 120.f);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringSettingsDefaultPupilDiameter) {
    EyeboxSteeringSettings s;
    EXPECT_FLOAT_EQ(s.pupilDiameterMm, 4.f);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringSettingsDefaultSteeringSlices) {
    EyeboxSteeringSettings s;
    EXPECT_EQ(s.steeringSlices, 8u);
}

TEST_F(EyeboxSteeringFixture, SetAndGetEyeboxSteeringSettings) {
    Renderer r(*ctx, *sc);
    EyeboxSteeringSettings s;
    s.enabled         = true;
    s.gazeUpdateHz    = 60.f;
    s.pupilDiameterMm = 3.f;
    s.steeringSlices  = 16;
    r.setEyeboxSteeringSettings(s);
    EXPECT_TRUE     (r.eyeboxSteeringSettings().enabled);
    EXPECT_FLOAT_EQ (r.eyeboxSteeringSettings().gazeUpdateHz,    60.f);
    EXPECT_FLOAT_EQ (r.eyeboxSteeringSettings().pupilDiameterMm, 3.f);
    EXPECT_EQ       (r.eyeboxSteeringSettings().steeringSlices,  16u);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    EyeboxSteeringSettings s;
    s.enabled        = true;
    s.steeringSlices = 8;
    r.setEyeboxSteeringSettings(s);
    RendererSettings rs;
    rs.enableEyeboxSteering = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.eyeboxSteeringActive);
    EXPECT_EQ   (stats.eyeboxSteeringSliceCount, 0u);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    EyeboxSteeringSettings s;
    s.enabled        = true;
    s.steeringSlices = 8;
    r.setEyeboxSteeringSettings(s);
    RendererSettings rs;
    rs.enableEyeboxSteering = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.eyeboxSteeringActive);
    EXPECT_EQ  (stats.eyeboxSteeringSliceCount, 8u);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringSliceCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    EyeboxSteeringSettings s;
    s.enabled        = true;
    s.steeringSlices = 16;
    r.setEyeboxSteeringSettings(s);
    RendererSettings rs;
    rs.enableEyeboxSteering = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.eyeboxSteeringSliceCount, 16u);
}

TEST_F(EyeboxSteeringFixture, EyeboxSteeringInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    EyeboxSteeringSettings s;
    s.enabled        = false;
    s.steeringSlices = 8;
    r.setEyeboxSteeringSettings(s);
    RendererSettings rs;
    rs.enableEyeboxSteering = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.eyeboxSteeringActive);
}
