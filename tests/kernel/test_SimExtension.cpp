// ─────────────────────────────────────────────────────────────────────────────
//  Tests for SimExtension — sim.rigid_*, sim.cloth_*, sim.fluid_*
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SimExtension.h>
#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerSimCommands(h);
    return h;
}

nexus::automation::ScriptCommand makeCmd(
    std::string name,
    std::vector<std::pair<std::string,std::string>> args = {})
{
    nexus::automation::ScriptCommand cmd;
    cmd.name = std::move(name);
    for (auto& [k, v] : args)
        cmd.args[k] = v;
    return cmd;
}

bool hasPrefix(const std::vector<std::string>& msgs, const std::string& prefix)
{
    return std::any_of(msgs.begin(), msgs.end(),
        [&](const std::string& m){ return m.rfind(prefix, 0) == 0; });
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  sim.rigid_init
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, RigidInitOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_init ok"));
    EXPECT_TRUE(ctx.hasRigidSolver);
    EXPECT_NE(ctx.rigidSolver, nullptr);
}

TEST(SimExtension, RigidInitCustomGravity)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_init", {{"gx","0"},{"gy","-1"},{"gz","0"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_init ok gravity="));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.rigid_add_body
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, RigidAddBodyNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_add_body error:"));
}

TEST(SimExtension, RigidAddBodyReturnsId)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","2"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_add_body ok id="));
    EXPECT_EQ(ctx.rigidSolver->bodyCount(), 1u);
}

TEST(SimExtension, RigidAddStaticBody)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","0"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_add_body ok id="));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.rigid_apply_force
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, RigidApplyForceNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_apply_force", {{"id","1"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_apply_force error:"));
}

TEST(SimExtension, RigidApplyForceInvalidIdFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_apply_force", {{"id","99"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_apply_force error:"));
}

TEST(SimExtension, RigidApplyForceSuccess)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    [[maybe_unused]] bool add = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","1"}}), msgs);
    msgs.clear();

    // Body id=1 is the first one added.
    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_apply_force",
            {{"id","1"},{"fx","0"},{"fy","10"},{"fz","0"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_apply_force ok id=1"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.rigid_step
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, RigidStepNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_step"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_step error:"));
}

TEST(SimExtension, RigidStepAdvancesTime)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    [[maybe_unused]] bool add = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","1"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_step", {{"dt","0.1"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_step ok time="));
    EXPECT_GT(ctx.rigidSolver->simulationTime(), 0.0);
}

TEST(SimExtension, RigidStepReportsIntegrated)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","1"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","1"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx, makeCmd("sim.rigid_step", {{"dt","0.016"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_step ok time="));
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_step ok time=") &&
        std::any_of(msgs.begin(), msgs.end(),
            [](const std::string& m){ return m.find("integrated=2") != std::string::npos; }));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.rigid_snapshot / sim.rigid_diff
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, RigidSnapshotNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_snapshot"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_snapshot error:"));
}

TEST(SimExtension, RigidDiffNoSnapshotFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_diff"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_diff error: no snapshot"));
}

TEST(SimExtension, RigidSnapshotThenDiff)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    [[maybe_unused]] bool snap = h.registry().execute(ctx,
        makeCmd("sim.rigid_snapshot"), msgs);
    [[maybe_unused]] bool step = h.registry().execute(ctx,
        makeCmd("sim.rigid_step", {{"dt","0.1"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_diff"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_diff ok snapshot_time="));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.rigid_describe
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, RigidDescribeNoSolver)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_describe bodies=0 time=0"));
}

TEST(SimExtension, RigidDescribeAfterAddBody)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.rigid_init"), msgs);
    [[maybe_unused]] bool add = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.rigid_describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_describe bodies=1"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.cloth_*
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, ClothInitOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_init"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_init ok"));
    EXPECT_TRUE(ctx.hasClothSolver);
}

TEST(SimExtension, ClothAddNodeNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_add_node"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_add_node error:"));
}

TEST(SimExtension, ClothAddNodeReturnsId)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.cloth_init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_add_node", {{"mass","1"},{"py","1"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_add_node ok id="));
    EXPECT_EQ(ctx.clothSolver->nodeCount(), 1u);
}

TEST(SimExtension, ClothStepNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_step"), msgs);
    EXPECT_FALSE(ok);
}

TEST(SimExtension, ClothStepAdvancesTime)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.cloth_init"), msgs);
    [[maybe_unused]] bool add = h.registry().execute(ctx,
        makeCmd("sim.cloth_add_node", {{"mass","1"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_step", {{"dt","0.016"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_step ok time="));
}

TEST(SimExtension, ClothDescribeNoSolver)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_describe nodes=0 time=0"));
}

