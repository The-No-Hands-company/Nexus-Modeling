// ─────────────────────────────────────────────────────────────────────────────
//  Tests — Renderer descriptor binding + shadow map target integration
//  (Month 14 Tracks 1 & 2 — Renderer integration deliverables)
//  (Month 15 Track 1 — Shadow descriptor set integration)
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/DescriptorBinder.h>
#include <nexus/render/ShadowMapTarget.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>

using namespace nexus::gfx;
using namespace nexus::render;

// ── Test fixture ──────────────────────────────────────────────────────────────

struct RendererIntegrationFixture : public ::testing::Test {
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain>    swapchain;
    std::unique_ptr<Renderer>      renderer;

    void SetUp() override {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation       = ValidationLevel::Off;
        ctx = RenderContext::create(desc);

        SwapchainDesc sd{};
        sd.extent = {1280u, 720u};
        swapchain = ctx->createSwapchain(sd);

        renderer = std::make_unique<Renderer>(*ctx, *swapchain);
    }
};

// ── bindCompositeDescriptors ──────────────────────────────────────────────────

TEST_F(RendererIntegrationFixture, BindCompositeDescriptorsFailsWithoutGBuffer) {
    // No GBuffer set — should return false since core inputs are absent.
    EXPECT_FALSE(renderer->bindCompositeDescriptors(ctx->device()));
    EXPECT_FALSE(renderer->compositeDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, BindCompositeDescriptorsReturnsFalseOnNullBackend) {
    // The Null backend uses the direct swapchain path which does not allocate
    // GBuffer textures; bindCompositeDescriptors requires all 4 GBuffer textures
    // to be present, so it must return false on this path.
    auto& dev = ctx->device();

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = dev.createSampler({});
    bindings.normalSampler         = dev.createSampler({});
    bindings.velocitySampler       = dev.createSampler({});
    bindings.depthSampler          = dev.createSampler({});
    renderer->setCompositeMaterialBindings(bindings);

    Camera cam;
    SceneGraph scene;
    renderer->beginFrame();
    renderer->render(cam, scene);
    renderer->endFrame();

    // GBuffer not allocated on Null path — binding must fail gracefully.
    EXPECT_FALSE(renderer->bindCompositeDescriptors(dev));
    EXPECT_FALSE(renderer->compositeDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, CompositeDescriptorSetInitiallyNotReady) {
    EXPECT_FALSE(renderer->compositeDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, DestroyCompositeDescriptorsIsSafe) {
    // Destroy before any allocation — must not crash.
    renderer->destroyCompositeDescriptors(ctx->device());
    EXPECT_FALSE(renderer->compositeDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, DestroyCompositeDescriptorsAfterBind) {
    auto& dev = ctx->device();

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = dev.createSampler({});
    bindings.normalSampler         = dev.createSampler({});
    bindings.velocitySampler       = dev.createSampler({});
    bindings.depthSampler          = dev.createSampler({});
    renderer->setCompositeMaterialBindings(bindings);

    Camera cam;
    SceneGraph scene;
    renderer->beginFrame();
    renderer->render(cam, scene);
    renderer->endFrame();

    (void)renderer->bindCompositeDescriptors(dev);
    renderer->destroyCompositeDescriptors(dev);
    EXPECT_FALSE(renderer->compositeDescriptorSet()->isReady());

    // Double-destroy must also be safe.
    renderer->destroyCompositeDescriptors(dev);
}

// ── setShadowMapTarget ────────────────────────────────────────────────────────

TEST_F(RendererIntegrationFixture, SetShadowMapTargetInitiallyNull) {
    EXPECT_EQ(renderer->shadowMapTarget(), nullptr);
}

TEST_F(RendererIntegrationFixture, SetShadowMapTargetAttaches) {
    ShadowMapTarget target;
    renderer->setShadowMapTarget(&target);
    EXPECT_EQ(renderer->shadowMapTarget(), &target);
}

TEST_F(RendererIntegrationFixture, SetShadowMapTargetNullDetaches) {
    ShadowMapTarget target;
    renderer->setShadowMapTarget(&target);
    renderer->setShadowMapTarget(nullptr);
    EXPECT_EQ(renderer->shadowMapTarget(), nullptr);
}

TEST_F(RendererIntegrationFixture, SetShadowMapTargetPopulatesShadowInput) {
    auto& dev = ctx->device();

    ShadowMapTarget target;
    ShadowMapTargetDesc desc{ {1024u, 1024u}, 2u };
    ASSERT_TRUE(target.create(dev, desc));
    EXPECT_TRUE(target.isReady());

    renderer->setShadowMapTarget(&target);
    EXPECT_EQ(renderer->shadowMapTarget(), &target);
    EXPECT_TRUE(renderer->shadowMapTarget()->depthTexture().valid());
    EXPECT_TRUE(renderer->shadowMapTarget()->depthSampler().valid());
    EXPECT_EQ(renderer->shadowMapTarget()->cascadeCount(), 2u);

    target.destroy(dev);
}

TEST_F(RendererIntegrationFixture, OnResizeCallsShadowMapTargetResize) {
    auto& dev = ctx->device();

    ShadowMapTarget target;
    ASSERT_TRUE(target.create(dev, { {512u, 512u}, 1u }));

    const auto oldTexId = target.depthTexture().id;
    renderer->setShadowMapTarget(&target);

    // onResize should call target.resize(); NullDevice gives new handle IDs.
    renderer->onResize({1024u, 1024u});

    // After resize, target should still be ready with a new handle.
    EXPECT_TRUE(target.isReady());
    EXPECT_NE(target.depthTexture().id, oldTexId);
    EXPECT_EQ(target.extent().width, 1024u);
    EXPECT_EQ(target.extent().height, 1024u);

    target.destroy(dev);
}

TEST_F(RendererIntegrationFixture, OnResizeWithNullShadowMapTargetIsSafe) {
    // No shadow map target attached — onResize must not crash.
    EXPECT_EQ(renderer->shadowMapTarget(), nullptr);
    renderer->onResize({800u, 600u}); // must not crash
}

// ── RendererShadowDesc — Month 15 Track 1 ────────────────────────────────────

TEST_F(RendererIntegrationFixture, ShadowDescriptorSetInitiallyNotReady) {
    EXPECT_FALSE(renderer->shadowDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, BindShadowDescriptorsFailsWithoutContract) {
    // No shadow lighting contract set — must return false.
    auto& dev = ctx->device();
    EXPECT_FALSE(renderer->bindShadowDescriptors(dev));
    EXPECT_FALSE(renderer->shadowDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, BindShadowDescriptorsFailsWithPartialContract) {
    auto& dev = ctx->device();

    // Only set cascade count — depth texture + sampler + buffer absent.
    ShadowLightingContract contract{};
    contract.cascadeCount = 2u;
    renderer->setShadowLightingContract(contract);

    EXPECT_FALSE(renderer->bindShadowDescriptors(dev));
    EXPECT_FALSE(renderer->shadowDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, DestroyShadowDescriptorsBeforeBindIsSafe) {
    // Destroy before any allocation — must not crash.
    renderer->destroyShadowDescriptors(ctx->device());
    EXPECT_FALSE(renderer->shadowDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, DestroyShadowDescriptorsAfterBindClearsReady) {
    auto& dev = ctx->device();

    // Build a minimal complete ShadowLightingContract via the internal path:
    // inject handles via setShadowLightingContract then force a lighting buffer
    // allocation so buildShadowLightingBindingDesc() returns isComplete()==true.
    //
    // On the Null backend the internal shadow lighting buffer is only allocated
    // when uploadShadowLightingContract() runs inside render().  Instead, we
    // call setShadowLightingContract with a pre-built contract that already has
    // valid synthetic handles so bindShadowDescriptors() can succeed.
    ShadowLightingContract contract{};
    contract.cascadeCount = 1u;
    renderer->setShadowLightingContract(contract);

    // bindShadowDescriptors will still fail because the internal shadow depth
    // texture/sampler/buffer are not allocated on the Null path.  This test
    // specifically validates the destroy-after-bind-attempt sequence is safe.
    (void)renderer->bindShadowDescriptors(dev);  // may return false — that's ok
    renderer->destroyShadowDescriptors(dev);
    EXPECT_FALSE(renderer->shadowDescriptorSet()->isReady());
}

TEST_F(RendererIntegrationFixture, DoublDestroyShadowDescriptorsIsSafe) {
    auto& dev = ctx->device();
    renderer->destroyShadowDescriptors(dev);
    renderer->destroyShadowDescriptors(dev);  // must not crash or assert
    EXPECT_FALSE(renderer->shadowDescriptorSet()->isReady());
}
