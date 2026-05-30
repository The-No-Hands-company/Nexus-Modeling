// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — RenderPassGraphPlanner extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/GfxPlannerExtension.h>
#include <nexus/gfx/RenderContext.h>

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
gpGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = gpGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct PlannerState {
    nexus::gfx::NullDevice                     device;
    nexus::gfx::SemaphoreHandle                timelineSemaphore;
    nexus::gfx::RenderPassGraphPlanner         planner;
    uint32_t                                   passCount = 0;
    nexus::gfx::RenderPassSubmissionPlan       lastPlan;
    bool                                       hasPlan   = false;

    PlannerState() : timelineSemaphore(device.createSemaphore()) {}
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerGfxPlannerCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<PlannerState>();

    // ── gfx_planner.add_pass ──────────────────────────────────────────────────
    //
    // Adds a render pass to the planner.
    //
    // Arguments:
    //   queue=Graphics|Compute   (default Graphics)
    //
    // On success: "gfx_planner.add_pass ok pass_id=<N> queue=<Q>"
    harness.registry().registerCommand("gfx_planner.add_pass",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto queueStr = gpGetArg(cmd, "queue");
            nexus::gfx::QueueType queue = nexus::gfx::QueueType::Graphics;
            std::string queueName = "Graphics";
            if (queueStr && *queueStr == "Compute") {
                queue     = nexus::gfx::QueueType::Compute;
                queueName = "Compute";
            }

            // addPass requires at least one cmd handle; allocate a dummy one
            const auto dummyCmd = state->device.allocateCommandBuffer(queue);

            nexus::gfx::RenderPassGraphPlanner::PassDesc desc;
            desc.queue = queue;
            desc.cmds  = std::span<const nexus::gfx::CmdBufHandle>(&dummyCmd, 1);
            const auto passId = state->planner.addPass(desc);
            state->passCount++;

            messages.push_back("gfx_planner.add_pass ok"
                " pass_id=" + std::to_string(passId) +
                " queue="   + queueName);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_planner.add_dep ───────────────────────────────────────────────────
    //
    // Declares a dependency edge between two passes.
    //
    // Arguments:
    //   src=<N>   source pass id
    //   dst=<N>   destination pass id
    //
    // On success: "gfx_planner.add_dep ok src=<N> dst=<N>"
    harness.registry().registerCommand("gfx_planner.add_dep",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t src = uintArg(cmd, "src", 0u);
            const uint32_t dst = uintArg(cmd, "dst", 0u);

            state->planner.addDependency(src, dst);

            messages.push_back("gfx_planner.add_dep ok"
                " src=" + std::to_string(src) +
                " dst=" + std::to_string(dst));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_planner.build_plan ────────────────────────────────────────────────
    //
    // Builds a submission plan from declared passes and edges.
    // Returns false when no passes have been added.
    //
    // On success: "gfx_planner.build_plan ok submits=<N> terminals=<N>"
    // On error:   "gfx_planner.build_plan error: no passes"
    harness.registry().registerCommand("gfx_planner.build_plan",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (state->passCount == 0) {
                messages.push_back("gfx_planner.build_plan error: no passes");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->lastPlan = state->planner.buildSubmissionPlan(
                state->timelineSemaphore, 1u,
                nexus::gfx::FencePolicy::None, {});
            state->hasPlan = true;

            messages.push_back("gfx_planner.build_plan ok"
                " submits="   + std::to_string(state->lastPlan.submits.size()) +
                " terminals=" + std::to_string(state->lastPlan.terminalSubmitIndices.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_planner.build_completion ─────────────────────────────────────────
    //
    // Builds a completion-wait descriptor from the last submission plan.
    // Returns false when no plan has been built.
    //
    // On success: "gfx_planner.build_completion ok fences=<N> wait_values=<N>"
    // On error:   "gfx_planner.build_completion error: no plan built"
    harness.registry().registerCommand("gfx_planner.build_completion",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasPlan) {
                messages.push_back("gfx_planner.build_completion error: no plan built");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto waitDesc =
                nexus::gfx::RenderPassGraphPlanner::buildCompletionWaitDesc(
                    state->lastPlan);

            messages.push_back("gfx_planner.build_completion ok"
                " fences="      + std::to_string(waitDesc.fences.size()) +
                " wait_values=" + std::to_string(waitDesc.timelineWaitValues.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_planner.clear ────────────────────────────────────────────────────
    //
    // Clears all passes, edges, and the last plan.
    //
    // On success: "gfx_planner.clear ok"
    harness.registry().registerCommand("gfx_planner.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->planner.clear();
            state->passCount = 0;
            state->hasPlan   = false;

            messages.push_back("gfx_planner.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gfx_planner.describe ─────────────────────────────────────────────────
    //
    // Reports planner state. Always returns true.
    //
    // "gfx_planner.describe passes=<N> has_plan=<0|1>"
    harness.registry().registerCommand("gfx_planner.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("gfx_planner.describe"
                " passes="   + std::to_string(state->passCount) +
                " has_plan=" + std::string(state->hasPlan ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
