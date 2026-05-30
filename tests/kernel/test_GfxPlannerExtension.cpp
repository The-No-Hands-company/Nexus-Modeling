// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation gfx_planner.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/GfxPlannerExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerGfxPlannerCommands(h);
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

// ── gfx_planner.add_pass ─────────────────────────────────────────────────────

TEST(GfxPlannerExtension, AddPassGraphics)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.add_pass ok"));
    EXPECT_TRUE(hasMsg(msgs, "pass_id=0"));
    EXPECT_TRUE(hasMsg(msgs, "queue=Graphics"));
}

TEST(GfxPlannerExtension, AddPassCompute)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_planner.add_pass", {{"queue","Compute"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "queue=Compute"));
}

TEST(GfxPlannerExtension, AddPassDefaultIsGraphics)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_planner.add_pass", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "queue=Graphics"));
}

TEST(GfxPlannerExtension, AddPassIdsIncrement)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.add_pass", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "pass_id=1"));
}

TEST(GfxPlannerExtension, AddPassMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_planner.add_pass", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── gfx_planner.add_dep ──────────────────────────────────────────────────────

TEST(GfxPlannerExtension, AddDepOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {{"queue","Compute"}},  msgs);
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.add_dep", {{"src","0"},{"dst","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.add_dep ok"));
    EXPECT_TRUE(hasMsg(msgs, "src=0"));
    EXPECT_TRUE(hasMsg(msgs, "dst=1"));
}

// ── gfx_planner.build_plan ───────────────────────────────────────────────────

TEST(GfxPlannerExtension, BuildPlanRequiresPasses)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "gfx_planner.build_plan", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(GfxPlannerExtension, BuildPlanSinglePass)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.build_plan", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.build_plan ok"));
}

TEST(GfxPlannerExtension, BuildPlanWithDep)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {{"queue","Compute"}},  msgs);
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    ok = run(h, "gfx_planner.add_dep",  {{"src","0"},{"dst","1"}}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.build_plan", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.build_plan ok"));
}

// ── gfx_planner.build_completion ─────────────────────────────────────────────

TEST(GfxPlannerExtension, BuildCompletionRequiresPlan)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "gfx_planner.build_completion", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(GfxPlannerExtension, BuildCompletionAfterBuildPlan)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    ok = run(h, "gfx_planner.build_plan", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.build_completion", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.build_completion ok"));
}

// ── gfx_planner.clear ────────────────────────────────────────────────────────

TEST(GfxPlannerExtension, ClearOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "gfx_planner.add_pass", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.clear ok"));
}

TEST(GfxPlannerExtension, ClearResetsPasses)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {}, msgs);
    ok = run(h, "gfx_planner.add_pass", {}, msgs);
    ok = run(h, "gfx_planner.clear",    {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
}

TEST(GfxPlannerExtension, ClearResetsPlan)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    ok = run(h, "gfx_planner.build_plan", {}, msgs);
    ok = run(h, "gfx_planner.clear",      {}, msgs);
    EXPECT_FALSE(run(h, "gfx_planner.build_completion", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

// ── gfx_planner.describe ─────────────────────────────────────────────────────

TEST(GfxPlannerExtension, DescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
    EXPECT_TRUE(hasMsg(msgs, "has_plan=0"));
}

TEST(GfxPlannerExtension, DescribeAfterAddPass)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "gfx_planner.add_pass", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=1"));
}

TEST(GfxPlannerExtension, DescribeAfterBuildPlan)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    ok = run(h, "gfx_planner.build_plan", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "has_plan=1"));
}

TEST(GfxPlannerExtension, DescribeMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(GfxPlannerExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // 3 passes: compute → graphics → graphics
    ok = run(h, "gfx_planner.add_pass", {{"queue","Compute"}},  msgs);
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);
    ok = run(h, "gfx_planner.add_pass", {{"queue","Graphics"}}, msgs);

    ok = run(h, "gfx_planner.add_dep", {{"src","0"},{"dst","1"}}, msgs);
    ok = run(h, "gfx_planner.add_dep", {{"src","1"},{"dst","2"}}, msgs);

    EXPECT_TRUE(run(h, "gfx_planner.build_plan", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.build_plan ok"));

    EXPECT_TRUE(run(h, "gfx_planner.build_completion", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "gfx_planner.build_completion ok"));

    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=3"));
    EXPECT_TRUE(hasMsg(msgs, "has_plan=1"));

    // Clear and verify reset
    ok = run(h, "gfx_planner.clear", {}, msgs);
    EXPECT_TRUE(run(h, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
    EXPECT_TRUE(hasMsg(msgs, "has_plan=0"));
}

TEST(GfxPlannerExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "gfx_planner.add_pass", {}, msgs);

    EXPECT_TRUE(run(h2, "gfx_planner.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "passes=0"));
}
