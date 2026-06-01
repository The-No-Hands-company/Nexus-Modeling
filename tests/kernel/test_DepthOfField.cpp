// Null-backend tests for the Depth of Field compute pass (v0.9 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct DoFFixture : public ::testing::Test {
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

TEST_F(DoFFixture, DoFSettingsDefaultValues) {
    DoFSettings dof;
    EXPECT_FLOAT_EQ(dof.focalDistance, 10.f);
    EXPECT_FLOAT_EQ(dof.focalRange,    5.f);
    EXPECT_FLOAT_EQ(dof.maxCoC,        0.05f);
    EXPECT_EQ      (dof.sampleRadius,  8u);
}

TEST_F(DoFFixture, SetAndGetDoFSettings) {
    Renderer r(*ctx, *sc);
    DoFSettings dof;
    dof.focalDistance = 20.f;
    dof.focalRange    = 8.f;
    dof.maxCoC        = 0.1f;
    dof.sampleRadius  = 16;
    r.setDoFSettings(dof);
    EXPECT_FLOAT_EQ(r.dofSettings().focalDistance, 20.f);
    EXPECT_FLOAT_EQ(r.dofSettings().focalRange,    8.f);
    EXPECT_FLOAT_EQ(r.dofSettings().maxCoC,        0.1f);
    EXPECT_EQ      (r.dofSettings().sampleRadius,  16u);
}

TEST_F(DoFFixture, EnableDoFFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableDoF);
}

TEST_F(DoFFixture, DoFActiveWhenEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDoF = true;
    r.applySettings(s);
    EXPECT_TRUE(renderOnce(r).dofActive);
}

TEST_F(DoFFixture, DoFInactiveWhenDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDoF = false;
    r.applySettings(s);
    EXPECT_FALSE(renderOnce(r).dofActive);
}

TEST_F(DoFFixture, DoFSampleCountPopulatedWhenActive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDoF = true;
    r.applySettings(s);
    DoFSettings dof;
    dof.sampleRadius = 4;
    r.setDoFSettings(dof);
    EXPECT_GT(renderOnce(r).dofSampleCount, 0u);
}

TEST_F(DoFFixture, DoFSampleCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableDoF = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).dofSampleCount, 0u);
}

TEST_F(DoFFixture, DoFSettingsRoundTripIndependent) {
    Renderer r(*ctx, *sc);
    DoFSettings a;
    a.focalDistance = 5.f;
    r.setDoFSettings(a);
    DoFSettings b;
    b.focalDistance = 50.f;
    r.setDoFSettings(b);
    EXPECT_FLOAT_EQ(r.dofSettings().focalDistance, 50.f);
}
