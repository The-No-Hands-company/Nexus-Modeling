// ─────────────────────────────────────────────────────────────────────────────
//  Tests — DescriptorBinder (Month 14 Track 1)
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/render/DescriptorBinder.h>

// Pull in NullDevice for headless testing.
#include "../../src/kernel/src/backend/null/NullDevice.h"

using namespace nexus::render;
using namespace nexus::gfx;

// ── Helpers ───────────────────────────────────────────────────────────────────

static TextureHandle makeTexture(NullDevice& dev) {
    return dev.createTexture(TextureDesc{});
}
static SamplerHandle makeSampler(NullDevice& dev) {
    return dev.createSampler(SamplerDesc{});
}
static BufferHandle makeBuffer(NullDevice& dev) {
    return dev.createBuffer(BufferDesc{});
}

// ── CompositeDescriptorSet ────────────────────────────────────────────────────

TEST(DescriptorBinder, CompositeDescriptorSetInitiallyNotReady) {
    CompositeDescriptorSet ds;
    EXPECT_FALSE(ds.isReady());
    EXPECT_FALSE(ds.descriptorSet().valid());
}

TEST(DescriptorBinder, CompositeDescriptorSetAllocatesAndUpdates) {
    NullDevice dev;
    CompositeDescriptorSet ds;

    CompositeDescriptorInputs inputs;
    inputs.albedo   = makeTexture(dev);
    inputs.normal   = makeTexture(dev);
    inputs.velocity = makeTexture(dev);
    inputs.depth    = makeTexture(dev);
    inputs.sampler  = makeSampler(dev);

    EXPECT_TRUE(ds.update(dev, inputs));
    EXPECT_TRUE(ds.isReady());
    EXPECT_TRUE(ds.descriptorSet().valid());

    ds.destroy(dev);
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, CompositeDescriptorSetRejectsInvalidAlbedo) {
    NullDevice dev;
    CompositeDescriptorSet ds;

    CompositeDescriptorInputs inputs;
    // albedo left invalid
    inputs.normal   = makeTexture(dev);
    inputs.velocity = makeTexture(dev);
    inputs.depth    = makeTexture(dev);
    inputs.sampler  = makeSampler(dev);

    EXPECT_FALSE(ds.update(dev, inputs));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, CompositeDescriptorSetRejectsInvalidSampler) {
    NullDevice dev;
    CompositeDescriptorSet ds;

    CompositeDescriptorInputs inputs;
    inputs.albedo   = makeTexture(dev);
    inputs.normal   = makeTexture(dev);
    inputs.velocity = makeTexture(dev);
    inputs.depth    = makeTexture(dev);
    // sampler left invalid

    EXPECT_FALSE(ds.update(dev, inputs));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, CompositeDescriptorSetDoubleDestroyIsSafe) {
    NullDevice dev;
    CompositeDescriptorSet ds;

    CompositeDescriptorInputs inputs;
    inputs.albedo   = makeTexture(dev);
    inputs.normal   = makeTexture(dev);
    inputs.velocity = makeTexture(dev);
    inputs.depth    = makeTexture(dev);
    inputs.sampler  = makeSampler(dev);

    EXPECT_TRUE(ds.update(dev, inputs));
    ds.destroy(dev);
    ds.destroy(dev); // must not crash
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, CompositeDescriptorSetCanUpdateTwice) {
    NullDevice dev;
    CompositeDescriptorSet ds;

    auto makeInputs = [&]() {
        CompositeDescriptorInputs i;
        i.albedo   = makeTexture(dev);
        i.normal   = makeTexture(dev);
        i.velocity = makeTexture(dev);
        i.depth    = makeTexture(dev);
        i.sampler  = makeSampler(dev);
        return i;
    };

    EXPECT_TRUE(ds.update(dev, makeInputs()));
    const auto firstHandle = ds.descriptorSet();
    EXPECT_TRUE(ds.update(dev, makeInputs()));
    // Same handle should be reused on second update (no new allocation)
    EXPECT_EQ(firstHandle.id, ds.descriptorSet().id);

    ds.destroy(dev);
}

// ── MaterialTableDescriptorSet ────────────────────────────────────────────────

TEST(DescriptorBinder, MaterialTableDescriptorSetInitiallyNotReady) {
    MaterialTableDescriptorSet ds;
    EXPECT_FALSE(ds.isReady());
    EXPECT_EQ(0u, ds.offsetBytes());
}

