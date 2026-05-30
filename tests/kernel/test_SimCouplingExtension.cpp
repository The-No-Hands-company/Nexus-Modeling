// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation sim_coupling.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/SimCouplingExtension.h>
#include <nexus/sim/SimulationCore.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerSimCouplingCommands(h);
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

static bool runCtx(ScriptBatchHarness& h, const std::string& name,
                   std::initializer_list<std::pair<std::string,std::string>> args,
                   ScriptContext& ctx,
                   std::vector<std::string>& msgs)
{
    ScriptCommand cmd;
    cmd.name = name;
    for (auto& [k, v] : args) cmd.args[k] = v;
    msgs.clear();
    return h.registry().execute(ctx, cmd, msgs);
}

static bool hasMsg(const std::vector<std::string>& msgs, const std::string& sub)
{
    for (const auto& m : msgs)
        if (m.find(sub) != std::string::npos) return true;
    return false;
}

// Build a ScriptContext that has an initialised RigidBodySolver with N bodies
static ScriptContext makeCtxWithSolver(int numBodies = 0)
{
    ScriptContext ctx;
    ctx.rigidSolver = std::make_unique<nexus::RigidBodySolver>();
    ctx.hasRigidSolver = true;
    for (int i = 0; i < numBodies; ++i) {
        nexus::SimBodyDesc desc;
        desc.mass     = 1.f;
        desc.position = {static_cast<float>(i), 0.f, 0.f};
        ctx.rigidSolver->addBody(desc);
    }
    return ctx;
}

// ── sim_coupling.register ─────────────────────────────────────────────────────

