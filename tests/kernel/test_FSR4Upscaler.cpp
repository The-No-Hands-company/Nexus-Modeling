// Null-backend tests for the FSR 4 upscaler integration (v0.10 Track 3).
#include <gtest/gtest.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::neural;
using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct FSR4Fixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain>    sc;
    IDevice* dev = nullptr;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
        dev = &ctx->device();
        SwapchainDesc sd{};
        sd.extent = { 640, 480 };
        sc = ctx->createSwapchain(sd);
        ASSERT_NE(sc, nullptr);
    }
};

} // namespace

TEST_F(FSR4Fixture, FSR4NeuralBackendEnumValueIsStable) {
    // API-freeze guard: FSR4 must remain 7 (Auto=0…Bilinear=6, FSR4=7).
    constexpr int kExpected = 7;
    EXPECT_EQ(static_cast<int>(NeuralBackend::FSR4), kExpected);
}

TEST_F(FSR4Fixture, FSR4UpscalerBackendEnumValueIsDistinctFromFSR3) {
    EXPECT_NE(static_cast<int>(UpscalerBackend::FSR4),
              static_cast<int>(UpscalerBackend::FSR3));
}

TEST_F(FSR4Fixture, FactoryCreateFSR4ReturnsNonNullOnNullBackend) {
    auto nr = NeuralRendererFactory::create(NeuralBackend::FSR4, *dev);
    ASSERT_NE(nr, nullptr);
}

TEST_F(FSR4Fixture, FactoryCreateFSR4FallsBackWhenSDKAbsent) {
    auto nr = NeuralRendererFactory::create(NeuralBackend::FSR4, *dev);
    ASSERT_NE(nr, nullptr);
    // Without the FidelityFX 4 SDK the factory falls back gracefully.
    const UpscalerBackend ub = nr->activeUpscaler();
    EXPECT_NE(ub, UpscalerBackend::FSR4);
}

TEST_F(FSR4Fixture, FSR4AttachesToRendererWithoutCrash) {
    auto nr = NeuralRendererFactory::create(NeuralBackend::FSR4, *dev);
    ASSERT_NE(nr, nullptr);

    Renderer r(*ctx, *sc);
    r.setNeuralRenderer(nr.get());

    RendererSettings s = r.settings();
    s.enableUpscaling = true;
    r.applySettings(s);

    SceneGraph scene;
    Camera cam;
    r.beginFrame();
    EXPECT_NO_THROW(r.render(cam, scene));
    r.endFrame();
}

TEST_F(FSR4Fixture, FSR4UpscalingActiveWhenAttached) {
    auto nr = NeuralRendererFactory::create(NeuralBackend::FSR4, *dev);
    ASSERT_NE(nr, nullptr);

    Renderer r(*ctx, *sc);
    r.setNeuralRenderer(nr.get());

    RendererSettings s = r.settings();
    s.enableUpscaling = true;
    r.applySettings(s);

    SceneGraph scene;
    Camera cam;
    r.beginFrame();
    r.render(cam, scene);
    r.endFrame();

    EXPECT_TRUE(r.lastFrameStats().upscalingActive);
}

TEST_F(FSR4Fixture, FSR4UpscalerBackendEnumValueIsStable) {
    // API-freeze guard: FSR4 UpscalerBackend must remain 5.
    constexpr int kExpected = 5;
    EXPECT_EQ(static_cast<int>(UpscalerBackend::FSR4), kExpected);
}
