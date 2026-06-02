// Null-backend tests for Neural Scene Flow / 4D Occupancy Grids (v0.24 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct NeuralSceneFlowFixture : public ::testing::Test {
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

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowSettingsDefaultDisabled) {
    NeuralSceneFlowSettings s;
    EXPECT_FALSE(s.enabled);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowSettingsDefaultFlowNetworkLayers) {
    NeuralSceneFlowSettings s;
    EXPECT_EQ(s.flowNetworkLayers, 6u);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowSettingsDefaultOccupancyResolution) {
    NeuralSceneFlowSettings s;
    EXPECT_EQ(s.occupancyResolution, 128u);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowSettingsDefaultTemporalWindow) {
    NeuralSceneFlowSettings s;
    EXPECT_EQ(s.temporalWindow, 4u);
}

TEST_F(NeuralSceneFlowFixture, SetAndGetNeuralSceneFlowSettings) {
    Renderer r(*ctx, *sc);
    NeuralSceneFlowSettings s;
    s.enabled             = true;
    s.flowNetworkLayers   = 4;
    s.occupancyResolution = 64;
    s.temporalWindow      = 8;
    r.setNeuralSceneFlowSettings(s);
    EXPECT_TRUE(r.neuralSceneFlowSettings().enabled);
    EXPECT_EQ  (r.neuralSceneFlowSettings().flowNetworkLayers,   4u);
    EXPECT_EQ  (r.neuralSceneFlowSettings().occupancyResolution, 64u);
    EXPECT_EQ  (r.neuralSceneFlowSettings().temporalWindow,      8u);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowGateFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeuralSceneFlowSettings s;
    s.enabled             = true;
    s.occupancyResolution = 128;
    r.setNeuralSceneFlowSettings(s);
    RendererSettings rs;
    rs.enableNeuralSceneFlow = false;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neuralSceneFlowActive);
    EXPECT_EQ   (stats.neuralSceneFlowVoxelCount, 0u);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowGateTrue_StatActive) {
    Renderer r(*ctx, *sc);
    NeuralSceneFlowSettings s;
    s.enabled             = true;
    s.occupancyResolution = 128;
    r.setNeuralSceneFlowSettings(s);
    RendererSettings rs;
    rs.enableNeuralSceneFlow = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_TRUE(stats.neuralSceneFlowActive);
    EXPECT_EQ  (stats.neuralSceneFlowVoxelCount, 128u * 128u * 128u);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowVoxelCountReflectsResolution) {
    Renderer r(*ctx, *sc);
    NeuralSceneFlowSettings s;
    s.enabled             = true;
    s.occupancyResolution = 64;
    r.setNeuralSceneFlowSettings(s);
    RendererSettings rs;
    rs.enableNeuralSceneFlow = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_EQ(stats.neuralSceneFlowVoxelCount, 64u * 64u * 64u);
}

TEST_F(NeuralSceneFlowFixture, NeuralSceneFlowInnerFlagFalse_StatInactive) {
    Renderer r(*ctx, *sc);
    NeuralSceneFlowSettings s;
    s.enabled             = false;
    s.occupancyResolution = 128;
    r.setNeuralSceneFlowSettings(s);
    RendererSettings rs;
    rs.enableNeuralSceneFlow = true;
    r.applySettings(rs);
    const FrameStats stats = renderOnce(r);
    EXPECT_FALSE(stats.neuralSceneFlowActive);
}
