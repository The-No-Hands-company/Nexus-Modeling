// Null-backend tests for Mueller Matrix BSDF (v0.20 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct MuellerBSDFFixture : public ::testing::Test {
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

TEST_F(MuellerBSDFFixture, MuellerBSDFSettingsDefaultDisabled) {
    MuellerBSDFSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(MuellerBSDFFixture, MuellerBSDFSettingsDefaultMatrixOrder) {
    MuellerBSDFSettings s;
    EXPECT_EQ(s.matrixOrder, 4u);
}

TEST_F(MuellerBSDFFixture, MuellerBSDFSettingsDefaultTrackDepolarisation) {
    MuellerBSDFSettings s;
    EXPECT_TRUE(s.trackDepolarisation);
}

TEST_F(MuellerBSDFFixture, SetAndGetMuellerBSDFSettings) {
    Renderer r(*ctx, *sc);
    MuellerBSDFSettings s;
    s.enabled              = true;
    s.matrixOrder          = 4;
    s.trackDepolarisation  = false;
    r.setMuellerBSDFSettings(s);
    EXPECT_TRUE (r.muellerBSDFSettings().enabled);
    EXPECT_EQ   (r.muellerBSDFSettings().matrixOrder, 4u);
    EXPECT_FALSE(r.muellerBSDFSettings().trackDepolarisation);
}

TEST_F(MuellerBSDFFixture, MuellerBSDFGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    MuellerBSDFSettings s;
    s.enabled = true;
    r.setMuellerBSDFSettings(s);
    RendererSettings rs;
    rs.enableMuellerBSDF = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.muellerBSDFActive);
    EXPECT_EQ   (stats.muellerBSDFEvalCount, 0u);
}

TEST_F(MuellerBSDFFixture, MuellerBSDFGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    MuellerBSDFSettings s;
    s.enabled     = true;
    s.matrixOrder = 4;
    r.setMuellerBSDFSettings(s);
    RendererSettings rs;
    rs.enableMuellerBSDF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.muellerBSDFActive);
    EXPECT_GT  (stats.muellerBSDFEvalCount, 0u);
}

TEST_F(MuellerBSDFFixture, MuellerBSDFEvalCountReflectsOrder) {
    Renderer r(*ctx, *sc);
    MuellerBSDFSettings s;
    s.enabled     = true;
    s.matrixOrder = 4;
    r.setMuellerBSDFSettings(s);
    RendererSettings rs;
    rs.enableMuellerBSDF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    const uint32_t expected = 640u * 480u * 4u;
    EXPECT_EQ(stats.muellerBSDFEvalCount, expected);
}

TEST_F(MuellerBSDFFixture, MuellerBSDFInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    MuellerBSDFSettings s;
    s.enabled = false;
    r.setMuellerBSDFSettings(s);
    RendererSettings rs;
    rs.enableMuellerBSDF = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.muellerBSDFActive);
}
