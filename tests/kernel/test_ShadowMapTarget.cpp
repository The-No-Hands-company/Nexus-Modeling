// ─────────────────────────────────────────────────────────────────────────────
//  Tests — ShadowMapTarget (Month 14 Track 2)
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/render/ShadowMapTarget.h>

#include "../../src/kernel/src/backend/null/NullDevice.h"

using namespace nexus::render;
using namespace nexus::gfx;

// ── Basic lifecycle ───────────────────────────────────────────────────────────

TEST(ShadowMapTarget, InitiallyNotReady) {
    ShadowMapTarget t;
    EXPECT_FALSE(t.isReady());
    EXPECT_FALSE(t.depthTexture().valid());
    EXPECT_FALSE(t.depthSampler().valid());
    EXPECT_EQ(0u, t.cascadeCount());
}

TEST(ShadowMapTarget, CreateIsReady) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{};
    desc.extent       = { 1024u, 1024u };
    desc.cascadeCount = 1u;

    EXPECT_TRUE(t.create(dev, desc));
    EXPECT_TRUE(t.isReady());
    EXPECT_TRUE(t.depthTexture().valid());
    EXPECT_TRUE(t.depthSampler().valid());
    EXPECT_EQ(1u, t.cascadeCount());
    EXPECT_EQ(1024u, t.extent().width);
    EXPECT_EQ(1024u, t.extent().height);

    t.destroy(dev);
}

TEST(ShadowMapTarget, CreateWithMultipleCascades) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{};
    desc.extent       = { 2048u, 2048u };
    desc.cascadeCount = 4u;

    EXPECT_TRUE(t.create(dev, desc));
    EXPECT_EQ(4u, t.cascadeCount());

    t.destroy(dev);
}

TEST(ShadowMapTarget, ZeroExtentWidthIsNotReady) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{};
    desc.extent       = { 0u, 1024u };
    desc.cascadeCount = 1u;

    EXPECT_FALSE(t.create(dev, desc));
    EXPECT_FALSE(t.isReady());
}

TEST(ShadowMapTarget, ZeroExtentHeightIsNotReady) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{};
    desc.extent       = { 1024u, 0u };
    desc.cascadeCount = 1u;

    EXPECT_FALSE(t.create(dev, desc));
    EXPECT_FALSE(t.isReady());
}

TEST(ShadowMapTarget, ZeroCascadeCountIsNotReady) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{};
    desc.extent       = { 512u, 512u };
    desc.cascadeCount = 0u;

    EXPECT_FALSE(t.create(dev, desc));
    EXPECT_FALSE(t.isReady());
}

// ── Resize ────────────────────────────────────────────────────────────────────

TEST(ShadowMapTarget, ResizeReleasesAndReallocates) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{};
    desc.extent       = { 512u, 512u };
    desc.cascadeCount = 2u;

    EXPECT_TRUE(t.create(dev, desc));

    const auto oldTex = t.depthTexture();
    const auto oldSmp = t.depthSampler();

    EXPECT_TRUE(t.resize(dev, { 1024u, 1024u }));
    EXPECT_TRUE(t.isReady());
    EXPECT_EQ(1024u, t.extent().width);
    EXPECT_EQ(1024u, t.extent().height);
    // NullDevice increments a counter, so new handles have different IDs
    EXPECT_NE(oldTex.id, t.depthTexture().id);
    EXPECT_NE(oldSmp.id, t.depthSampler().id);

    t.destroy(dev);
}

TEST(ShadowMapTarget, ResizePreservesCascadeCount) {
    NullDevice dev;
    ShadowMapTarget t;

    ShadowMapTargetDesc desc{ { 512u, 512u }, 3u };
    EXPECT_TRUE(t.create(dev, desc));
    EXPECT_TRUE(t.resize(dev, { 256u, 256u }));
    EXPECT_EQ(3u, t.cascadeCount());

    t.destroy(dev);
}

TEST(ShadowMapTarget, ResizeToZeroExtentDestroys) {
    NullDevice dev;
    ShadowMapTarget t;

    EXPECT_TRUE(t.create(dev, { { 512u, 512u }, 1u }));
    EXPECT_FALSE(t.resize(dev, { 0u, 256u }));
    EXPECT_FALSE(t.isReady());
}

// ── Destroy ───────────────────────────────────────────────────────────────────

TEST(ShadowMapTarget, DestroyInvalidatesHandles) {
    NullDevice dev;
    ShadowMapTarget t;

    EXPECT_TRUE(t.create(dev, { { 1024u, 1024u }, 1u }));
    EXPECT_TRUE(t.isReady());

    t.destroy(dev);

    EXPECT_FALSE(t.isReady());
    EXPECT_FALSE(t.depthTexture().valid());
    EXPECT_FALSE(t.depthSampler().valid());
    EXPECT_EQ(0u, t.cascadeCount());
}

TEST(ShadowMapTarget, DestroyBeforeCreateIsSafe) {
    NullDevice dev;
    ShadowMapTarget t;
    t.destroy(dev); // must not crash
    EXPECT_FALSE(t.isReady());
}

TEST(ShadowMapTarget, DoubleDestroyIsSafe) {
    NullDevice dev;
    ShadowMapTarget t;

    EXPECT_TRUE(t.create(dev, { { 512u, 512u }, 1u }));
    t.destroy(dev);
    t.destroy(dev); // must not crash
    EXPECT_FALSE(t.isReady());
}
