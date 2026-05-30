// ─────────────────────────────────────────────────────────────────────────────
//  Tests for nexus::automation expr_dsl.* commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/ScriptExpressionExtension.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace nexus::automation;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ScriptBatchHarness makeHarness()
{
    ScriptBatchHarness h;
    registerScriptExpressionCommands(h);
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

// ── expr_dsl.compile ─────────────────────────────────────────────────────────

TEST(ScriptExpressionExtension, CompileRequiresSrc)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "expr_dsl.compile", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(ScriptExpressionExtension, CompileLiteral)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","42"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "expr_dsl.compile ok"));
    EXPECT_TRUE(hasMsg(msgs, "vars=0"));
}

TEST(ScriptExpressionExtension, CompileWithVariables)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","x + y"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "vars=2"));
}

TEST(ScriptExpressionExtension, CompileWithBuiltinFunction)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","sin(x) + cos(y)"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "vars=2"));
}

TEST(ScriptExpressionExtension, CompileWithConstant)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    // pi is a built-in constant, not a variable
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","pi * r * r"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "vars=1"));  // only r
}

TEST(ScriptExpressionExtension, CompileBadExpressionFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "expr_dsl.compile", {{"src","((( unclosed"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "expr_dsl.compile fail"));
}

TEST(ScriptExpressionExtension, CompileReplacesExisting)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","x + y + z"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","a * b"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "vars=2"));
}

// ── expr_dsl.evaluate ────────────────────────────────────────────────────────

TEST(ScriptExpressionExtension, EvaluateRequiresCompile)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "expr_dsl.evaluate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(ScriptExpressionExtension, EvaluateLiteral)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","7"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.evaluate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=7.000000"));
}

TEST(ScriptExpressionExtension, EvaluateWithVariable)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","x * 2"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.evaluate", {{"var_x","5"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=10.000000"));
}

TEST(ScriptExpressionExtension, EvaluateTwoVariables)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","x + y"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.evaluate", {{"var_x","3"},{"var_y","4"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=7.000000"));
}

TEST(ScriptExpressionExtension, EvaluateMissingVariableFails)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","x + y"}}, msgs);
    // provide only x; y is missing
    EXPECT_FALSE(run(h, "expr_dsl.evaluate", {{"var_x","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(ScriptExpressionExtension, EvaluateBuiltinMath)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","sqrt(x)"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.evaluate", {{"var_x","9"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=3.000000"));
}

TEST(ScriptExpressionExtension, EvaluatePiConstant)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","pi"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.evaluate", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=3.14159"));
}

TEST(ScriptExpressionExtension, EvaluateComplex)
{
    // clamp(x, 0, 1) with x=2 → 1
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","clamp(x, 0, 1)"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.evaluate", {{"var_x","2"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=1.000000"));
}

// ── expr_dsl.vars ────────────────────────────────────────────────────────────

TEST(ScriptExpressionExtension, VarsRequiresCompile)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_FALSE(run(h, "expr_dsl.vars", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "error:"));
}

TEST(ScriptExpressionExtension, VarsNoFreeVars)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","1 + 2"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.vars", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "count=0"));
}

TEST(ScriptExpressionExtension, VarsListsSorted)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","z + a + m"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.vars", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "count=3"));
    // variableNames() returns sorted; verify all three names appear
    EXPECT_TRUE(hasMsg(msgs, "name=a"));
    EXPECT_TRUE(hasMsg(msgs, "name=m"));
    EXPECT_TRUE(hasMsg(msgs, "name=z"));
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ── expr_dsl.describe ────────────────────────────────────────────────────────

TEST(ScriptExpressionExtension, DescribeInitial)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    EXPECT_TRUE(run(h, "expr_dsl.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "compiled=0"));
}

TEST(ScriptExpressionExtension, DescribeAfterCompile)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","x * y"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "compiled=1"));
    EXPECT_TRUE(hasMsg(msgs, "src=x * y"));
}

// ── expr_dsl.clear ───────────────────────────────────────────────────────────

TEST(ScriptExpressionExtension, ClearOk)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h, "expr_dsl.compile", {{"src","x"}}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.clear", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "expr_dsl.clear ok"));
}

TEST(ScriptExpressionExtension, ClearDisablesEvaluate)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "expr_dsl.compile",  {{"src","42"}}, msgs);
    ok = run(h, "expr_dsl.clear",    {}, msgs);
    EXPECT_FALSE(run(h, "expr_dsl.evaluate", {}, msgs));
}

TEST(ScriptExpressionExtension, ClearThenRecompile)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;
    ok = run(h, "expr_dsl.compile", {{"src","x + y"}}, msgs);
    ok = run(h, "expr_dsl.clear",   {}, msgs);
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","a * b * c"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "vars=3"));
}

// ── Integration: full expression pipeline ────────────────────────────────────

TEST(ScriptExpressionExtension, FullPipeline)
{
    auto h = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok;

    // Compile a quadratic expression
    EXPECT_TRUE(run(h, "expr_dsl.compile", {{"src","a * x^2 + b * x + c"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "vars=4"));

    // Describe
    ok = run(h, "expr_dsl.describe", {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "compiled=1"));

    // List vars
    ok = run(h, "expr_dsl.vars", {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "count=4"));

    // Evaluate: a=1, b=-3, c=2, x=2  →  1*4 + (-3)*2 + 2 = 4-6+2 = 0
    EXPECT_TRUE(run(h, "expr_dsl.evaluate",
        {{"var_a","1"},{"var_b","-3"},{"var_c","2"},{"var_x","2"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=0.000000"));

    // Evaluate: same expression at x=1  →  1-3+2 = 0
    EXPECT_TRUE(run(h, "expr_dsl.evaluate",
        {{"var_a","1"},{"var_b","-3"},{"var_c","2"},{"var_x","1"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=0.000000"));

    // Evaluate at x=3  →  9-9+2 = 2
    EXPECT_TRUE(run(h, "expr_dsl.evaluate",
        {{"var_a","1"},{"var_b","-3"},{"var_c","2"},{"var_x","3"}}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "result=2.000000"));

    // Clear and verify
    ok = run(h, "expr_dsl.clear", {}, msgs);
    ok = run(h, "expr_dsl.describe", {}, msgs);
    EXPECT_TRUE(hasMsg(msgs, "compiled=0"));
}

TEST(ScriptExpressionExtension, StateIsolatedBetweenHarnesses)
{
    auto h1 = makeHarness();
    auto h2 = makeHarness();
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = run(h1, "expr_dsl.compile", {{"src","x"}}, msgs);

    EXPECT_TRUE(run(h2, "expr_dsl.describe", {}, msgs));
    EXPECT_TRUE(hasMsg(msgs, "compiled=0"));
}
