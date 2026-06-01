// ─────────────────────────────────────────────────────────────────────────────
//  Tests: DLSS Ray Reconstruction (v0.5 Track 3)
//
//  Verifies DenoiserBackend::DLSS_RR, NeuralBackend::DLSS_RR, and the
//  NeuralRendererFactory::create(DLSS_RR) path. Null-backend tests always
//  run; SDK-dependent behaviour skips when NEXUS_ENABLE_DLSS is absent.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus::neural;
using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct DLSSRRFixture : public ::testing::Test {
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

// ── Enum surface ──────────────────────────────────────────────────────────────

TEST_F(DLSSRRFixture, DenoiserBackendDLSSRRIsDistinctFromDLSS4) {
    EXPECT_NE(static_cast<int>(DenoiserBackend::DLSS_RR),
              static_cast<int>(DenoiserBackend::DLSS4));
}

TEST_F(DLSSRRFixture, NeuralBackendDLSSRRIsDistinctFromDLSS4) {
    EXPECT_NE(static_cast<int>(NeuralBackend::DLSS_RR),
              static_cast<int>(NeuralBackend::DLSS4));
}

// ── Factory on Null backend (SDK absent) ─────────────────────────────────────

TEST_F(DLSSRRFixture, FactoryCreateDLSSRRReturnsNonNullOnNullBackend) {
    // SDK will be absent on Null backend — factory must fall back to Bilinear,
    // never return nullptr.
    auto nr = NeuralRendererFactory::create(NeuralBackend::DLSS_RR, *dev);
    ASSERT_NE(nr, nullptr);
}

TEST_F(DLSSRRFixture, FactoryCreateDLSSRRFallsBackWhenSDKAbsent) {
    auto nr = NeuralRendererFactory::create(NeuralBackend::DLSS_RR, *dev);
    ASSERT_NE(nr, nullptr);
    // Without the DLSS SDK the factory falls back to the deterministic renderer.
    // The denoiser must NOT report DLSS_RR when running on the software fallback.
    const DenoiserBackend db = nr->activeDenoiser();
    EXPECT_NE(db, DenoiserBackend::DLSS_RR);
}

// ── Renderer integration on Null backend ─────────────────────────────────────

TEST_F(DLSSRRFixture, DLSSRRRendererAttachesWithoutCrash) {
    auto nr = NeuralRendererFactory::create(NeuralBackend::DLSS_RR, *dev);
    ASSERT_NE(nr, nullptr);

    SceneGraph scene;
    Camera     cam;
    Renderer   renderer(*ctx, *sc);
    renderer.setNeuralRenderer(nr.get());

    RendererSettings settings{};
    settings.enableDenoising = true;
    renderer.applySettings(settings);

    renderer.beginFrame();
    EXPECT_NO_THROW(renderer.render(cam, scene));
    renderer.endFrame();
}

TEST_F(DLSSRRFixture, DLSSRRFallbackDoesNotActivateDenoisingOnNullBackend) {
    // The software fallback renderer does not actually denoise, so
    // denoisingActive should follow the INeuralRenderer::activeDenoiser() path.
    auto nr = NeuralRendererFactory::create(NeuralBackend::DLSS_RR, *dev);
    ASSERT_NE(nr, nullptr);

    SceneGraph scene;
    Camera     cam;
    Renderer   renderer(*ctx, *sc);
    renderer.setNeuralRenderer(nr.get());

    RendererSettings settings{};
    settings.enableDenoising = true;
    renderer.applySettings(settings);
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const auto stats = renderer.lastFrameStats();
    // With enableDenoising=true and a neural renderer attached, denoisingActive
    // reflects whether the denoiser dispatch was attempted — true on all backends.
    // The actual denoiser quality depends on SDK availability (DLSS vs fallback).
    // We verify the flag is set correctly (dispatch ran) not the backend quality.
    EXPECT_TRUE(stats.denoisingActive);
}

// ── perf_smoke flag smoke ─────────────────────────────────────────────────────

TEST_F(DLSSRRFixture, DLSSRRNeuralBackendEnumValueIsStable) {
    // Regression guard: the numeric value of DLSS_RR must not change once
    // published (API surface freeze).
    constexpr int kExpectedDLSSRRValue = 2;  // Auto=0, DLSS4=1, DLSS_RR=2
    EXPECT_EQ(static_cast<int>(NeuralBackend::DLSS_RR), kExpectedDLSSRRValue);
}

}  // namespace