TEST(SimCouplingExtension, RegisterInvalidBodyFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    // body=0 is kInvalidBodyId
    EXPECT_FALSE(run(h, "sim_coupling.register", {{"body","0"},{"node","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SimCouplingExtension, RegisterInvalidNodeFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sim_coupling.register", {{"body","1"},{"node","0"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SimCouplingExtension, RegisterOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "sim_coupling.register",
        {{"body","1"},{"node","100"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sim_coupling.register ok"));
    EXPECT_TRUE(hasMsg(msgs, "count=1"));
}

TEST(SimCouplingExtension, RegisterMultiple)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sim_coupling.register", {{"body","1"},{"node","10"}}, msgs);
    ok = run(h, "sim_coupling.register", {{"body","2"},{"node","20"}}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.register",
        {{"body","3"},{"node","30"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "count=3"));
}

TEST(SimCouplingExtension, RegisterDuplicateBodyFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sim_coupling.register",
        {{"body","1"},{"node","10"}}, msgs);
    EXPECT_FALSE(run(h, "sim_coupling.register",
        {{"body","1"},{"node","99"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

// ── sim_coupling.unregister ───────────────────────────────────────────────────

TEST(SimCouplingExtension, UnregisterNotFoundFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sim_coupling.unregister", {{"body","5"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SimCouplingExtension, UnregisterOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sim_coupling.register",
        {{"body","1"},{"node","10"}}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.unregister", {{"body","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "count=0"));
}

// ── sim_coupling.sync ─────────────────────────────────────────────────────────

TEST(SimCouplingExtension, SyncRequiresSolver)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    ScriptContext ctx;  // no solver
    EXPECT_FALSE(runCtx(h, "sim_coupling.sync", {}, ctx, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SimCouplingExtension, SyncNoRegistrations)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    ScriptContext ctx = makeCtxWithSolver(2);
    EXPECT_TRUE(runCtx(h, "sim_coupling.sync", {}, ctx, msgs));
    EXPECT_TRUE(hasMsg(msgs, "synced=0"));
}

TEST(SimCouplingExtension, SyncWithRegistration)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;

    // Set up solver with 2 bodies
    ScriptContext ctx = makeCtxWithSolver(2);

    // The solver assigns BodyIds starting at 1
    [[maybe_unused]] bool ok = run(h, "sim_coupling.register",
        {{"body","1"},{"node","100"}}, msgs);

    // Sync
    EXPECT_TRUE(runCtx(h, "sim_coupling.sync", {}, ctx, msgs));
    EXPECT_TRUE(hasMsg(msgs, "synced=1"));
}

// ── sim_coupling.query ────────────────────────────────────────────────────────

TEST(SimCouplingExtension, QueryNoSnapshot)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "sim_coupling.query", {{"body","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(SimCouplingExtension, QueryAfterSync)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;

    ScriptContext ctx = makeCtxWithSolver(1);
    [[maybe_unused]] bool ok;
    ok = run(h, "sim_coupling.register", {{"body","1"},{"node","10"}}, msgs);
    ok = runCtx(h, "sim_coupling.sync", {}, ctx, msgs);

    EXPECT_TRUE(run(h, "sim_coupling.query", {{"body","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sim_coupling.query ok"));
    EXPECT_TRUE(hasMsg(msgs, "px="));
    EXPECT_TRUE(hasMsg(msgs, "py="));
    EXPECT_TRUE(hasMsg(msgs, "pz="));
}

// ── sim_coupling.clear ────────────────────────────────────────────────────────

TEST(SimCouplingExtension, ClearOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sim_coupling.register", {{"body","1"},{"node","10"}}, msgs);
    ok = run(h, "sim_coupling.register", {{"body","2"},{"node","20"}}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "sim_coupling.clear ok"));
}

TEST(SimCouplingExtension, ClearResetsCount)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "sim_coupling.register", {{"body","1"},{"node","10"}}, msgs);
    ok = run(h, "sim_coupling.clear",    {}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=0"));
}

// ── sim_coupling.describe ─────────────────────────────────────────────────────

TEST(SimCouplingExtension, DescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=0"));
    EXPECT_TRUE(hasMsg(msgs, "synced=0"));
}

TEST(SimCouplingExtension, DescribeAfterRegister)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "sim_coupling.register",
        {{"body","1"},{"node","5"}}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=1"));
}

TEST(SimCouplingExtension, DescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── Integration: register bodies, step solver, sync, query ───────────────────

TEST(SimCouplingExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Create solver with 2 bodies at different positions
    ScriptContext ctx;
    ctx.rigidSolver = std::make_unique<nexus::RigidBodySolver>();
    ctx.hasRigidSolver = true;

    nexus::SimBodyDesc d1;
    d1.mass = 1.f; d1.position = {0.f, 10.f, 0.f};
    nexus::SimBodyDesc d2;
    d2.mass = 2.f; d2.position = {5.f, 0.f, 0.f};

    auto id1 = ctx.rigidSolver->addBody(d1);
    auto id2 = ctx.rigidSolver->addBody(d2);

    // Register both bodies
    ok = run(h, "sim_coupling.register",
        {{"body", std::to_string(id1)}, {"node","100"}}, msgs);
    ok = run(h, "sim_coupling.register",
        {{"body", std::to_string(id2)}, {"node","200"}}, msgs);

    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=2"));

    // Step the solver a few frames
    for (int i = 0; i < 5; ++i)
        ctx.rigidSolver->step(1.0 / 60.0);

    // Sync from solver
    EXPECT_TRUE(runCtx(h, "sim_coupling.sync", {}, ctx, msgs));
    EXPECT_TRUE(hasMsg(msgs, "synced=2"));

    // Query body 1 — should have valid snapshot
    EXPECT_TRUE(run(h, "sim_coupling.query",
        {{"body", std::to_string(id1)}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "px="));

    // Query body 2
    EXPECT_TRUE(run(h, "sim_coupling.query",
        {{"body", std::to_string(id2)}}, msgs));

    // Unregister body 1
    ok = run(h, "sim_coupling.unregister",
        {{"body", std::to_string(id1)}}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=1"));

    // Clear all
    ok = run(h, "sim_coupling.clear", {}, msgs);
    EXPECT_TRUE(run(h, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=0"));
}

TEST(SimCouplingExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "sim_coupling.register",
        {{"body","1"},{"node","10"}}, msgs);

    EXPECT_TRUE(run(h2, "sim_coupling.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "registered=0"));
}
