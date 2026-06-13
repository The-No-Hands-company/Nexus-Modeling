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

TEST_F(GPUAllocatorTest, BudgetUtilizationIsZeroInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_FLOAT_EQ(computeBudgetUtilization(alloc), 0.0f);
}

TEST_F(GPUAllocatorTest, RemainingBudgetEqualsFullBudget)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(remainingBudgetBytes(alloc), alloc.budgetBytes());
}

TEST_F(GPUAllocatorTest, WouldExceedBudgetReturnsFalseForSmallAllocation)
{
    auto& alloc = ctx->allocator();
    EXPECT_FALSE(wouldExceedBudget(alloc, 1024));
}

TEST_F(GPUAllocatorTest, MemoryPressureIsLowInitially)
{
    auto& alloc = ctx->allocator();
    EXPECT_EQ(computeMemoryPressure(alloc), MemoryPressure::Low);
}

TEST_F(GPUAllocatorTest, IsOverBudgetRespectsThreshold)
{
    auto& alloc = ctx->allocator();
    EXPECT_FALSE(isOverBudget(alloc, 0.5f));
}

TEST_F(GPUAllocatorTest, BudgetIsPositive)
{
    auto& alloc = ctx->allocator();
    EXPECT_GT(alloc.budgetBytes(), 0u);
    EXPECT_GT(alloc.allocatedBytes() + remainingBudgetBytes(alloc), 0u);
}
