// ─────────────────────────────────────────────────────────────────────────────
//  Tests for neural upscaler scheduling and DLSS/XeSS perf gate (Month 21)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/neural/NeuralRenderer.h>

#include <gtest/gtest.h>

#include <chrono>
#include <numeric>

using namespace nexus::render;
using namespace nexus::gfx;
using namespace nexus::neural;

// ── Stub INeuralRenderer (records calls for both denoiser and upscaler) ───────

class StubFullNeuralRenderer final : public INeuralRenderer {
public:
    DenoiserBackend activeDenoiser() const noexcept override { return m_denoiserBackend; }
    UpscalerBackend activeUpscaler() const noexcept override { return m_upscalerBackend; }

    void denoise(nexus::gfx::CmdBufHandle, const DenoiserInput&, DenoiserOutput&) override
    { ++m_denoiseCalls; }
    void upscale(nexus::gfx::CmdBufHandle, const UpscalerInput&, UpscalerOutput&) override
    { ++m_upscaleCalls; }

    uint32_t denoiseCalls()  const noexcept { return m_denoiseCalls; }
    uint32_t upscaleCalls()  const noexcept { return m_upscaleCalls; }
    void setDenoiserBackend(DenoiserBackend b) noexcept { m_denoiserBackend = b; }
    void setUpscalerBackend(UpscalerBackend b) noexcept { m_upscalerBackend = b; }

private:
    uint32_t        m_denoiseCalls    = 0;
    uint32_t        m_upscaleCalls    = 0;
    DenoiserBackend m_denoiserBackend = DenoiserBackend::OIDN_CPU;
    UpscalerBackend m_upscalerBackend = UpscalerBackend::Bilinear;
};

// ── Fixture ───────────────────────────────────────────────────────────────────

namespace {

RenderContextDesc nullCtxDesc()
{
    RenderContextDesc d{};
    d.preferredBackend = Backend::Null;
    d.validation       = ValidationLevel::Off;
    return d;
}

struct UpscalerFixture : ::testing::Test {
    void SetUp() override
    {
        ctx = RenderContext::create(nullCtxDesc());
        ASSERT_TRUE(ctx);
        SwapchainDesc sd{};
        sd.extent = {800u, 600u};
        sc = ctx->createSwapchain(sd);
        ASSERT_TRUE(sc);
        renderer = std::make_unique<Renderer>(*ctx, *sc);
    }

    void renderOneFrame()
    {
        renderer->beginFrame();
        renderer->render(cam, scene);
        renderer->endFrame();
    }

    std::shared_ptr<RenderContext> ctx;
    std::shared_ptr<ISwapchain>    sc;
    std::unique_ptr<Renderer>      renderer;
    Camera                         cam;
    SceneGraph                     scene;
};

} // namespace

// ── Upscaler unit tests ───────────────────────────────────────────────────────

TEST(NeuralUpscaler, DefaultUpscalingIsInactive)
{
    auto ctx = RenderContext::create(nullCtxDesc());
    ASSERT_TRUE(ctx);
    SwapchainDesc sd{}; sd.extent = {800u, 600u};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_TRUE(sc);
    Renderer renderer(*ctx, *sc);

    EXPECT_FALSE(renderer.settings().enableUpscaling);
}

TEST(NeuralUpscaler, UpscalingInactiveByDefaultAfterRender)
{
    auto ctx = RenderContext::create(nullCtxDesc());
    ASSERT_TRUE(ctx);
    SwapchainDesc sd{}; sd.extent = {800u, 600u};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_TRUE(sc);
    Renderer renderer(*ctx, *sc);

    Camera cam; SceneGraph scene;
    renderer.beginFrame(); renderer.render(cam, scene); renderer.endFrame();

    EXPECT_FALSE(renderer.lastFrameStats().upscalingActive);
    EXPECT_EQ(renderer.lastFrameStats().activeUpscaler, UpscalerBackend::None);
}

TEST_F(UpscalerFixture, UpscalingInactiveWhenEnabledButNoRendererAttached)
{
    RendererSettings s = renderer->settings();
    s.enableUpscaling = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().upscalingActive);
    EXPECT_EQ(renderer->lastFrameStats().activeUpscaler, UpscalerBackend::None);
}

