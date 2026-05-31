// ─────────────────────────────────────────────────────────────────────────────
//  Tests for neural denoiser / Renderer async-compute scheduling (Month 20)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/neural/NeuralRenderer.h>

#include <gtest/gtest.h>

using namespace nexus::render;
using namespace nexus::gfx;
using namespace nexus::neural;

// ── Stub INeuralRenderer ─────────────────────────────────────────────────────

class StubNeuralRenderer final : public INeuralRenderer {
public:
    DenoiserBackend activeDenoiser() const noexcept override { return m_denoiserBackend; }
    UpscalerBackend activeUpscaler() const noexcept override { return UpscalerBackend::None; }

    void denoise(nexus::gfx::CmdBufHandle, const DenoiserInput&, DenoiserOutput&) override
    {
        ++m_denoiseCalls;
    }
    void upscale(nexus::gfx::CmdBufHandle, const UpscalerInput&, UpscalerOutput&) override {}

    uint32_t        denoiseCalls()  const noexcept { return m_denoiseCalls; }
    void            setDenoiserBackend(DenoiserBackend b) noexcept { m_denoiserBackend = b; }

private:
    uint32_t        m_denoiseCalls    = 0;
    DenoiserBackend m_denoiserBackend = DenoiserBackend::OIDN_CPU;
};

// ── Fixtures ─────────────────────────────────────────────────────────────────

namespace {

RenderContextDesc nullCtxDesc()
{
    RenderContextDesc d{};
    d.preferredBackend = Backend::Null;
    d.validation       = ValidationLevel::Off;
    return d;
}

struct NeuralDenoiserFixture : ::testing::Test {
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

// ── Unit tests — no backend needed ────────────────────────────────────────────

TEST(NeuralDenoiser, DefaultDenoisingIsInactive)
{
    auto ctx = RenderContext::create(nullCtxDesc());
    ASSERT_TRUE(ctx);
    SwapchainDesc sd{}; sd.extent = {800u, 600u};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_TRUE(sc);
    Renderer renderer(*ctx, *sc);

    EXPECT_EQ(renderer.neuralRenderer(), nullptr);
    EXPECT_FALSE(renderer.settings().enableDenoising);
}

TEST(NeuralDenoiser, AttachDetachRoundTrip)
{
    auto ctx = RenderContext::create(nullCtxDesc());
    ASSERT_TRUE(ctx);
    SwapchainDesc sd{}; sd.extent = {800u, 600u};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_TRUE(sc);
    Renderer renderer(*ctx, *sc);

    StubNeuralRenderer stub;
    renderer.setNeuralRenderer(&stub);
    EXPECT_EQ(renderer.neuralRenderer(), &stub);

    renderer.setNeuralRenderer(nullptr);
    EXPECT_EQ(renderer.neuralRenderer(), nullptr);
}

// ── Integration tests using NeuralDenoiserFixture ─────────────────────────────

TEST_F(NeuralDenoiserFixture, DenoisingInactiveByDefaultAfterRender)
{
    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().denoisingActive);
    EXPECT_EQ(renderer->lastFrameStats().activeDenoiser, DenoiserBackend::None);
}

TEST_F(NeuralDenoiserFixture, DenoisingInactiveWhenEnabledButNoRendererAttached)
{
    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().denoisingActive);
    EXPECT_EQ(renderer->lastFrameStats().activeDenoiser, DenoiserBackend::None);
}

TEST_F(NeuralDenoiserFixture, DenoisingInactiveWhenRendererAttachedButDisabled)
{
    StubNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);
    // enableDenoising stays false (default)

    renderOneFrame();
    EXPECT_FALSE(renderer->lastFrameStats().denoisingActive);
    EXPECT_EQ(stub.denoiseCalls(), 0u);
}

TEST_F(NeuralDenoiserFixture, DenoisingActiveWhenEnabledAndRendererAttached)
{
    StubNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_TRUE(renderer->lastFrameStats().denoisingActive);
    EXPECT_EQ(stub.denoiseCalls(), 1u);
}

TEST_F(NeuralDenoiserFixture, ActiveDenoiserMatchesStubBackend)
{
    StubNeuralRenderer stub;
    stub.setDenoiserBackend(DenoiserBackend::OIDN_CPU);
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);

    renderOneFrame();
    EXPECT_EQ(renderer->lastFrameStats().activeDenoiser, DenoiserBackend::OIDN_CPU);
}

TEST_F(NeuralDenoiserFixture, DenoiseCalledOncePerFrame)
{
    StubNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);

    for (int i = 0; i < 4; ++i) renderOneFrame();
    EXPECT_EQ(stub.denoiseCalls(), 4u);
}

TEST_F(NeuralDenoiserFixture, DisablingDenoisingStopsDenoiseCall)
{
    StubNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);
    renderOneFrame();
    EXPECT_EQ(stub.denoiseCalls(), 1u);

    s.enableDenoising = false;
    renderer->applySettings(s);
    renderOneFrame();
    // Still 1 — no new call after disabling.
    EXPECT_EQ(stub.denoiseCalls(), 1u);
    EXPECT_FALSE(renderer->lastFrameStats().denoisingActive);
}

TEST_F(NeuralDenoiserFixture, DetachingRendererStopsDenoiseCall)
{
    StubNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);
    renderOneFrame();
    EXPECT_EQ(stub.denoiseCalls(), 1u);

    renderer->setNeuralRenderer(nullptr);
    renderOneFrame();
    EXPECT_EQ(stub.denoiseCalls(), 1u);
    EXPECT_FALSE(renderer->lastFrameStats().denoisingActive);
}

TEST_F(NeuralDenoiserFixture, DenoisingActiveDoesNotAffectDrawCallCount)
{
    StubNeuralRenderer stub;
    renderer->setNeuralRenderer(&stub);

    RendererSettings s = renderer->settings();
    s.enableDenoising = true;
    renderer->applySettings(s);

    renderOneFrame();
    // Denoiser must not spuriously increment draw call counters.
    EXPECT_EQ(renderer->lastFrameStats().drawCalls, 0u);
}