TEST(DescriptorBinder, MaterialTableDescriptorSetAllocatesAndUpdates) {
    NullDevice dev;
    MaterialTableDescriptorSet ds;

    auto buf = makeBuffer(dev);
    EXPECT_TRUE(ds.update(dev, buf, 0u));
    EXPECT_TRUE(ds.isReady());
    EXPECT_EQ(0u, ds.offsetBytes());

    ds.destroy(dev);
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, MaterialTableDescriptorSetRejectsInvalidOffset) {
    NullDevice dev;
    MaterialTableDescriptorSet ds;

    auto buf = makeBuffer(dev);
    // offsetBytes not 4-byte aligned
    EXPECT_FALSE(ds.update(dev, buf, 3u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, MaterialTableDescriptorSetRejectsOffsetBeyondCapacity) {
    NullDevice dev;
    MaterialTableDescriptorSet ds;

    auto buf = makeBuffer(dev);
    // offset (256) >= capacity (256) should fail
    EXPECT_FALSE(ds.update(dev, buf, 256u, 256u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, MaterialTableDescriptorSetAcceptsOffsetWithinCapacity) {
    NullDevice dev;
    MaterialTableDescriptorSet ds;

    auto buf = makeBuffer(dev);
    EXPECT_TRUE(ds.update(dev, buf, 128u, 256u));
    EXPECT_TRUE(ds.isReady());
    EXPECT_EQ(128u, ds.offsetBytes());

    ds.destroy(dev);
}

TEST(DescriptorBinder, MaterialTableDescriptorSetRejectsInvalidBuffer) {
    NullDevice dev;
    MaterialTableDescriptorSet ds;

    EXPECT_FALSE(ds.update(dev, BufferHandle{}, 0u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, MaterialTableDescriptorSetZeroCapacitySkipsRangeCheck) {
    NullDevice dev;
    MaterialTableDescriptorSet ds;

    auto buf = makeBuffer(dev);
    // capacityBytes == 0 means unbounded; any aligned offset should pass
    EXPECT_TRUE(ds.update(dev, buf, 1024u, 0u));
    EXPECT_TRUE(ds.isReady());

    ds.destroy(dev);
}

// ── ShadowDescriptorSet ───────────────────────────────────────────────────────

TEST(DescriptorBinder, ShadowDescriptorSetInitiallyNotReady) {
    ShadowDescriptorSet ds;
    EXPECT_FALSE(ds.isReady());
    EXPECT_EQ(0u, ds.cascadeCount());
}

TEST(DescriptorBinder, ShadowDescriptorSetAllocatesAndUpdates) {
    NullDevice dev;
    ShadowDescriptorSet ds;

    auto tex = makeTexture(dev);
    auto smp = makeSampler(dev);
    auto buf = makeBuffer(dev);

    EXPECT_TRUE(ds.update(dev, tex, smp, buf, 4u));
    EXPECT_TRUE(ds.isReady());
    EXPECT_EQ(4u, ds.cascadeCount());

    ds.destroy(dev);
    EXPECT_FALSE(ds.isReady());
    EXPECT_EQ(0u, ds.cascadeCount());
}

TEST(DescriptorBinder, ShadowDescriptorSetRequiresCascadeCountNonZero) {
    NullDevice dev;
    ShadowDescriptorSet ds;

    EXPECT_FALSE(ds.update(dev, makeTexture(dev), makeSampler(dev), makeBuffer(dev), 0u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, ShadowDescriptorSetRejectsInvalidTexture) {
    NullDevice dev;
    ShadowDescriptorSet ds;

    EXPECT_FALSE(ds.update(dev, TextureHandle{}, makeSampler(dev), makeBuffer(dev), 1u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, ShadowDescriptorSetRejectsInvalidSampler) {
    NullDevice dev;
    ShadowDescriptorSet ds;

    EXPECT_FALSE(ds.update(dev, makeTexture(dev), SamplerHandle{}, makeBuffer(dev), 1u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, ShadowDescriptorSetRejectsInvalidBuffer) {
    NullDevice dev;
    ShadowDescriptorSet ds;

    EXPECT_FALSE(ds.update(dev, makeTexture(dev), makeSampler(dev), BufferHandle{}, 1u));
    EXPECT_FALSE(ds.isReady());
}

TEST(DescriptorBinder, ShadowDescriptorSetDoubleDestroyIsSafe) {
    NullDevice dev;
    ShadowDescriptorSet ds;
    EXPECT_TRUE(ds.update(dev, makeTexture(dev), makeSampler(dev), makeBuffer(dev), 2u));
    ds.destroy(dev);
    ds.destroy(dev);
    EXPECT_FALSE(ds.isReady());
}