TEST_F(UpscalerFixture, UpscalingInactiveWhenRendererAttachedButDisabled)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);
    // enableUpscaling stays false (default)

    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().upscalingActive);
    EXPECT_EQ(stub.upscaleCalls(), 0u);
}

TEST_F(UpscalerFixture, UpscalingActiveWhenEnabledAndRendererAttached)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableUpscaling = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_TRUE(renderer->lastFrameStats().upscalingActive);
    EXPECT_EQ(stub.upscaleCalls(), 1u);
}

TEST_F(UpscalerFixture, ActiveUpscalerMatchesStubBackend)
{
    StubFullNeuralRenderer stub;
    stub.setUpscalerBackend(UpscalerBackend::Bilinear);
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableUpscaling = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().activeUpscaler, UpscalerBackend::Bilinear);
}

TEST_F(UpscalerFixture, UpscaleCalledOncePerFrame)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableUpscaling = true;
    renderer->applySettings(s);

    for (int i = 0; i < 4; ++i) renderOneFrame();
    EXPECT_EQ(stub.upscaleCalls(), 4u);
}

TEST_F(UpscalerFixture, DisablingUpscalingStopsUpscaleCall)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableUpscaling = true;
    renderer->applySettings(s);
    renderOneFrame();
    EXPECT_EQ(stub.upscaleCalls(), 1u);

    s.enableUpscaling = false;
    renderer->applySettings(s);
    renderOneFrame();
    EXPECT_EQ(stub.upscaleCalls(), 1u);
    EXPECT_FALSE(renderer->lastFrameStats().upscalingActive);
}

TEST_F(UpscalerFixture, BothDenoiseAndUpscaleRunInSameFrame)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    s.enableUpscaling = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_TRUE(renderer->lastFrameStats().denoisingActive);
    EXPECT_TRUE(renderer->lastFrameStats().upscalingActive);
    EXPECT_EQ(stub.denoiseCalls(), 1u);
    EXPECT_EQ(stub.upscaleCalls(), 1u);
}

TEST_F(UpscalerFixture, UpscalingDoesNotAffectDrawCallCount)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableUpscaling = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().drawCalls, 0u);
}

// ── DLSS/XeSS perf gate ───────────────────────────────────────────────────────
// Verifies that the denoiser+upscaler dispatch overhead on the Null backend
// does not regress beyond an absolute wall-clock ceiling per frame.
// The ceiling is generous (50 ms) to stay stable on loaded CI runners.

TEST_F(UpscalerFixture, NeuralDispatchOverheadBelowCeilingOnNullBackend)
{
    StubFullNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    s.enableUpscaling = true;
    renderer->applySettings(s);

    constexpr int kFrames = 64;
    std::vector<double> frameTimes;
    frameTimes.reserve(kFrames);

    for (int i = 0; i < kFrames; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        renderOneFrame();
        const auto t1 = std::chrono::steady_clock::now();
        frameTimes.push_back(
            std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    const double average =
        std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / kFrames;

    // Null backend with stub neural renderer must average under 50 ms/frame.
    EXPECT_LT(average, 50.0)
        << "Neural dispatch overhead regression: average frame time "
        << average << " ms exceeds 50 ms ceiling";

    // Sanity: both passes were actually called.
    EXPECT_EQ(stub.denoiseCalls(), static_cast<uint32_t>(kFrames));
    EXPECT_EQ(stub.upscaleCalls(), static_cast<uint32_t>(kFrames));
}

TEST_F(UpscalerFixture, NeuralDispatchDeterministicAcrossRuns)
{
    // Run the same frame sequence twice and verify call counts are identical,
    // ensuring the dispatch path has no non-deterministic side effects.
    auto runFrames = [&](int n) -> std::pair<uint32_t, uint32_t> {
        StubFullNeuralRenderer stub;
        renderer->setNeuralRenderer(&stub);

        RendererSettings s = renderer->settings();
        s.enableDenoising = true;
        s.enableUpscaling = true;
        renderer->applySettings(s);

        for (int i = 0; i < n; ++i) renderOneFrame();

        renderer->setNeuralRenderer(nullptr);
        return {stub.denoiseCalls(), stub.upscaleCalls()};
    };

    const auto [d1, u1] = runFrames(8);
    const auto [d2, u2] = runFrames(8);

    EXPECT_EQ(d1, d2);
    EXPECT_EQ(u1, u2);
}
