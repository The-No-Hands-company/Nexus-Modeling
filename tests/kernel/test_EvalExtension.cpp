// Tests for eval.add_node, eval.connect, eval.disconnect, eval.mark_dirty,
// eval.evaluate, and eval.describe automation commands.
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/EvalExtension.h>
#include <nexus/eval/EvalGraph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerEvalCommands(h);
    return h;
}

// Add a node and return its id parsed from the success message.
uint32_t addNode(nexus::automation::ScriptBatchHarness& h,
                 nexus::automation::ScriptContext& ctx,
                 const char* kind = "constant",
                 const char* name = "n")
{
    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.add_node";
    cmd.args["kind"] = kind;
    cmd.args["name"] = name;
    std::vector<std::string> msgs;
    if (!h.registry().execute(ctx, cmd, msgs)) return 0;
    for (const auto& m : msgs) {
        auto pos = m.find("id=");
        if (pos == std::string::npos) continue;
        uint32_t id = 0;
        std::from_chars(m.data() + pos + 3, m.data() + m.size(), id);
        return id;
    }
    return 0;
}

} // namespace

// ── eval.add_node ─────────────────────────────────────────────────────────────

TEST(EvalExtension, AddNodeDefaultKind)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.add_node";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("eval.add_node ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(EvalExtension, AddNodeAllKinds)
{
    const char* kinds[] = {
        "geometry","animation","transform","merge",
        "proxy_geometry","reconstruction","expression","constant"
    };
    for (const auto* k : kinds) {
        auto h = makeHarness();
        nexus::automation::ScriptContext ctx;
        const uint32_t id = addNode(h, ctx, k, "test");
        EXPECT_GT(id, 0u) << "kind '" << k << "' failed";
    }
}

TEST(EvalExtension, AddNodeReturnsDistinctIds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addNode(h, ctx, "constant", "a");
    const uint32_t b = addNode(h, ctx, "geometry", "b");
    const uint32_t c = addNode(h, ctx, "transform","c");
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_GT(a, 0u);
}

TEST(EvalExtension, AddNodeSetsHasEvalGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    EXPECT_FALSE(ctx.hasEvalGraph);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.add_node";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_TRUE(ctx.hasEvalGraph);
}

TEST(EvalExtension, AddNodeMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.add_node";
    cmd.args["kind"] = "geometry";
    cmd.args["name"] = "sphere_gen";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool hasId = false, hasKind = false, hasName = false;
    for (const auto& m : msgs) {
        if (m.find("id=")   != std::string::npos) hasId   = true;
        if (m.find("kind=") != std::string::npos) hasKind = true;
        if (m.find("name=") != std::string::npos) hasName = true;
    }
    EXPECT_TRUE(hasId);
    EXPECT_TRUE(hasKind);
    EXPECT_TRUE(hasName);
}

