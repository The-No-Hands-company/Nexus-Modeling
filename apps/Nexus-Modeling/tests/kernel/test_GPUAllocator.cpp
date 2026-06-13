#include <gtest/gtest.h>

#include <nexus/gfx/Allocator.h>
#include <nexus/gfx/RenderContext.h>

using namespace nexus::gfx;

class GPUAllocatorTest : public ::testing::Test {
protected:
    std::unique_ptr<RenderContext> ctx;

    void SetUp() override {
        RenderContextDesc desc;
        desc.preferredBackend = Backend::Null;
        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);
    }
};

TEST_F(GPUAllocatorTest, BudgetIsNonNegative)
{
    auto& alloc = ctx->allocator();
    EXPECT_GE(alloc.budgetBytes(), 0u);
}

TEST_F(GPUAllocatorTest, AllocatedZeroInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.allocatedBytes(), 0u);
}

TEST_F(GPUAllocatorTest, UsedBytesZeroInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.usedBytes(), 0u);
}

TEST_F(GPUAllocatorTest, BudgetMinusAllocatedEqualsUsedInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(alloc.usedBytes(), alloc.allocatedBytes());
    EXPECT_LE(alloc.allocatedBytes(), alloc.budgetBytes());
}
