// Null-backend tests for the ReSTIR PT path-tracing pass (v0.16 Track 1).
#include <gtest/gtest.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

struct ReSTIRPTFixture : public ::testing::Test {
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

TEST_F(ReSTIRPTFixture, ReSTIRPTSettingsDefaultValues) {
    ReSTIRPTSettings s;
    EXPECT_FALSE(s.enabled);
    EXPECT_EQ   (s.raysPerPixel,         1u);
    EXPECT_EQ   (s.maxPathLength,        6u);
    EXPECT_EQ   (s.russianRouletteDepth, 3u);
    EXPECT_TRUE (s.neeEnabled);
}

TEST_F(ReSTIRPTFixture, SetAndGetReSTIRPTSettings) {
    Renderer r(*ctx, *sc);
    ReSTIRPTSettings s;
    s.enabled               = true;
    s.raysPerPixel          = 2;
    s.maxPathLength         = 8;
    s.russianRouletteDepth  = 4;
    s.neeEnabled            = false;
    r.setReSTIRPTSettings(s);
    EXPECT_TRUE (r.reSTIRPTSettings().enabled);
    EXPECT_EQ   (r.reSTIRPTSettings().raysPerPixel,         2u);
    EXPECT_EQ   (r.reSTIRPTSettings().maxPathLength,        8u);
    EXPECT_EQ   (r.reSTIRPTSettings().russianRouletteDepth, 4u);
    EXPECT_FALSE(r.reSTIRPTSettings().neeEnabled);
}

TEST_F(ReSTIRPTFixture, EnableReSTIRPTFlagDefaultFalse) {
    RendererSettings s;
    EXPECT_FALSE(s.enableReSTIRPT);
}

TEST_F(ReSTIRPTFixture, ReSTIRPTActiveWhenBothFlagsEnabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIRPT = true;
    r.applySettings(s);
    ReSTIRPTSettings pt;
    pt.enabled = true;
    r.setReSTIRPTSettings(pt);
    EXPECT_TRUE(renderOnce(r).restirPTActive);
}

TEST_F(ReSTIRPTFixture, ReSTIRPTInactiveWhenFlagDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIRPT = false;
    r.applySettings(s);
    ReSTIRPTSettings pt;
    pt.enabled = true;
    r.setReSTIRPTSettings(pt);
    EXPECT_FALSE(renderOnce(r).restirPTActive);
}

TEST_F(ReSTIRPTFixture, ReSTIRPTInactiveWhenSettingsDisabled) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIRPT = true;
    r.applySettings(s);
    ReSTIRPTSettings pt;
    pt.enabled = false;
    r.setReSTIRPTSettings(pt);
    EXPECT_FALSE(renderOnce(r).restirPTActive);
}

TEST_F(ReSTIRPTFixture, PathCountReflectsResolutionAndRaysPerPixel) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIRPT = true;
    r.applySettings(s);
    ReSTIRPTSettings pt;
    pt.enabled       = true;
    pt.raysPerPixel  = 2;
    r.setReSTIRPTSettings(pt);
    EXPECT_EQ(renderOnce(r).restirPTPathCount, 640u * 480u * 2u);
}

TEST_F(ReSTIRPTFixture, PathCountZeroWhenInactive) {
    Renderer r(*ctx, *sc);
    RendererSettings s = r.settings();
    s.enableReSTIRPT = false;
    r.applySettings(s);
    EXPECT_EQ(renderOnce(r).restirPTPathCount, 0u);
}
