// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation gfx_device.* / gfx_alloc.* /
//  gfx_swapchain.* / gfx_cmdbuf.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/GfxDeviceExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerGfxDeviceCommands(h);
    return h;
}

static bool run(ScriptBatchHarness& h, const std::string& name,
                std::initializer_list<std::pair<std::string,std::string>> args,
                std::vector<std::string>& msgs)
{
    ScriptCommand cmd;
    cmd.name = name;
    for (auto& [k, v] : args) cmd.args[k] = v;
    ScriptContext ctx;
    msgs.clear();
    return h.registry().execute(ctx, cmd, msgs);
}

static bool hasMsg(const std::vector<std::string>& msgs, const std::string& sub)
{
    for (const auto& m : msgs)
        if (m.find(sub) != std::string::npos) return true;
    return false;
}

// ── gfx_device.describe ──────────────────────────────────────────────────────

TEST(GfxDeviceExtension, DescribeBackendNull)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "backend=Null"));
}

TEST(GfxDeviceExtension, DescribeTierLow)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "tier=Low"));
}

TEST(GfxDeviceExtension, DescribeDeviceName)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "name=NullDevice"));
}

TEST(GfxDeviceExtension, DescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── gfx_device.create_buffer ─────────────────────────────────────────────────

TEST(GfxDeviceExtension, CreateBufferDefaultSize)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.create_buffer", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_device.create_buffer ok"));
    EXPECT_TRUE(hasMsg(msgs, "valid=1"));
}

TEST(GfxDeviceExtension, CreateBufferCustomSize)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.create_buffer", {{"size","4096"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "valid=1"));
}

// ── gfx_device.wait_idle ─────────────────────────────────────────────────────

TEST(GfxDeviceExtension, WaitIdleOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_device.wait_idle", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_device.wait_idle ok"));
}

// ── gfx_alloc.describe ───────────────────────────────────────────────────────

TEST(GfxDeviceExtension, AllocDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_alloc.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "budget=0"));
    EXPECT_TRUE(hasMsg(msgs, "allocated=0"));
    EXPECT_TRUE(hasMsg(msgs, "used=0"));
}

TEST(GfxDeviceExtension, AllocDescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_alloc.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── gfx_swapchain.acquire ────────────────────────────────────────────────────

TEST(GfxDeviceExtension, SwapchainAcquireOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_swapchain.acquire", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_swapchain.acquire ok"));
    EXPECT_TRUE(hasMsg(msgs, "result=Ok"));
    EXPECT_TRUE(hasMsg(msgs, "image="));
}

TEST(GfxDeviceExtension, SwapchainAcquireMultipleFrames)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_swapchain.acquire", {}, msgs);
    ok = run(h, "gfx_swapchain.acquire", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_swapchain.acquire", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=Ok"));
}

// ── gfx_swapchain.present ────────────────────────────────────────────────────

TEST(GfxDeviceExtension, SwapchainPresentOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "gfx_swapchain.acquire", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_swapchain.present", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_swapchain.present ok"));
    EXPECT_TRUE(hasMsg(msgs, "result=Ok"));
}

// ── gfx_swapchain.resize ─────────────────────────────────────────────────────

TEST(GfxDeviceExtension, SwapchainResizeOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_swapchain.resize", {{"width","1920"},{"height","1080"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_swapchain.resize ok"));
    EXPECT_TRUE(hasMsg(msgs, "width=1920"));
    EXPECT_TRUE(hasMsg(msgs, "height=1080"));
}

TEST(GfxDeviceExtension, SwapchainResizeUpdatesDescribe)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "gfx_swapchain.resize",
        {{"width","800"},{"height","600"}}, msgs);
    EXPECT_TRUE(run(h, "gfx_swapchain.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=800"));
    EXPECT_TRUE(hasMsg(msgs, "height=600"));
}

// ── gfx_swapchain.describe ───────────────────────────────────────────────────

TEST(GfxDeviceExtension, SwapchainDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_swapchain.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=1280"));
    EXPECT_TRUE(hasMsg(msgs, "height=720"));
    EXPECT_TRUE(hasMsg(msgs, "images=3"));
    EXPECT_TRUE(hasMsg(msgs, "hdr=0"));
}

TEST(GfxDeviceExtension, SwapchainDescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_swapchain.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── gfx_cmdbuf.alloc ─────────────────────────────────────────────────────────

TEST(GfxDeviceExtension, CmdBufAllocGraphics)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_cmdbuf.alloc", {{"queue","Graphics"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_cmdbuf.alloc ok"));
    EXPECT_TRUE(hasMsg(msgs, "valid=1"));
    EXPECT_TRUE(hasMsg(msgs, "queue=Graphics"));
}

TEST(GfxDeviceExtension, CmdBufAllocCompute)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_cmdbuf.alloc", {{"queue","Compute"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "queue=Compute"));
}

TEST(GfxDeviceExtension, CmdBufAllocDefaultIsGraphics)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_cmdbuf.alloc", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "queue=Graphics"));
}

// ── gfx_cmdbuf.describe ──────────────────────────────────────────────────────

TEST(GfxDeviceExtension, CmdBufDescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_cmdbuf.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "acquires=0"));
    EXPECT_TRUE(hasMsg(msgs, "presents=0"));
}

TEST(GfxDeviceExtension, CmdBufDescribeAfterAcquirePresent)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_swapchain.acquire", {}, msgs);
    ok = run(h, "gfx_swapchain.present", {}, msgs);
    ok = run(h, "gfx_swapchain.acquire", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_cmdbuf.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "acquires=2"));
    EXPECT_TRUE(hasMsg(msgs, "presents=1"));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(GfxDeviceExtension, FullFrameLoop)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Describe device
    EXPECT_TRUE(run(h, "gfx_device.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "backend=Null"));

    // Allocate a buffer
    EXPECT_TRUE(run(h, "gfx_device.create_buffer", {{"size","256"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "valid=1"));

    // Allocate a command buffer
    EXPECT_TRUE(run(h, "gfx_cmdbuf.alloc", {{"queue","Graphics"}}, msgs));

    // 3 frames
    for (int i = 0; i < 3; ++i) {
        ok = run(h, "gfx_swapchain.acquire", {}, msgs);
        ok = run(h, "gfx_swapchain.present", {}, msgs);
    }

    EXPECT_TRUE(run(h, "gfx_cmdbuf.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "acquires=3"));
    EXPECT_TRUE(hasMsg(msgs, "presents=3"));

    // Resize
    ok = run(h, "gfx_swapchain.resize", {{"width","2560"},{"height","1440"}}, msgs);
    EXPECT_TRUE(run(h, "gfx_swapchain.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=2560"));

    // Alloc describe stays zero on null backend
    EXPECT_TRUE(run(h, "gfx_alloc.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "budget=0"));

    // Wait idle is safe
    EXPECT_TRUE(run(h, "gfx_device.wait_idle", {}, msgs));
}

TEST(GfxDeviceExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h1, "gfx_swapchain.acquire", {}, msgs);
    ok = run(h1, "gfx_swapchain.present", {}, msgs);

    EXPECT_TRUE(run(h2, "gfx_cmdbuf.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "acquires=0"));
    EXPECT_TRUE(hasMsg(msgs, "presents=0"));
}
