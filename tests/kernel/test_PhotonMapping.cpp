// Null-backend tests for Photon Mapping (v0.17 Track 2).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct PhotonFixture : public ::testing::Test {
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

TEST_F(PhotonFixture, PhotonMappingSettingsDefaultDisabled) {
    PhotonMappingSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(PhotonFixture, PhotonMappingSettingsDefaultPhotonCount) {
    PhotonMappingSettings s;
    EXPECT_EQ(s.photonCount, 100000u);
}

TEST_F(PhotonFixture, PhotonMappingSettingsDefaultMaxBounces) {
    PhotonMappingSettings s;
    EXPECT_EQ(s.maxBounces, 4u);
}

TEST_F(PhotonFixture, PhotonMappingSettingsDefaultGatherRadius) {
    PhotonMappingSettings s;
    EXPECT_FLOAT_EQ(s.gatherRadius, 0.05f);
}

TEST_F(PhotonFixture, SetAndGetPhotonMappingSettings) {
    Renderer r(*ctx, *sc);
    PhotonMappingSettings s;
    s.enabled      = true;
    s.photonCount  = 50000;
    s.maxBounces   = 2;
    s.gatherRadius = 0.1f;
    r.setPhotonMappingSettings(s);
    EXPECT_TRUE     (r.photonMappingSettings().enabled);
    EXPECT_EQ       (r.photonMappingSettings().photonCount,  50000u);
    EXPECT_EQ       (r.photonMappingSettings().maxBounces,   2u);
    EXPECT_FLOAT_EQ (r.photonMappingSettings().gatherRadius, 0.1f);
}

TEST_F(PhotonFixture, PhotonMappingGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PhotonMappingSettings s;
    s.enabled     = true;
    s.photonCount = 100000;
    r.setPhotonMappingSettings(s);
    RendererSettings rs;
    rs.enablePhotonMapping = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.photonMappingActive);
    EXPECT_EQ   (stats.photonsEmitted, 0u);
}

TEST_F(PhotonFixture, PhotonMappingGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    PhotonMappingSettings s;
    s.enabled     = true;
    s.photonCount = 100000;
    r.setPhotonMappingSettings(s);
    RendererSettings rs;
    rs.enablePhotonMapping = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.photonMappingActive);
    EXPECT_EQ  (stats.photonsEmitted, 100000u);
}

TEST_F(PhotonFixture, PhotonCountReflectedInStat) {
    Renderer r(*ctx, *sc);
    PhotonMappingSettings s;
    s.enabled     = true;
    s.photonCount = 200000;
    r.setPhotonMappingSettings(s);
    RendererSettings rs;
    rs.enablePhotonMapping = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.photonsEmitted, 200000u);
}

TEST_F(PhotonFixture, PhotonMappingInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    PhotonMappingSettings s;
    s.enabled     = false;
    s.photonCount = 100000;
    r.setPhotonMappingSettings(s);
    RendererSettings rs;
    rs.enablePhotonMapping = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.photonMappingActive);
}