TEST(EvalExtension, AddNodeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.add_node";
    cmd.args["kind"] = "transform";
    cmd.args["name"] = "t";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── eval.connect ─────────────────────────────────────────────────────────────

TEST(EvalExtension, ConnectTwoNodesSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t src = addNode(h, ctx, "constant", "src");
    const uint32_t dst = addNode(h, ctx, "geometry", "dst");
    ASSERT_GT(src, 0u); ASSERT_GT(dst, 0u);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.connect";
    cmd.args["src"] = std::to_string(src);
    cmd.args["dst"] = std::to_string(dst);

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("eval.connect ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
    EXPECT_TRUE(ctx.evalGraph.isConnected(src, dst));
}

TEST(EvalExtension, ConnectUnknownNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.connect";
    cmd.args["src"] = "999";
    cmd.args["dst"] = "1000";

    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
    ASSERT_FALSE(msgs.empty());
}

TEST(EvalExtension, ConnectDuplicateEdgeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t src = addNode(h, ctx, "constant", "s");
    const uint32_t dst = addNode(h, ctx, "geometry", "d");

    nexus::automation::ScriptCommand connect;
    connect.name = "eval.connect";
    connect.args["src"] = std::to_string(src);
    connect.args["dst"] = std::to_string(dst);

    std::vector<std::string> m1, m2;
    ASSERT_TRUE(h.registry().execute(ctx, connect, m1));
    EXPECT_FALSE(h.registry().execute(ctx, connect, m2));
}

TEST(EvalExtension, ConnectMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addNode(h, ctx, "constant", "a");
    const uint32_t b = addNode(h, ctx, "geometry", "b");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.connect";
    cmd.args["src"] = std::to_string(a);
    cmd.args["dst"] = std::to_string(b);

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── eval.disconnect ───────────────────────────────────────────────────────────

TEST(EvalExtension, DisconnectExistingEdgeSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t src = addNode(h, ctx, "constant", "s");
    const uint32_t dst = addNode(h, ctx, "geometry", "d");

    // Connect first
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "eval.connect";
        cmd.args["src"] = std::to_string(src);
        cmd.args["dst"] = std::to_string(dst);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.disconnect";
    cmd.args["src"] = std::to_string(src);
    cmd.args["dst"] = std::to_string(dst);
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_FALSE(ctx.evalGraph.isConnected(src, dst));
}

TEST(EvalExtension, DisconnectAbsentEdgeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addNode(h, ctx, "constant", "a");
    const uint32_t b = addNode(h, ctx, "geometry", "b");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.disconnect";
    cmd.args["src"] = std::to_string(a);
    cmd.args["dst"] = std::to_string(b);
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
}

// ── eval.mark_dirty ───────────────────────────────────────────────────────────

TEST(EvalExtension, MarkDirtyKnownNodeSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t id = addNode(h, ctx, "geometry", "g");

    // Clear dirty first so we can detect re-dirtying
    ctx.evalGraph.clearDirtyAll();
    EXPECT_FALSE(ctx.evalGraph.isDirty(id));

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.mark_dirty";
    cmd.args["id"] = std::to_string(id);
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_TRUE(ctx.evalGraph.isDirty(id));
}

TEST(EvalExtension, MarkDirtyUnknownNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.mark_dirty";
    cmd.args["id"] = "9999";
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
    ASSERT_FALSE(msgs.empty());
}

// ── eval.evaluate ─────────────────────────────────────────────────────────────

TEST(EvalExtension, EvaluateEmptyGraphSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.evaluate";
    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("eval.evaluate ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(EvalExtension, EvaluateLinearChainSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    // A → B → C
    const uint32_t a = addNode(h, ctx, "constant", "a");
    const uint32_t b = addNode(h, ctx, "transform","b");
    const uint32_t c = addNode(h, ctx, "geometry", "c");

    auto connect = [&](uint32_t s, uint32_t d) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "eval.connect";
        cmd.args["src"] = std::to_string(s);
        cmd.args["dst"] = std::to_string(d);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    };
    connect(a, b); connect(b, c);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.evaluate";
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool hasNodes = false;
    for (const auto& m : msgs)
        if (m.find("nodes=3") != std::string::npos) { hasNodes = true; break; }
    EXPECT_TRUE(hasNodes);
}

TEST(EvalExtension, EvaluateCycleReturnsWarn)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t a = addNode(h, ctx, "geometry", "a");
    const uint32_t b = addNode(h, ctx, "geometry", "b");

    // Connect a→b directly through graph API to create a cycle a←b→a
    [[maybe_unused]] bool cab = ctx.evalGraph.connect(a, b);
    [[maybe_unused]] bool cba = ctx.evalGraph.connect(b, a);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.evaluate";
    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    EXPECT_TRUE(ok); // informational — never hard-fails
    bool foundCycle = false;
    for (const auto& m : msgs)
        if (m.find("cycle_detected") != std::string::npos) { foundCycle = true; break; }
    EXPECT_TRUE(foundCycle);
}

TEST(EvalExtension, EvaluateMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addNode(h, ctx, "constant", "x");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.evaluate";
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── eval.describe ─────────────────────────────────────────────────────────────

TEST(EvalExtension, DescribeEmptyGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.describe";
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("eval.describe") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(EvalExtension, DescribeReportsNodeAndEdgeCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t a = addNode(h, ctx, "constant", "a");
    const uint32_t b = addNode(h, ctx, "geometry", "b");
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "eval.connect";
        cmd.args["src"] = std::to_string(a);
        cmd.args["dst"] = std::to_string(b);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool found2 = false, found1e = false;
    for (const auto& m : msgs) {
        if (m.find("nodes=2") != std::string::npos) found2   = true;
        if (m.find("edges=1") != std::string::npos) found1e  = true;
    }
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found1e);
}

TEST(EvalExtension, DescribeListsNodeDetails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addNode(h, ctx, "animation", "my_anim_node");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool foundNodeLine = false;
    for (const auto& m : msgs)
        if (m.find("eval.describe node") != std::string::npos) { foundNodeLine = true; break; }
    EXPECT_TRUE(foundNodeLine);
}

TEST(EvalExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addNode(h, ctx, "merge", "m1");
    addNode(h, ctx, "merge", "m2");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.describe";
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── Pipeline: build → connect → evaluate → describe ──────────────────────────

TEST(EvalExtension, FullPipelineDAG)
{
    // Build a diamond DAG: src → left, src → right, left → sink, right → sink
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t src   = addNode(h, ctx, "constant",  "src");
    const uint32_t left  = addNode(h, ctx, "transform", "left");
    const uint32_t right = addNode(h, ctx, "transform", "right");
    const uint32_t sink  = addNode(h, ctx, "merge",     "sink");

    auto connect = [&](uint32_t s, uint32_t d) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "eval.connect";
        cmd.args["src"] = std::to_string(s);
        cmd.args["dst"] = std::to_string(d);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    };
    connect(src, left);
    connect(src, right);
    connect(left, sink);
    connect(right, sink);

    // Evaluate
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "eval.evaluate";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        bool found4 = false;
        for (const auto& m : msgs)
            if (m.find("nodes=4") != std::string::npos) { found4 = true; break; }
        EXPECT_TRUE(found4);
    }

    // Describe — graph has 4 nodes, 4 edges
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "eval.describe";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
        bool found4n = false;
        for (const auto& m : msgs)
            if (m.find("nodes=4") != std::string::npos) { found4n = true; break; }
        EXPECT_TRUE(found4n);
    }
}

TEST(EvalExtension, MarkDirtyPropagatesDownstream)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t a = addNode(h, ctx, "constant", "a");
    const uint32_t b = addNode(h, ctx, "geometry", "b");
    const uint32_t c = addNode(h, ctx, "geometry", "c");

    [[maybe_unused]] bool c1 = ctx.evalGraph.connect(a, b);
    [[maybe_unused]] bool c2 = ctx.evalGraph.connect(b, c);

    // Clear dirty state
    ctx.evalGraph.clearDirtyAll();
    EXPECT_FALSE(ctx.evalGraph.isDirty(a));
    EXPECT_FALSE(ctx.evalGraph.isDirty(b));
    EXPECT_FALSE(ctx.evalGraph.isDirty(c));

    // Mark a dirty via automation command
    nexus::automation::ScriptCommand cmd;
    cmd.name = "eval.mark_dirty";
    cmd.args["id"] = std::to_string(a);
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    // All downstream should be dirty
    EXPECT_TRUE(ctx.evalGraph.isDirty(a));
    EXPECT_TRUE(ctx.evalGraph.isDirty(b));
    EXPECT_TRUE(ctx.evalGraph.isDirty(c));
}
