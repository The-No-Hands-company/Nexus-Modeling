// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — IFrameScheduler headless extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/FrameSchedulerExtension.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/gfx/Types.h>

#include <backend/null/NullDevice.h>

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
fsGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = fsGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

// Minimal null frame scheduler — runs the Acquire→Record→Submit→Present loop
// without any real GPU work.
class NullFrameScheduler final : public nexus::gfx::IFrameScheduler {
public:
    explicit NullFrameScheduler(uint32_t maxInFlight = 2)
        : m_maxInFlight(maxInFlight)
    {
        m_cmd = m_device.allocateCommandBuffer(nexus::gfx::QueueType::Graphics);
    }

    [[nodiscard]] std::optional<nexus::gfx::FrameContext> beginFrame() override
    {
        if (m_outOfDate) return std::nullopt;

        nexus::gfx::FrameContext fc{};
        fc.cmd        = m_device.getCommandBuffer(m_cmd);
        fc.extent     = m_extent;
        fc.frameSlot  = m_frameCount % m_maxInFlight;
        fc.imageIndex = m_frameCount % 3u;
        m_inFrame     = true;
        return fc;
    }

    void endFrame() override
    {
        m_inFrame = false;
        ++m_frameCount;
    }

    void onResize(nexus::gfx::Extent2D newExtent) override
    {
        m_extent    = newExtent;
        m_outOfDate = false;
    }

    [[nodiscard]] uint32_t maxFramesInFlight() const noexcept override
    {
        return m_maxInFlight;
    }

    void insertGBufferRTBarrier(nexus::gfx::ICommandBuffer&,
                                nexus::gfx::TextureHandle) noexcept override {}

    [[nodiscard]] uint32_t frameCount()   const noexcept { return m_frameCount; }
    [[nodiscard]] bool     isInFrame()    const noexcept { return m_inFrame; }
    [[nodiscard]] nexus::gfx::Extent2D extent() const noexcept { return m_extent; }

    void setOutOfDate() noexcept { m_outOfDate = true; }

private:
    nexus::gfx::NullDevice    m_device;
    nexus::gfx::CmdBufHandle  m_cmd;
    nexus::gfx::Extent2D      m_extent{1280u, 720u};
    uint32_t                  m_maxInFlight;
    uint32_t                  m_frameCount = 0;
    bool                      m_inFrame    = false;
    bool                      m_outOfDate  = false;
};

struct FrameSchedState {
    NullFrameScheduler scheduler;
    bool               lastBeginOk = false;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerFrameSchedulerCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<FrameSchedState>();

    // ── frame_sched.begin ─────────────────────────────────────────────────────
    //
    // Acquires the next frame from the null scheduler.
    // Returns false when the scheduler is out-of-date (needs resize).
    //
    // On success: "frame_sched.begin ok slot=<S> image=<I>"
    // On resize:  "frame_sched.begin out_of_date"  (returns false)
    harness.registry().registerCommand("frame_sched.begin",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto fc = state->scheduler.beginFrame();
            if (!fc) {
                messages.push_back("frame_sched.begin out_of_date");
                std::sort(messages.begin(), messages.end());
                state->lastBeginOk = false;
                return false;
            }
            state->lastBeginOk = true;
            messages.push_back("frame_sched.begin ok"
                " slot="  + std::to_string(fc->frameSlot) +
                " image=" + std::to_string(fc->imageIndex));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── frame_sched.end ───────────────────────────────────────────────────────
    //
    // Submits and presents the current frame.
    // Returns false when beginFrame was not successfully called first.
    //
    // On success: "frame_sched.end ok frames=<N>"
    // On error:   "frame_sched.end error: no active frame"
    harness.registry().registerCommand("frame_sched.end",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->lastBeginOk) {
                messages.push_back("frame_sched.end error: no active frame");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            state->scheduler.endFrame();
            state->lastBeginOk = false;
            messages.push_back("frame_sched.end ok"
                " frames=" + std::to_string(state->scheduler.frameCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── frame_sched.resize ────────────────────────────────────────────────────
    //
    // Notifies the scheduler of a swapchain resize.
    //
    // Arguments:
    //   width=<N>   new width  (default 1280)
    //   height=<N>  new height (default 720)
    //
    // On success: "frame_sched.resize ok width=<W> height=<H>"
    harness.registry().registerCommand("frame_sched.resize",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t w = uintArg(cmd, "width",  1280u);
            const uint32_t h = uintArg(cmd, "height",  720u);
            state->scheduler.onResize({w, h});
            messages.push_back("frame_sched.resize ok"
                " width="  + std::to_string(w) +
                " height=" + std::to_string(h));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── frame_sched.set_out_of_date ───────────────────────────────────────────
    //
    // Forces the scheduler into out-of-date state so the next beginFrame
    // returns nullopt (simulates a window resize event).
    //
    // On success: "frame_sched.set_out_of_date ok"
    harness.registry().registerCommand("frame_sched.set_out_of_date",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->scheduler.setOutOfDate();
            messages.push_back("frame_sched.set_out_of_date ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── frame_sched.describe ─────────────────────────────────────────────────
    //
    // Reports scheduler state.  Always returns true.
    //
    // "frame_sched.describe max_in_flight=<N> frames=<F> width=<W> height=<H>"
    harness.registry().registerCommand("frame_sched.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto ext = state->scheduler.extent();
            messages.push_back("frame_sched.describe"
                " max_in_flight=" + std::to_string(state->scheduler.maxFramesInFlight()) +
                " frames="        + std::to_string(state->scheduler.frameCount()) +
                " width="         + std::to_string(ext.width) +
                " height="        + std::to_string(ext.height));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