TEST(SimExtension, ClothDescribeAfterAddNode)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.cloth_init"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("sim.cloth_add_node", {{"mass","1"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("sim.cloth_add_node", {{"mass","0"}}), msgs); // pinned
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.cloth_describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_describe nodes=2"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  sim.fluid_*
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, FluidInitOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_init"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_init ok"));
    EXPECT_TRUE(ctx.hasFluidSolver);
}

TEST(SimExtension, FluidAddParticleNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_add_particle"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_add_particle error:"));
}

TEST(SimExtension, FluidAddParticleReturnsId)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.fluid_init"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_add_particle", {{"mass","1"},{"density","1000"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_add_particle ok id="));
    EXPECT_EQ(ctx.fluidSolver->particleCount(), 1u);
}

TEST(SimExtension, FluidStepNoSolverFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_step"), msgs);
    EXPECT_FALSE(ok);
}

TEST(SimExtension, FluidStepAdvancesTime)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.fluid_init"), msgs);
    [[maybe_unused]] bool add = h.registry().execute(ctx,
        makeCmd("sim.fluid_add_particle", {{"mass","1"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_step", {{"dt","0.016"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_step ok time="));
}

TEST(SimExtension, FluidDescribeNoSolver)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_describe particles=0 time=0"));
}

TEST(SimExtension, FluidDescribeAfterAddParticle)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool init = h.registry().execute(ctx,
        makeCmd("sim.fluid_init"), msgs);
    for (int i = 0; i < 3; ++i) {
        [[maybe_unused]] bool a = h.registry().execute(ctx,
            makeCmd("sim.fluid_add_particle", {{"mass","1"}}), msgs);
    }
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("sim.fluid_describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_describe particles=3"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full pipeline: multi-solver tick + snapshot/diff
// ─────────────────────────────────────────────────────────────────────────────

TEST(SimExtension, MultiSolverPipeline)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Spin up all three solvers.
    [[maybe_unused]] bool ri = h.registry().execute(ctx, makeCmd("sim.rigid_init"), msgs);
    [[maybe_unused]] bool ci = h.registry().execute(ctx, makeCmd("sim.cloth_init"), msgs);
    [[maybe_unused]] bool fi = h.registry().execute(ctx, makeCmd("sim.fluid_init"), msgs);

    EXPECT_TRUE(ctx.hasRigidSolver);
    EXPECT_TRUE(ctx.hasClothSolver);
    EXPECT_TRUE(ctx.hasFluidSolver);

    // Add one body / node / particle each.
    [[maybe_unused]] bool rb = h.registry().execute(ctx,
        makeCmd("sim.rigid_add_body", {{"mass","1"},{"py","10"}}), msgs);
    [[maybe_unused]] bool cn = h.registry().execute(ctx,
        makeCmd("sim.cloth_add_node", {{"mass","1"},{"py","5"}}), msgs);
    [[maybe_unused]] bool fp = h.registry().execute(ctx,
        makeCmd("sim.fluid_add_particle", {{"mass","1"},{"py","2"}}), msgs);

    // Snapshot the rigid state before stepping.
    [[maybe_unused]] bool sn = h.registry().execute(ctx,
        makeCmd("sim.rigid_snapshot"), msgs);
    EXPECT_TRUE(ctx.hasRigidLastState);

    // Step all three for 4 frames.
    for (int i = 0; i < 4; ++i) {
        [[maybe_unused]] bool rs = h.registry().execute(ctx,
            makeCmd("sim.rigid_step", {{"dt","0.016"}}), msgs);
        [[maybe_unused]] bool cs = h.registry().execute(ctx,
            makeCmd("sim.cloth_step", {{"dt","0.016"}}), msgs);
        [[maybe_unused]] bool fs = h.registry().execute(ctx,
            makeCmd("sim.fluid_step", {{"dt","0.016"}}), msgs);
    }

    msgs.clear();

    // Diff should show time has advanced.
    {
        bool ok = h.registry().execute(ctx, makeCmd("sim.rigid_diff"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_diff ok snapshot_time=0"));
    }

    msgs.clear();

    // Describe all three.
    {
        [[maybe_unused]] bool d1 = h.registry().execute(ctx,
            makeCmd("sim.rigid_describe"), msgs);
        [[maybe_unused]] bool d2 = h.registry().execute(ctx,
            makeCmd("sim.cloth_describe"), msgs);
        [[maybe_unused]] bool d3 = h.registry().execute(ctx,
            makeCmd("sim.fluid_describe"), msgs);

        EXPECT_TRUE(hasPrefix(msgs, "sim.rigid_describe bodies=1"));
        EXPECT_TRUE(hasPrefix(msgs, "sim.cloth_describe nodes=1"));
        EXPECT_TRUE(hasPrefix(msgs, "sim.fluid_describe particles=1"));
    }
}
