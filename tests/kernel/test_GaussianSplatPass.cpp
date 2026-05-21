// ─────────────────────────────────────────────────────────────────────────────
//  test_GaussianSplatPass — behavior tests for the Month-13 splat pass
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>

#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/render/Camera.h>
#include <nexus/render/GaussianSplatPass.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

#include <cmath>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

// Build a Vulkan-style reversed-NDC perspective projection (z range [0, w]).
Mat4 makePerspective(float fovY, float aspect, float zn, float zf)
{
    const float f = 1.f / std::tan(fovY * 0.5f);
    Mat4 m{};
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = zf / (zn - zf);
    m.m[2][3] = (zn * zf) / (zn - zf);
    m.m[3][2] = -1.f;
    return m;
}

GaussianSplatCloud makeCloudOnZ(const std::vector<float>& zValues,
                                float opacity = 0.f)
{
    GaussianSplatCloud cloud{};
    cloud.splats.reserve(zValues.size());
    for (float z : zValues) {
        GaussianSplat s{};
        s.position = { 0.f, 0.f, z };
        s.opacity  = opacity;
        cloud.splats.push_back(s);
    }
    return cloud;
}

GaussianSplatPassConfig makeDefaultConfig()
{
    GaussianSplatPassConfig cfg{};
    cfg.view       = Mat4::identity();
    cfg.projection = makePerspective(1.0472f /* 60deg */, 16.f / 9.f, 0.5f, 100.f);
    return cfg;
}

} // namespace

TEST(GaussianSplatPass, EmptyHasZeroStats)
{
    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());

    const auto stats = pass.computeStats();
    EXPECT_EQ(stats.splatDrawCalls,  0u);
    EXPECT_EQ(stats.submittedSplats, 0u);
    EXPECT_EQ(stats.projectedSplats, 0u);
    EXPECT_EQ(stats.discardedSplats, 0u);
    EXPECT_EQ(pass.attachedCloudCount(), 0u);
}

TEST(GaussianSplatPass, NullCloudIsIgnored)
{
    GaussianSplatPass pass;
    pass.addCloud(nullptr);
    EXPECT_EQ(pass.attachedCloudCount(), 0u);
}

TEST(GaussianSplatPass, SubmittedEqualsCloudSize)
{
    GaussianSplatSceneNode node{ makeCloudOnZ({ -2.f, -5.f, -20.f, -50.f }) };

    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&node);

    const auto stats = pass.computeStats();
    EXPECT_EQ(stats.submittedSplats, 4u);
    EXPECT_EQ(stats.submittedSplats,
              stats.projectedSplats + stats.discardedSplats);
}

TEST(GaussianSplatPass, ClipsBehindAndOutsideFrustum)
{
    // Splats at z = -5, -20 are in front of the camera (right-handed -Z look);
    // splat at z = +5 is behind, splat at z = -200 is past far plane.
    GaussianSplatSceneNode node{ makeCloudOnZ({ -5.f, -20.f, 5.f, -200.f }) };

    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&node);

    const auto stats = pass.computeStats();
    EXPECT_EQ(stats.submittedSplats, 4u);
    EXPECT_EQ(stats.projectedSplats, 2u);
    EXPECT_EQ(stats.discardedSplats, 2u);
    EXPECT_EQ(stats.splatDrawCalls,  1u); // one cloud, has projected splats
}

TEST(GaussianSplatPass, LowOpacityDiscarded)
{
    GaussianSplatSceneNode node{ makeCloudOnZ({ -5.f, -10.f }, /*opacity=*/-10.f) };

    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&node);

    const auto stats = pass.computeStats();
    EXPECT_EQ(stats.submittedSplats, 2u);
    EXPECT_EQ(stats.projectedSplats, 0u);
    EXPECT_EQ(stats.discardedSplats, 2u);
    EXPECT_EQ(stats.splatDrawCalls,  0u); // no projected splats → no draw call
}

TEST(GaussianSplatPass, HiddenCloudNotCounted)
{
    GaussianSplatSceneNode node{ makeCloudOnZ({ -5.f, -10.f }) };
    node.visible = false;

    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&node);

    const auto stats = pass.computeStats();
    EXPECT_EQ(stats.submittedSplats, 0u);
    EXPECT_EQ(stats.projectedSplats, 0u);
    EXPECT_EQ(stats.splatDrawCalls,  0u);
}

