// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Simulation extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SimExtension.h>
#include <nexus/sim/SimulationCore.h>
#include <nexus/sim/ClothSolver.h>
#include <nexus/sim/FluidSolver.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
simGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = simGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static double doubleArg(const ScriptCommand& cmd,
                                      std::string_view key, double def)
{
    if (const auto v = simGetArg(cmd, key)) {
        double out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = simGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerSimCommands(ScriptBatchHarness& harness)
{
    // ═════════════════════════════════════════════════════════════════════════
    //  RIGID BODY
    // ═════════════════════════════════════════════════════════════════════════

    // ── sim.rigid_init ────────────────────────────────────────────────────────
    //
    // Creates (or resets) ctx.rigidSolver.
    //
    // Arguments:
    //   gx=<f> gy=<f> gz=<f>   gravity vector (default 0 -9.81 0)
    //
    // On success: "sim.rigid_init ok gravity=(<gx>,<gy>,<gz>)"
    harness.registry().registerCommand("sim.rigid_init",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const float gx = floatArg(cmd, "gx", 0.f);
            const float gy = floatArg(cmd, "gy", -9.81f);
            const float gz = floatArg(cmd, "gz", 0.f);

            ctx.rigidSolver = std::make_unique<nexus::RigidBodySolver>();
            ctx.rigidSolver->setGravity({gx, gy, gz});
            ctx.hasRigidSolver = true;

            messages.push_back("sim.rigid_init ok"
                " gravity=(" + std::to_string(gx) + "," +
                               std::to_string(gy) + "," +
                               std::to_string(gz) + ")");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.rigid_add_body ────────────────────────────────────────────────────
    //
    // Adds a body to ctx.rigidSolver.
    //
    // Arguments:
    //   mass=<f>                   (default 1.0; 0 = static)
    //   px=<f> py=<f> pz=<f>      position (default 0 0 0)
    //   vx=<f> vy=<f> vz=<f>      velocity (default 0 0 0)
    //
    // On success: "sim.rigid_add_body ok id=<I> mass=<M>"
    // On error:   "sim.rigid_add_body error: solver not initialized"
    harness.registry().registerCommand("sim.rigid_add_body",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidSolver || !ctx.rigidSolver) {
                messages.push_back("sim.rigid_add_body error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::SimBodyDesc desc;
            desc.mass     = floatArg(cmd, "mass", 1.f);
            desc.position = {floatArg(cmd, "px", 0.f),
                             floatArg(cmd, "py", 0.f),
                             floatArg(cmd, "pz", 0.f)};
            desc.velocity = {floatArg(cmd, "vx", 0.f),
                             floatArg(cmd, "vy", 0.f),
                             floatArg(cmd, "vz", 0.f)};

            const nexus::BodyId id = ctx.rigidSolver->addBody(desc);

            messages.push_back("sim.rigid_add_body ok"
                " id="   + std::to_string(id) +
                " mass=" + std::to_string(desc.mass));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.rigid_apply_force ─────────────────────────────────────────────────
    //
    // Accumulates a force on a body (cleared after next step).
    //
    // Arguments:
    //   id=<N>                 body id
    //   fx=<f> fy=<f> fz=<f>  force vector (default 0 0 0)
    //
    // On success: "sim.rigid_apply_force ok id=<I>"
    // On error:   "sim.rigid_apply_force error: ..."
    harness.registry().registerCommand("sim.rigid_apply_force",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidSolver || !ctx.rigidSolver) {
                messages.push_back("sim.rigid_apply_force error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const nexus::BodyId id = uintArg(cmd, "id", nexus::kInvalidBodyId);
            if (id == nexus::kInvalidBodyId || !ctx.rigidSolver->hasBody(id)) {
                messages.push_back("sim.rigid_apply_force error: body id=" +
                    std::to_string(id) + " not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const nexus::SimVec3 force = {floatArg(cmd, "fx", 0.f),
                                          floatArg(cmd, "fy", 0.f),
                                          floatArg(cmd, "fz", 0.f)};
            if (!ctx.rigidSolver->applyForce(id, force)) {
                messages.push_back("sim.rigid_apply_force error: body is static id=" +
                    std::to_string(id));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim.rigid_apply_force ok id=" + std::to_string(id));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.rigid_step ────────────────────────────────────────────────────────
    //
    // Advances ctx.rigidSolver by dt seconds.
    //
    // Arguments:
    //   dt=<f>   time delta in seconds (default 0.016)
    //
    // On success: "sim.rigid_step ok time=<T> integrated=<N>"
    // On error:   "sim.rigid_step error: ..."
    harness.registry().registerCommand("sim.rigid_step",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidSolver || !ctx.rigidSolver) {
                messages.push_back("sim.rigid_step error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const double dt = doubleArg(cmd, "dt", 1.0 / 60.0);
            const nexus::StepReport report = ctx.rigidSolver->step(dt);

            if (!report.ok) {
                messages.push_back("sim.rigid_step error: invalid dt");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim.rigid_step ok"
                " time="       + std::to_string(report.simulationTime) +
                " integrated=" + std::to_string(report.bodiesIntegrated));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.rigid_snapshot ────────────────────────────────────────────────────
    //
    // Saves ctx.rigidSolver state into ctx.rigidLastState for later diff.
    //
    // On success: "sim.rigid_snapshot ok bodies=<N> time=<T>"
    // On error:   "sim.rigid_snapshot error: solver not initialized"
    harness.registry().registerCommand("sim.rigid_snapshot",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidSolver || !ctx.rigidSolver) {
                messages.push_back("sim.rigid_snapshot error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.rigidLastState   = ctx.rigidSolver->captureState();
            ctx.hasRigidLastState = true;

            messages.push_back("sim.rigid_snapshot ok"
                " bodies=" + std::to_string(ctx.rigidLastState.bodies.size()) +
                " time="   + std::to_string(ctx.rigidLastState.simulationTime));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.rigid_diff ────────────────────────────────────────────────────────
    //
    // Compares current rigid solver time against the last snapshot.
    // Returns false when no snapshot exists.
    //
    // On success: "sim.rigid_diff ok snapshot_time=<S> current_time=<C>"
    // On error:   "sim.rigid_diff error: no snapshot"
    harness.registry().registerCommand("sim.rigid_diff",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidLastState) {
                messages.push_back("sim.rigid_diff error: no snapshot");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.hasRigidSolver || !ctx.rigidSolver) {
                messages.push_back("sim.rigid_diff error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim.rigid_diff ok"
                " snapshot_time=" + std::to_string(ctx.rigidLastState.simulationTime) +
                " current_time="  + std::to_string(ctx.rigidSolver->simulationTime()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.rigid_describe ────────────────────────────────────────────────────
    //
    // Reports current rigid solver state: body count and simulation time.
    // Always returns true.
    harness.registry().registerCommand("sim.rigid_describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasRigidSolver || !ctx.rigidSolver) {
                messages.push_back("sim.rigid_describe bodies=0 time=0");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            messages.push_back("sim.rigid_describe"
                " bodies=" + std::to_string(ctx.rigidSolver->bodyCount()) +
                " time="   + std::to_string(ctx.rigidSolver->simulationTime()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ═════════════════════════════════════════════════════════════════════════
    //  CLOTH
    // ═════════════════════════════════════════════════════════════════════════

    // ── sim.cloth_init ────────────────────────────────────────────────────────
    //
    // Creates (or resets) ctx.clothSolver.
    //
    // On success: "sim.cloth_init ok"
    harness.registry().registerCommand("sim.cloth_init",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const float gx = floatArg(cmd, "gx", 0.f);
            const float gy = floatArg(cmd, "gy", -9.81f);
            const float gz = floatArg(cmd, "gz", 0.f);

            ctx.clothSolver = std::make_unique<nexus::ClothSolver>();
            ctx.clothSolver->setGravity({gx, gy, gz});
            ctx.hasClothSolver = true;

            messages.push_back("sim.cloth_init ok"
                " gravity=(" + std::to_string(gx) + "," +
                               std::to_string(gy) + "," +
                               std::to_string(gz) + ")");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.cloth_add_node ────────────────────────────────────────────────────
    //
    // Adds a cloth particle node.
    //
    // Arguments:
    //   mass=<f>                   (default 1.0; 0 = pinned)
    //   px=<f> py=<f> pz=<f>      position
    //
    // On success: "sim.cloth_add_node ok id=<I> mass=<M>"
    // On error:   "sim.cloth_add_node error: solver not initialized"
    harness.registry().registerCommand("sim.cloth_add_node",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasClothSolver || !ctx.clothSolver) {
                messages.push_back("sim.cloth_add_node error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::ClothNodeDesc desc;
            desc.mass     = floatArg(cmd, "mass", 1.f);
            desc.position = {floatArg(cmd, "px", 0.f),
                             floatArg(cmd, "py", 0.f),
                             floatArg(cmd, "pz", 0.f)};

            const nexus::ClothNodeId id = ctx.clothSolver->addNode(desc);

            messages.push_back("sim.cloth_add_node ok"
                " id="   + std::to_string(id) +
                " mass=" + std::to_string(desc.mass));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.cloth_step ────────────────────────────────────────────────────────
    //
    // Advances ctx.clothSolver by dt seconds.
    //
    // Arguments:
    //   dt=<f>   time delta in seconds (default 0.016)
    //
    // On success: "sim.cloth_step ok time=<T> integrated=<N>"
    // On error:   "sim.cloth_step error: ..."
    harness.registry().registerCommand("sim.cloth_step",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasClothSolver || !ctx.clothSolver) {
                messages.push_back("sim.cloth_step error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const double dt = doubleArg(cmd, "dt", 1.0 / 60.0);
            const nexus::ClothStepReport report = ctx.clothSolver->step(dt);

            if (!report.ok) {
                messages.push_back("sim.cloth_step error: invalid dt");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim.cloth_step ok"
                " time="       + std::to_string(report.simulationTime) +
                " integrated=" + std::to_string(report.nodesIntegrated));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.cloth_describe ────────────────────────────────────────────────────
    //
    // Reports cloth solver state. Always returns true.
    harness.registry().registerCommand("sim.cloth_describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasClothSolver || !ctx.clothSolver) {
                messages.push_back("sim.cloth_describe nodes=0 time=0");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            const nexus::ClothState state = ctx.clothSolver->captureState();
            messages.push_back("sim.cloth_describe"
                " nodes=" + std::to_string(ctx.clothSolver->nodeCount()) +
                " time="  + std::to_string(state.simulationTime));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ═════════════════════════════════════════════════════════════════════════
    //  FLUID
    // ═════════════════════════════════════════════════════════════════════════

    // ── sim.fluid_init ────────────────────────────────────────────────────────
    //
    // Creates (or resets) ctx.fluidSolver.
    //
    // On success: "sim.fluid_init ok"
    harness.registry().registerCommand("sim.fluid_init",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const float gx = floatArg(cmd, "gx", 0.f);
            const float gy = floatArg(cmd, "gy", -9.81f);
            const float gz = floatArg(cmd, "gz", 0.f);

            ctx.fluidSolver = std::make_unique<nexus::FluidSolver>();
            ctx.fluidSolver->setGravity({gx, gy, gz});
            ctx.hasFluidSolver = true;

            messages.push_back("sim.fluid_init ok"
                " gravity=(" + std::to_string(gx) + "," +
                               std::to_string(gy) + "," +
                               std::to_string(gz) + ")");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.fluid_add_particle ────────────────────────────────────────────────
    //
    // Adds a fluid particle.
    //
    // Arguments:
    //   mass=<f>                   (default 1.0; 0 = static boundary)
    //   px=<f> py=<f> pz=<f>      position
    //   density=<f>                rest density kg/m³ (default 1000)
    //
    // On success: "sim.fluid_add_particle ok id=<I> mass=<M>"
    // On error:   "sim.fluid_add_particle error: solver not initialized"
    harness.registry().registerCommand("sim.fluid_add_particle",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasFluidSolver || !ctx.fluidSolver) {
                messages.push_back("sim.fluid_add_particle error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::FluidParticleDesc desc;
            desc.mass     = floatArg(cmd, "mass", 1.f);
            desc.position = {floatArg(cmd, "px", 0.f),
                             floatArg(cmd, "py", 0.f),
                             floatArg(cmd, "pz", 0.f)};
            desc.density  = floatArg(cmd, "density", 1000.f);

            const nexus::FluidParticleId id = ctx.fluidSolver->addParticle(desc);

            messages.push_back("sim.fluid_add_particle ok"
                " id="   + std::to_string(id) +
                " mass=" + std::to_string(desc.mass));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.fluid_step ────────────────────────────────────────────────────────
    //
    // Advances ctx.fluidSolver by dt seconds.
    //
    // Arguments:
    //   dt=<f>   time delta in seconds (default 0.016)
    //
    // On success: "sim.fluid_step ok time=<T> advanced=<N>"
    // On error:   "sim.fluid_step error: ..."
    harness.registry().registerCommand("sim.fluid_step",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasFluidSolver || !ctx.fluidSolver) {
                messages.push_back("sim.fluid_step error: solver not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const double dt = doubleArg(cmd, "dt", 1.0 / 60.0);
            const nexus::FluidStepReport report = ctx.fluidSolver->step(dt);

            if (!report.ok) {
                messages.push_back("sim.fluid_step error: invalid dt");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("sim.fluid_step ok"
                " time="     + std::to_string(report.simulationTime) +
                " advanced=" + std::to_string(report.particlesAdvanced));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sim.fluid_describe ────────────────────────────────────────────────────
    //
    // Reports fluid solver state. Always returns true.
    harness.registry().registerCommand("sim.fluid_describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasFluidSolver || !ctx.fluidSolver) {
                messages.push_back("sim.fluid_describe particles=0 time=0");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            const nexus::FluidState state = ctx.fluidSolver->captureState();
            messages.push_back("sim.fluid_describe"
                " particles=" + std::to_string(ctx.fluidSolver->particleCount()) +
                " time="      + std::to_string(state.simulationTime));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
