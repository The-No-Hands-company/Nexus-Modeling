// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation param_samples.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/ParametricSamplesExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerParametricSamplesCommands(h);
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

// ── param_samples.make_rect ───────────────────────────────────────────────────

TEST(ParametricSamplesExtension, MakeRectDefaultSize)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "param_samples.make_rect", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "param_samples.make_rect ok"));
    EXPECT_TRUE(hasMsg(msgs, "width=1.000000"));
    EXPECT_TRUE(hasMsg(msgs, "height=1.000000"));
}

TEST(ParametricSamplesExtension, MakeRectCustomSize)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "param_samples.make_rect",
        {{"width","4"},{"height","3"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=4.000000"));
    EXPECT_TRUE(hasMsg(msgs, "height=3.000000"));
}

TEST(ParametricSamplesExtension, MakeRectCreatesEntities)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "param_samples.make_rect", {{"width","2"},{"height","2"}}, msgs));
    // A rectangle sketch has at least 4 corner entities
    EXPECT_TRUE(hasMsg(msgs, "entities="));
    // Confirm entities > 0
    for (const auto& m : msgs) {
        auto pos = m.find("entities=");
        if (pos != std::string::npos) {
            int n = std::stoi(m.substr(pos + 9));
            EXPECT_GT(n, 0);
            break;
        }
    }
}

TEST(ParametricSamplesExtension, MakeRectReplacesExisting)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "param_samples.make_rect",
        {{"width","10"},{"height","10"}}, msgs);
    EXPECT_TRUE(run(h, "param_samples.make_rect",
        {{"width","2"},{"height","3"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=2.000000"));
}

TEST(ParametricSamplesExtension, MakeRectMessagesSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "param_samples.make_rect", {}, msgs));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── param_samples.solve ───────────────────────────────────────────────────────

TEST(ParametricSamplesExtension, SolveRequiresModel)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "param_samples.solve", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(ParametricSamplesExtension, SolveDefaultConfig)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "param_samples.make_rect",
        {{"width","1"},{"height","1"}}, msgs);
    EXPECT_TRUE(run(h, "param_samples.solve", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "param_samples.solve ok"));
    EXPECT_TRUE(hasMsg(msgs, "iterations="));
}

TEST(ParametricSamplesExtension, SolveConverges)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "param_samples.make_rect",
        {{"width","2"},{"height","3"}}, msgs);
    EXPECT_TRUE(run(h, "param_samples.solve",
        {{"max_iter","100"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "converged=1"));
}

TEST(ParametricSamplesExtension, SolveCustomMaxIter)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "param_samples.make_rect", {}, msgs);
    EXPECT_TRUE(run(h, "param_samples.solve", {{"max_iter","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "iterations="));
}

// ── param_samples.describe ────────────────────────────────────────────────────

TEST(ParametricSamplesExtension, DescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "param_samples.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "has_model=0"));
    EXPECT_TRUE(hasMsg(msgs, "entities=0"));
}

TEST(ParametricSamplesExtension, DescribeAfterMake)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "param_samples.make_rect", {}, msgs);
    EXPECT_TRUE(run(h, "param_samples.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "has_model=1"));
}

TEST(ParametricSamplesExtension, DescribeAfterSolve)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "param_samples.make_rect", {{"width","5"},{"height","5"}}, msgs);
    ok = run(h, "param_samples.solve",     {}, msgs);
    EXPECT_TRUE(run(h, "param_samples.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "has_model=1"));
    EXPECT_TRUE(hasMsg(msgs, "converged=1"));
}

// ── Integration ───────────────────────────────────────────────────────────────

TEST(ParametricSamplesExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Make a 3×4 rectangle
    EXPECT_TRUE(run(h, "param_samples.make_rect",
        {{"width","3"},{"height","4"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "width=3.000000"));

    // Solve with tight tolerance
    EXPECT_TRUE(run(h, "param_samples.solve",
        {{"max_iter","200"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "converged=1"));

    // Describe shows model + convergence
    EXPECT_TRUE(run(h, "param_samples.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "has_model=1"));
    EXPECT_TRUE(hasMsg(msgs, "converged=1"));

    // Re-make replaces the model and resets solver state
    EXPECT_TRUE(run(h, "param_samples.make_rect",
        {{"width","1"},{"height","1"}}, msgs));
    ok = run(h, "param_samples.describe", {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "converged=0"));  // reset after make
}

TEST(ParametricSamplesExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "param_samples.make_rect", {}, msgs);

    EXPECT_TRUE(run(h2, "param_samples.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "has_model=0"));
}
