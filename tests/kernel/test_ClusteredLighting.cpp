// Null-backend tests for the GPU-driven clustered lighting pass (v0.12 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct ClusteredLightingFixture : public ::testing::Test {
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

TEST_F(ClusteredLightingFixture, ClusteredLightingSettingsDefaultValues) {
    ClusteredLightingSettings cl;
    EXPECT_FALSE(cl.enabled);
    EXPECT_EQ   (cl.clusterDimX,         16u);
    EXPECT_EQ   (cl.clusterDimY,          8u);
    EXPECT_EQ   (cl.clusterDimZ,         24u);
    EXPECT_EQ   (cl.maxLightsPerCluster, 128u);
}

TEST_F(ClusteredLightingFixture, SetAndGetClusteredLightingSettings) {
    Renderer r(*ctx, *sc);
    ClusteredLightingSettings cl;
    cl.enabled             = true;
    cl.clusterDimX         = 32;
    cl.clusterDimY         = 16;
    cl.clusterDimZ         = 32;
    cl.maxLightsPerCluster = 64;
    r.setClusteredLightingSettings(cl);
    EXPECT_TRUE(r.clusteredLightingSettings().enabled);
    EXPECT_EQ  (r.clusteredLightingSettings().clusterDimX,         32u);
    EXPECT_EQ  (r.clusteredLightingSettings().clusterDimY,         16u);
    EXPECT_EQ  (r.clusteredLightingSettings().clusterDimZ,         32u);
    EXPECT_EQ  (r.clusteredLightingSettings().maxLightsPerCluster, 64u);
}

TEST_F(ClusteredLightingFixture, EnableClusteredLightingFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableClusteredLighting);
}

TEST_F(ClusteredLightingFixture, ClusteredLightingActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableClusteredLighting = true;
    r.applySettings(s);
    ClusteredLightingSettings cl;
    cl.enabled = true;
    r.setClusteredLightingSettings(cl);
    EXPECT_TRUE(renderOnce(r).clusteredLightingActive);
}

TEST_F(ClusteredLightingFixture, ClusteredLightingInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableClusteredLighting = false;
    r.applySettings(s);
    ClusteredLightingSettings cl;
    cl.enabled = true;
    r.setClusteredLightingSettings(cl);
    EXPECT_FALSE(renderOnce(r).clusteredLightingActive);
}

TEST_F(ClusteredLightingFixture, ClusteredLightingInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableClusteredLighting = true;
    r.applySettings(s);
    ClusteredLightingSettings cl;
    cl.enabled = false;
    r.setClusteredLightingSettings(cl);
    EXPECT_FALSE(renderOnce(r).clusteredLightingActive);
}

TEST_F(ClusteredLightingFixture, LightClusterCountReflectsDimensions) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableClusteredLighting = true;
    r.applySettings(s);
    ClusteredLightingSettings cl;
    cl.enabled     = true;
    cl.clusterDimX = 8;
    cl.clusterDimY = 4;
    cl.clusterDimZ = 16;
    r.setClusteredLightingSettings(cl);
    EXPECT_EQ(renderOnce(r).lightClusterCount, 8u * 4u * 16u);
}

TEST_F(ClusteredLightingFixture, ClusteredMaxLightsPerClusterReflectsSettings) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableClusteredLighting = true;
    r.applySettings(s);
    ClusteredLightingSettings cl;
    cl.enabled             = true;
    cl.maxLightsPerCluster = 32;
    r.setClusteredLightingSettings(cl);
    EXPECT_EQ(renderOnce(r).clusteredMaxLightsPerCluster, 32u);
}

TEST_F(ClusteredLightingFixture, LightClusterCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableClusteredLighting = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).lightClusterCount, 0u);
}
