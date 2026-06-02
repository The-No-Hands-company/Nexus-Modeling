// Null-backend tests for Instant-NGP Hash-Grid NeRF (v0.23 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct InstantNGPFixture : public ::testing::Test {
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

TEST_F(InstantNGPFixture, InstantNGPSettingsDefaultDisabled) {
    InstantNGPSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(InstantNGPFixture, InstantNGPSettingsDefaultHashGridLevels) {
    InstantNGPSettings s;
    EXPECT_EQ(s.hashGridLevels, 16u);
}

TEST_F(InstantNGPFixture, InstantNGPSettingsDefaultHashGridFeatures) {
    InstantNGPSettings s;
    EXPECT_EQ(s.hashGridFeatures, 2u);
}

TEST_F(InstantNGPFixture, InstantNGPSettingsDefaultHashTableSize) {
    InstantNGPSettings s;
    EXPECT_EQ(s.hashTableSize, 19u);
}

TEST_F(InstantNGPFixture, SetAndGetInstantNGPSettings) {
    Renderer r(*ctx, *sc);
    InstantNGPSettings s;
    s.enabled          = true;
    s.hashGridLevels   = 8;
    s.hashGridFeatures = 4;
    s.hashTableSize    = 22;
    r.setInstantNGPSettings(s);
    EXPECT_TRUE(r.instantNGPSettings().enabled);
    EXPECT_EQ  (r.instantNGPSettings().hashGridLevels,   8u);
    EXPECT_EQ  (r.instantNGPSettings().hashGridFeatures, 4u);
    EXPECT_EQ  (r.instantNGPSettings().hashTableSize,    22u);
}

TEST_F(InstantNGPFixture, InstantNGPGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    InstantNGPSettings s;
    s.enabled        = true;
    s.hashGridLevels = 16;
    r.setInstantNGPSettings(s);
    RendererSettings rs;
    rs.enableInstantNGP = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.instantNGPActive);
    EXPECT_EQ   (stats.instantNGPHashLevels, 0u);
}

TEST_F(InstantNGPFixture, InstantNGPGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    InstantNGPSettings s;
    s.enabled        = true;
    s.hashGridLevels = 16;
    r.setInstantNGPSettings(s);
    RendererSettings rs;
    rs.enableInstantNGP = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.instantNGPActive);
    EXPECT_EQ  (stats.instantNGPHashLevels, 16u);
}

TEST_F(InstantNGPFixture, InstantNGPHashLevelsReflectedInStat) {
    Renderer r(*ctx, *sc);
    InstantNGPSettings s;
    s.enabled        = true;
    s.hashGridLevels = 8;
    r.setInstantNGPSettings(s);
    RendererSettings rs;
    rs.enableInstantNGP = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.instantNGPHashLevels, 8u);
}

TEST_F(InstantNGPFixture, InstantNGPInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    InstantNGPSettings s;
    s.enabled        = false;
    s.hashGridLevels = 16;
    r.setInstantNGPSettings(s);
    RendererSettings rs;
    rs.enableInstantNGP = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.instantNGPActive);
}
