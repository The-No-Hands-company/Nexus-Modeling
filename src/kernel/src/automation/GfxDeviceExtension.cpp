// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Null GFX device / allocator / swapchain extension
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/GfxDeviceExtension.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/Allocator.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/Types.h>

// Null backend implementations (internal to the kernel)
#include <backend/null/NullDevice.h>
#include <backend/null/NullAllocator.h>
#include <backend/null/NullSwapchain.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
gdGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = gdGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint64_t uint64Arg(const ScriptCommand& cmd,
                                        std::string_view key, uint64_t def)
{
    if (const auto v = gdGetArg(cmd, key)) {
        uint64_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct GfxDeviceState {
    nexus::gfx::NullDevice    device;
    nexus::gfx::NullAllocator allocator;
    nexus::gfx::NullSwapchain swapchain{nexus::gfx::Extent2D{1280u, 720u}};
    uint32_t acquireCount  = 0;
    uint32_t presentCount  = 0;
    uint32_t lastImageIdx  = 0;
    bool     lastAcquireOk = false;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerGfxDeviceCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<GfxDeviceState>();

    // ── gfx_device.describe ───────────────────────────────────────────────────
    //
    // Reports null device backend, tier and name.  Always returns true.
    //
    // "gfx_device.describe backend=Null tier=Low name=NullDevice"
    harness.registry().registerCommand("gfx_device.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const std::string tierStr = [](nexus::gfx::HardwareTier t) -> std::string {
                switch (t) {
                    case nexus::gfx::HardwareTier::Low:   return "Low";
                    case nexus::gfx::HardwareTier::Mid:   return "Mid";
                    case nexus::gfx::HardwareTier::High:  return "High";
                    case nexus::gfx::HardwareTier::Ultra: return "Ultra";
                }
                return "Unknown";
            }(state->device.tier());

            messages.push_back("gfx_device.describe"
                " backend=Null"
                " tier="  + tierStr +
                " name="  + std::string(state->device.deviceName()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_device.create_buffer ──────────────────────────────────────────────
    //
    // Creates a buffer via the null device.
    //
    // Arguments:
    //   size=<N>   size in bytes (default 64)
    //
    // On success: "gfx_device.create_buffer ok valid=<0|1>"
    harness.registry().registerCommand("gfx_device.create_buffer",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint64_t sz = uint64Arg(cmd, "size", 64u);

            nexus::gfx::BufferDesc desc;
            desc.sizeBytes = sz;
            desc.usage     = nexus::gfx::BufferUsage::StorageBuffer;
            const auto h   = state->device.createBuffer(desc);

            messages.push_back("gfx_device.create_buffer ok"
                " valid=" + std::string(h.valid() ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_device.wait_idle ─────────────────────────────────────────────────
    //
    // Calls waitIdle on the null device (no-op on null backend).
    //
    // On success: "gfx_device.wait_idle ok"
    harness.registry().registerCommand("gfx_device.wait_idle",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->device.waitIdle();
            messages.push_back("gfx_device.wait_idle ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_alloc.describe ───────────────────────────────────────────────────
    //
    // Reports allocator budget statistics.  Always returns true.
    //
    // "gfx_alloc.describe budget=<B> allocated=<A> used=<U>"
    harness.registry().registerCommand("gfx_alloc.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("gfx_alloc.describe"
                " budget="    + std::to_string(state->allocator.budgetBytes()) +
                " allocated=" + std::to_string(state->allocator.allocatedBytes()) +
                " used="      + std::to_string(state->allocator.usedBytes()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_swapchain.acquire ────────────────────────────────────────────────
    //
    // Acquires the next swapchain frame from the null swapchain.
    //
    // On success: "gfx_swapchain.acquire ok result=Ok image=<I>"
    harness.registry().registerCommand("gfx_swapchain.acquire",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto frame = state->swapchain.acquire();

            std::string resultStr;
            switch (frame.result) {
                case nexus::gfx::AcquireResult::Ok:         resultStr = "Ok";         break;
                case nexus::gfx::AcquireResult::Suboptimal: resultStr = "Suboptimal"; break;
                case nexus::gfx::AcquireResult::OutOfDate:  resultStr = "OutOfDate";  break;
            }

            state->lastImageIdx  = frame.imageIndex;
            state->lastAcquireOk = (frame.result != nexus::gfx::AcquireResult::OutOfDate);
            state->acquireCount++;

            messages.push_back("gfx_swapchain.acquire ok"
                " result=" + resultStr +
                " image="  + std::to_string(frame.imageIndex));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_swapchain.present ────────────────────────────────────────────────
    //
    // Presents the last acquired frame.
    //
    // On success: "gfx_swapchain.present ok result=Ok"
    harness.registry().registerCommand("gfx_swapchain.present",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto res = state->swapchain.present(
                state->lastImageIdx, nexus::gfx::SemaphoreHandle{});
            state->presentCount++;

            std::string resultStr;
            switch (res) {
                case nexus::gfx::PresentResult::Ok:         resultStr = "Ok";         break;
                case nexus::gfx::PresentResult::Suboptimal: resultStr = "Suboptimal"; break;
                case nexus::gfx::PresentResult::OutOfDate:  resultStr = "OutOfDate";  break;
            }

            messages.push_back("gfx_swapchain.present ok"
                " result=" + resultStr);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_swapchain.resize ─────────────────────────────────────────────────
    //
    // Resizes the null swapchain.
    //
    // Arguments:
    //   width=<N>   new width  (default 1280)
    //   height=<N>  new height (default 720)
    //
    // On success: "gfx_swapchain.resize ok width=<W> height=<H>"
    harness.registry().registerCommand("gfx_swapchain.resize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t w = uintArg(cmd, "width",  1280u);
            const uint32_t h = uintArg(cmd, "height",  720u);

            state->swapchain.resize({w, h});

            messages.push_back("gfx_swapchain.resize ok"
                " width="  + std::to_string(w) +
                " height=" + std::to_string(h));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_swapchain.describe ───────────────────────────────────────────────
    //
    // Reports swapchain state.  Always returns true.
    //
    // "gfx_swapchain.describe width=<W> height=<H> images=<N> hdr=<0|1>"
    harness.registry().registerCommand("gfx_swapchain.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto ext = state->swapchain.extent();
            messages.push_back("gfx_swapchain.describe"
                " width="  + std::to_string(ext.width) +
                " height=" + std::to_string(ext.height) +
                " images=" + std::to_string(state->swapchain.imageCount()) +
                " hdr="    + std::string(state->swapchain.isHdrActive() ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_cmdbuf.alloc ─────────────────────────────────────────────────────
    //
    // Allocates a command buffer handle from the null device.
    //
    // Arguments:
    //   queue=Graphics|Compute   (default Graphics)
    //
    // On success: "gfx_cmdbuf.alloc ok valid=<0|1> queue=<Q>"
    harness.registry().registerCommand("gfx_cmdbuf.alloc",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto queueStr = gdGetArg(cmd, "queue");
            nexus::gfx::QueueType queue = nexus::gfx::QueueType::Graphics;
            std::string queueName = "Graphics";
            if (queueStr && *queueStr == "Compute") {
                queue     = nexus::gfx::QueueType::Compute;
                queueName = "Compute";
            }

            const auto h = state->device.allocateCommandBuffer(queue);

            messages.push_back("gfx_cmdbuf.alloc ok"
                " valid=" + std::string(h.valid() ? "1" : "0") +
                " queue=" + queueName);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_cmdbuf.describe ──────────────────────────────────────────────────
    //
    // Reports swapchain acquire/present counts.  Always returns true.
    //
    // "gfx_cmdbuf.describe acquires=<N> presents=<N>"
    harness.registry().registerCommand("gfx_cmdbuf.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("gfx_cmdbuf.describe"
                " acquires=" + std::to_string(state->acquireCount) +
                " presents=" + std::to_string(state->presentCount));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