TEST(GaussianSplatPass, DeterministicAcrossRuns)
{
    GaussianSplatSceneNode node{ makeCloudOnZ({ -1.f, -5.f, -25.f, -75.f, -150.f }) };

    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&node);

    const auto a = pass.computeStats();
    const auto b = pass.computeStats();
    EXPECT_EQ(a.submittedSplats, b.submittedSplats);
    EXPECT_EQ(a.projectedSplats, b.projectedSplats);
    EXPECT_EQ(a.discardedSplats, b.discardedSplats);
    EXPECT_EQ(a.splatDrawCalls,  b.splatDrawCalls);
}

TEST(GaussianSplatPass, MultipleCloudsAccumulate)
{
    GaussianSplatSceneNode a{ makeCloudOnZ({ -5.f, -10.f }) };
    GaussianSplatSceneNode b{ makeCloudOnZ({ -2.f, -3.f, -7.f }) };

    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&a);
    pass.addCloud(&b);
    EXPECT_EQ(pass.attachedCloudCount(), 2u);

    const auto stats = pass.computeStats();
    EXPECT_EQ(stats.submittedSplats, 5u);
    EXPECT_EQ(stats.projectedSplats, 5u);
    EXPECT_EQ(stats.splatDrawCalls,  2u);

    pass.clearClouds();
    EXPECT_EQ(pass.attachedCloudCount(), 0u);
    EXPECT_EQ(pass.computeStats().submittedSplats, 0u);
}

TEST(GaussianSplatPass, SetCameraMatricesPreservesUserConfig)
{
    GaussianSplatPassConfig cfg = makeDefaultConfig();
    cfg.splatScaleMultiplier = 2.5f;
    cfg.opacityCutoffLogit   = -3.0f;
    cfg.sortMode             = SplatSortMode::None;

    GaussianSplatPass pass;
    pass.setConfig(cfg);

    Mat4 newView = Mat4::identity();
    newView.m[0][3] = 1.f; // translate
    pass.setCameraMatrices(newView, makePerspective(0.7f, 1.f, 0.1f, 10.f));

    EXPECT_FLOAT_EQ(pass.config().splatScaleMultiplier, 2.5f);
    EXPECT_FLOAT_EQ(pass.config().opacityCutoffLogit,  -3.0f);
    EXPECT_EQ(pass.config().sortMode, SplatSortMode::None);
    EXPECT_FLOAT_EQ(pass.config().view.m[0][3], 1.f);
}

// ── Renderer integration ──────────────────────────────────────────────────────

TEST(GaussianSplatPass, RendererAccumulatesIntoFrameStats)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;
    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = { 640, 480 };
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    GaussianSplatSceneNode node{ makeCloudOnZ({ -2.f, -5.f, -20.f, 5.f }) };
    GaussianSplatPass pass;
    pass.setConfig(makeDefaultConfig());
    pass.addCloud(&node);

    renderer.setGaussianSplatPass(&pass);
    EXPECT_EQ(renderer.gaussianSplatPass(), &pass);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(60.f, 640.f / 480.f, 0.5f, 100.f);
    cam.lookAt({ 0.f, 0.f, 10.f }, { 0.f, 0.f, 0.f });

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const auto& fs = renderer.lastFrameStats();
    EXPECT_EQ(fs.submittedSplats, 4u);
    // projectedSplats / splatDrawCalls depend on the camera matrix convention
    // (Camera::projection is stored in GLSL upload form); the integration test
    // asserts wiring + counter additivity rather than CPU-side projection math.
    EXPECT_LE(fs.projectedSplats, fs.submittedSplats);
    EXPECT_LE(fs.splatDrawCalls,  pass.attachedCloudCount());
    // Renderer must thread the active Camera matrices into the pass each frame.
    const auto& ubo = cam.ubo();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_FLOAT_EQ(pass.config().view.m[r][c],       ubo.view.m[r][c]);
            EXPECT_FLOAT_EQ(pass.config().projection.m[r][c], ubo.projection.m[r][c]);
        }
    }
    // splat draw calls are mirrored into the aggregate drawCalls counter.
    EXPECT_GE(fs.drawCalls, fs.splatDrawCalls);
}

TEST(GaussianSplatPass, RendererWithoutPassLeavesSplatStatsZero)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;
    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = { 640, 480 };
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);
    SceneGraph scene;
    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const auto& fs = renderer.lastFrameStats();
    EXPECT_EQ(fs.splatDrawCalls,  0u);
    EXPECT_EQ(fs.submittedSplats, 0u);
    EXPECT_EQ(fs.projectedSplats, 0u);
    EXPECT_EQ(renderer.gaussianSplatPass(), nullptr);
}
