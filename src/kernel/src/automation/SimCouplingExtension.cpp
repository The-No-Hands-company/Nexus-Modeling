// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SimCouplingHarness extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SimCouplingExtension.h>
#include <nexus/sim/SimCouplingHarness.h>
#include <nexus/sim/SimulationCore.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
scGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = scGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct SimCouplingState {
    nexus::sim::SimCouplingHarness harness;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerSimCouplingCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<SimCouplingState>();

    // ── sim_coupling.register ─────────────────────────────────────────────────
    //
    // Registers a RigidBody → SceneNode pair in the coupling harness.
    //
    // Arguments:
    //   body=<N>   BodyId (required; must be non-zero)
    //   node=<N>   SceneNodeId (required; must be non-zero)
    //
    // On success: "sim_coupling.register ok body=<B> node=<N> count=<C>"
    // On error:   "sim_coupling.register error: ..."
    harness.registry().registerCommand("sim_coupling.register",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t bodyId = uintArg(cmd, "body", 0u);
            const uint32_t nodeId = uintArg(cmd, "node", 0u);

            if (bodyId == nexus::kInvalidBodyId) {
                messages.push_back("sim_coupling.register error: invalid body id (0)");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (nodeId == nexus::kInvalidSceneNodeId) {
                messages.push_back("sim_coupling.register error: invalid node id (0)");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const bool ok = state->harness.registerBody(
                static_cast<nexus::BodyId>(bodyId),
                static_cast<nexus::SceneNodeId>(nodeId));

            if (!ok) {
                messages.push_back("sim_coupling.register error:"
                    " body or node already registered");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim_coupling.register ok"
                " body="  + std::to_string(bodyId) +
                " node="  + std::to_string(nodeId) +
                " count=" + std::to_string(state->harness.registrationCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim_coupling.unregister ───────────────────────────────────────────────
    //
    // Removes a body registration from the coupling harness.
    //
    // Arguments:
    //   body=<N>   BodyId (required)
    //
    // On success: "sim_coupling.unregister ok body=<B> count=<C>"
    // On error:   "sim_coupling.unregister error: ..."
    harness.registry().registerCommand("sim_coupling.unregister",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t bodyId = uintArg(cmd, "body", 0u);

            const bool ok = state->harness.unregisterBody(
                static_cast<nexus::BodyId>(bodyId));

            if (!ok) {
                messages.push_back("sim_coupling.unregister error: body not registered");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim_coupling.unregister ok"
                " body="  + std::to_string(bodyId) +
                " count=" + std::to_string(state->harness.registrationCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim_coupling.sync ─────────────────────────────────────────────────────
    //
    // Calls syncFromSolver on ctx.rigidSolver to snapshot body states.
    // Returns false when ctx.hasRigidSolver is false.
    //
    // On success: "sim_coupling.sync ok synced=<N>"
    // On error:   "sim_coupling.sync error: ..."
    harness.registry().registerCommand("sim_coupling.sync",
        [state](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidSolver) {
                messages.push_back("sim_coupling.sync error: no rigid solver in context");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const size_t synced =
                state->harness.syncFromSolver(*ctx.rigidSolver);

            messages.push_back("sim_coupling.sync ok"
                " synced=" + std::to_string(synced));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim_coupling.query ────────────────────────────────────────────────────
    //
    // Queries the last synced position/velocity for a registered body.
    // Returns false when the body has no snapshot.
    //
    // Arguments:
    //   body=<N>   BodyId (required)
    //
    // On success: "sim_coupling.query ok body=<B> px=<X> py=<Y> pz=<Z> vx=<X> vy=<Y> vz=<Z>"
    // On error:   "sim_coupling.query error: ..."
    harness.registry().registerCommand("sim_coupling.query",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const uint32_t bodyId = uintArg(cmd, "body", 0u);

            const auto snap = state->harness.lastState(
                static_cast<nexus::BodyId>(bodyId));

            if (!snap) {
                messages.push_back("sim_coupling.query error: no snapshot for body " +
                    std::to_string(bodyId));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim_coupling.query ok"
                " body=" + std::to_string(bodyId) +
                " px=" + std::to_string(snap->position.x) +
                " py=" + std::to_string(snap->position.y) +
                " pz=" + std::to_string(snap->position.z) +
                " vx=" + std::to_string(snap->velocity.x) +
                " vy=" + std::to_string(snap->velocity.y) +
                " vz=" + std::to_string(snap->velocity.z));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim_coupling.clear ────────────────────────────────────────────────────
    //
    // Clears all registrations and snapshots from the harness.
    //
    // On success: "sim_coupling.clear ok"
    harness.registry().registerCommand("sim_coupling.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->harness.clear();
            messages.push_back("sim_coupling.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim_coupling.describe ─────────────────────────────────────────────────
    //
    // Reports registration and sync counts.  Always returns true.
    //
    // "sim_coupling.describe registered=<R> synced=<S>"
    harness.registry().registerCommand("sim_coupling.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("sim_coupling.describe"
                " registered=" + std::to_string(state->harness.registrationCount()) +
                " synced="     + std::to_string(state->harness.syncedCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
