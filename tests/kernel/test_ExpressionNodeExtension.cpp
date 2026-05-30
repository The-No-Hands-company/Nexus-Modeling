// ─────────────────────────────────────────────────────────────────────────────
//  Tests for ExpressionNodeExtension — expr.add_constant, expr.add_adapter,
//  expr.evaluate, expr.read, expr.describe
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/ExpressionNodeExtension.h>
#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerExpressionNodeCommands(h);
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
//  expr.add_constant
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExpressionNodeExtension, AddConstantDefaultName)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_constant"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_constant ok"));
}

TEST(ExpressionNodeExtension, AddConstantSetsHasEvalGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    EXPECT_FALSE(ctx.hasEvalGraph);
    [[maybe_unused]] bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}}), msgs);
    EXPECT_TRUE(ctx.hasEvalGraph);
}

TEST(ExpressionNodeExtension, AddConstantValueInMessage)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}, {"value", "3.5"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_constant ok"));
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_constant ok id="));
}

TEST(ExpressionNodeExtension, AddConstantDuplicateNameFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok1 = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}}), msgs);
    msgs.clear();

    const bool ok2 = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}}), msgs);
    EXPECT_FALSE(ok2);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_constant error:"));
}

TEST(ExpressionNodeExtension, AddConstantRegistersInEvalNodesByName)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "myVal"}}), msgs);
    EXPECT_TRUE(ctx.evalNodesByName.count("myVal"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  expr.add_adapter
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExpressionNodeExtension, AddAdapterLiteralExpr)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Expression "42" has no free variables — no bindings needed.
    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "result"}, {"source", "42"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_adapter ok"));
    EXPECT_EQ(ctx.exprAdapters.size(), 1u);
    EXPECT_EQ(ctx.exprAdapterNames[0], "result");
}

TEST(ExpressionNodeExtension, AddAdapterWithBinding)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool c = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}, {"value", "5"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "dbl"}, {"source", "x * 2"}, {"bind_x", "x"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_adapter ok"));
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_adapter ok id="));
    EXPECT_EQ(ctx.exprAdapters.back().bindings().size(), 1u);
}

TEST(ExpressionNodeExtension, AddAdapterUnknownSourceNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "bad"}, {"source", "x + 1"}, {"bind_x", "ghost"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_adapter error:"));
}

TEST(ExpressionNodeExtension, AddAdapterDuplicateNameFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok1 = h.registry().execute(ctx,
        makeCmd("expr.add_adapter", {{"name", "e"}, {"source", "1"}}), msgs);
    msgs.clear();

    const bool ok2 = h.registry().execute(ctx,
        makeCmd("expr.add_adapter", {{"name", "e"}, {"source", "2"}}), msgs);
    EXPECT_FALSE(ok2);
    EXPECT_TRUE(hasPrefix(msgs, "expr.add_adapter error:"));
}

TEST(ExpressionNodeExtension, AddAdapterRegistersInExprAdapterNames)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool ok = h.registry().execute(ctx,
        makeCmd("expr.add_adapter", {{"name", "myExpr"}, {"source", "7"}}), msgs);
    EXPECT_EQ(ctx.exprAdapterNames.size(), 1u);
    EXPECT_EQ(ctx.exprAdapterNames[0], "myExpr");
}

// ─────────────────────────────────────────────────────────────────────────────
//  expr.evaluate
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExpressionNodeExtension, EvaluateEmptyGraphOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.evaluate"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.evaluate ok nodes=0 adapters=0"));
}

TEST(ExpressionNodeExtension, EvaluateLiteralAdapterOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("expr.add_adapter", {{"name", "r"}, {"source", "9"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.evaluate"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.evaluate ok nodes=1 adapters=1"));
}

TEST(ExpressionNodeExtension, EvaluateWithBoundConstant)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool c1 = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}, {"value", "4"}}), msgs);
    [[maybe_unused]] bool c2 = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "sq"}, {"source", "x * x"}, {"bind_x", "x"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.evaluate"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.evaluate ok nodes=2 adapters=1"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  expr.read
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExpressionNodeExtension, ReadMissingNameFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.read"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.read error:"));
}

TEST(ExpressionNodeExtension, ReadUnknownNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.read", {{"name", "ghost"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.read error:"));
}

TEST(ExpressionNodeExtension, ReadConstantAfterEvaluate)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool c = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}, {"value", "7"}}), msgs);
    [[maybe_unused]] bool ev = h.registry().execute(ctx,
        makeCmd("expr.evaluate"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.read", {{"name", "x"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.read ok name=x"));
}

TEST(ExpressionNodeExtension, ReadAdapterResultAfterEvaluate)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool c = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}, {"value", "3"}}), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "triple"}, {"source", "x * 3"}, {"bind_x", "x"}}), msgs);
    [[maybe_unused]] bool ev = h.registry().execute(ctx,
        makeCmd("expr.evaluate"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.read", {{"name", "triple"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.read ok name=triple"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  expr.describe
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExpressionNodeExtension, DescribeEmptyGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("expr.describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "expr.describe nodes=0 adapters=0"));
}

TEST(ExpressionNodeExtension, DescribeAfterAddAdapter)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool c = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "a"}, {"value", "1"}}), msgs);
    [[maybe_unused]] bool ad = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "e"}, {"source", "a + 1"}, {"bind_a", "a"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx, makeCmd("expr.describe"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "expr.describe nodes=2 adapters=1"));
    EXPECT_TRUE(hasPrefix(msgs, "expr.describe adapter index=0 name=e bindings=1"));
}

TEST(ExpressionNodeExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    for (int i = 0; i < 3; ++i) {
        const std::string n = "e" + std::to_string(i);
        [[maybe_unused]] bool ok = h.registry().execute(ctx,
            makeCmd("expr.add_adapter", {{"name", n}, {"source", "1"}}), msgs);
    }
    msgs.clear();

    h.registry().execute(ctx, makeCmd("expr.describe"), msgs);
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full pipeline: constant → expression → read
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExpressionNodeExtension, FullComputePipeline)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // x = 6, y = 7  →  result = x * y  (should be 42)
    [[maybe_unused]] bool cx = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "x"}, {"value", "6"}}), msgs);
    [[maybe_unused]] bool cy = h.registry().execute(ctx,
        makeCmd("expr.add_constant", {{"name", "y"}, {"value", "7"}}), msgs);
    [[maybe_unused]] bool ad = h.registry().execute(ctx,
        makeCmd("expr.add_adapter",
            {{"name", "result"}, {"source", "x * y"},
             {"bind_x", "x"}, {"bind_y", "y"}}), msgs);

    EXPECT_EQ(ctx.exprAdapters.size(), 1u);
    EXPECT_EQ(ctx.evalGraph.nodeCount(), 3u);

    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("expr.evaluate"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "expr.evaluate ok nodes=3 adapters=1"));
    }

    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("expr.read", {{"name", "result"}}), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "expr.read ok name=result"));
    }

    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("expr.describe"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "expr.describe nodes=3 adapters=1"));
        EXPECT_TRUE(hasPrefix(msgs, "expr.describe adapter index=0 name=result bindings=2"));
    }
}
