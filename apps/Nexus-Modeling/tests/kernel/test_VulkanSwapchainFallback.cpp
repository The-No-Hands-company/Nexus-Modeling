// Regression: exercise Vulkan fallback path where swapchain images are not
// storage-writable. This test uses an environment hook to simulate a driver
// that created the swapchain without VK_IMAGE_USAGE_STORAGE_BIT.

#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/FrameScheduler.h>

#include "../../src/kernel/src/backend/vulkan/VulkanDevice.h"
#include "../../src/kernel/src/backend/vulkan/VulkanSwapchain.h"
#include "../../src/kernel/src/backend/vulkan/VulkanFrameScheduler.h"

#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(VulkanSwapchainFallback, BeginFrameUsesIntermediateWhenSwapchainNotStorage)
{
    // Force the VulkanSwapchain to report no STORAGE usage (test-only hook)
    setenv("NEXUS_FORCE_SWAPCHAIN_NO_STORAGE", "1", 1);

    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Vulkan;
    desc.validation = ValidationLevel::Off;

    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }

    SwapchainDesc sd{};
    sd.nativeWindowHandle = nullptr; // headless
    sd.extent = { 256, 256 };
    sd.imageCount = 2;

    auto sc = ctx->createSwapchain(sd);
    if (!sc) GTEST_SKIP() << "Swapchain creation returned null (headless/unsupported)";

    auto* vkDev = dynamic_cast<VulkanDevice*>(&ctx->device());
    auto* vkSc  = dynamic_cast<VulkanSwapchain*>(sc.get());
    if (!vkDev || !vkSc) GTEST_SKIP() << "Vulkan concrete types unavailable";

    // Construct scheduler — it will create intermediate storage textures if
    // the recorded vkImageUsageFlags() lacks VK_IMAGE_USAGE_STORAGE_BIT.
    auto sched = std::make_unique<VulkanFrameScheduler>(*vkDev, *vkSc, 2);

    auto frame = sched->beginFrame();
    ASSERT_TRUE(frame.has_value());

    // When fallback is active, the scheduler tells the renderer to end the
    // color target in TransferSrc so we can copy to the real swapchain image.
    EXPECT_EQ(frame->finalColorLayout, TextureLayout::TransferSrc);

    sched->endFrame();

    // Clean up env override
    unsetenv("NEXUS_FORCE_SWAPCHAIN_NO_STORAGE");
}
