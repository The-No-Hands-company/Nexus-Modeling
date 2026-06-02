// Null-backend tests for Deformable Surface Tracking (v0.25 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct DeformableSurfaceFixture : public ::testing::Test {
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

TEST_F(DeformableSurfaceFixture, DeformableSurfaceSettingsDefaultDisabled) {
    DeformableSurfaceSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceSettingsDefaultTrackingLayers) {
    DeformableSurfaceSettings s;
    EXPECT_EQ(s.trackingLayers, 4u);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceSettingsDefaultMeshResolution) {
    DeformableSurfaceSettings s;
    EXPECT_EQ(s.meshResolution, 256u);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceSettingsDefaultSmoothing) {
    DeformableSurfaceSettings s;
    EXPECT_FLOAT_EQ(s.deformationSmoothing, 0.1f);
}

TEST_F(DeformableSurfaceFixture, SetAndGetDeformableSurfaceSettings) {
    Renderer r(*ctx, *sc);
    DeformableSurfaceSettings s;
    s.enabled              = true;
    s.trackingLayers       = 6;
    s.meshResolution       = 128;
    s.deformationSmoothing = 0.05f;
    r.setDeformableSurfaceSettings(s);
    EXPECT_TRUE     (r.deformableSurfaceSettings().enabled);
    EXPECT_EQ       (r.deformableSurfaceSettings().trackingLayers,       6u);
    EXPECT_EQ       (r.deformableSurfaceSettings().meshResolution,        128u);
    EXPECT_FLOAT_EQ (r.deformableSurfaceSettings().deformationSmoothing, 0.05f);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    DeformableSurfaceSettings s;
    s.enabled        = true;
    s.meshResolution = 256;
    r.setDeformableSurfaceSettings(s);
    RendererSettings rs;
    rs.enableDeformableSurface = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.deformableSurfaceActive);
    EXPECT_EQ   (stats.deformableSurfaceVertexCount, 0u);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    DeformableSurfaceSettings s;
    s.enabled        = true;
    s.meshResolution = 256;
    r.setDeformableSurfaceSettings(s);
    RendererSettings rs;
    rs.enableDeformableSurface = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.deformableSurfaceActive);
    EXPECT_EQ  (stats.deformableSurfaceVertexCount, 256u * 256u);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceVertexCountReflectsResolution) {
    Renderer r(*ctx, *sc);
    DeformableSurfaceSettings s;
    s.enabled        = true;
    s.meshResolution = 128;
    r.setDeformableSurfaceSettings(s);
    RendererSettings rs;
    rs.enableDeformableSurface = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.deformableSurfaceVertexCount, 128u * 128u);
}

TEST_F(DeformableSurfaceFixture, DeformableSurfaceInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    DeformableSurfaceSettings s;
    s.enabled        = false;
    s.meshResolution = 256;
    r.setDeformableSurfaceSettings(s);
    RendererSettings rs;
    rs.enableDeformableSurface = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.deformableSurfaceActive);
}
