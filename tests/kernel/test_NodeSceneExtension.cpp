// Tests for scene.add_node, scene.connect, scene.disconnect,
// scene.set_parent, scene.clear_parent, scene.evaluate, scene.describe
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/NodeSceneExtension.h>
#include <nexus/scene/NodeScene.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerNodeSceneCommands(h);
    return h;
}

uint32_t addSceneNode(nexus::automation::ScriptBatchHarness& h,
                      nexus::automation::ScriptContext& ctx,
                      const char* name,
                      const char* kind = "constant")
{
    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.add_node";
    cmd.args["name"] = name;
    cmd.args["kind"] = kind;
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

// ── scene.add_node ────────────────────────────────────────────────────────────

TEST(NodeSceneExtension, AddNodeSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.add_node";
    cmd.args["name"] = "root";
    cmd.args["kind"] = "geometry";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("scene.add_node ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
    EXPECT_EQ(ctx.nodeScene.nodeCount(), 1u);
}

TEST(NodeSceneExtension, AddNodeSetsHasNodeScene)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    EXPECT_FALSE(ctx.hasNodeScene);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.add_node";
    cmd.args["name"] = "n";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_TRUE(ctx.hasNodeScene);
}

TEST(NodeSceneExtension, AddNodeDuplicateNameFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    ASSERT_GT(addSceneNode(h, ctx, "dup", "constant"), 0u);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.add_node";
    cmd.args["name"] = "dup";
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
    ASSERT_FALSE(msgs.empty());
    EXPECT_EQ(ctx.nodeScene.nodeCount(), 1u);
}

TEST(NodeSceneExtension, AddNodeAllKinds)
{
    const char* kinds[] = {
        "geometry","animation","transform","merge",
        "proxy_geometry","reconstruction","expression","constant"
    };
    for (std::size_t i = 0; i < std::size(kinds); ++i) {
        auto h = makeHarness();
        nexus::automation::ScriptContext ctx;
        const std::string name = std::string("n") + std::to_string(i);
        const uint32_t id = addSceneNode(h, ctx, name.c_str(), kinds[i]);
        EXPECT_GT(id, 0u) << "kind '" << kinds[i] << "' failed";
    }
}

TEST(NodeSceneExtension, AddNodeMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.add_node";
    cmd.args["name"] = "sphere";
    cmd.args["kind"] = "geometry";

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

TEST(NodeSceneExtension, AddNodeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.add_node";
    cmd.args["name"] = "t"; cmd.args["kind"] = "transform";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── scene.connect ─────────────────────────────────────────────────────────────

TEST(NodeSceneExtension, ConnectSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t src = addSceneNode(h, ctx, "src", "constant");
    const uint32_t dst = addSceneNode(h, ctx, "dst", "geometry");
    ASSERT_GT(src, 0u); ASSERT_GT(dst, 0u);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.connect";
    cmd.args["src"] = std::to_string(src);
    cmd.args["dst"] = std::to_string(dst);

    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_TRUE(ctx.nodeScene.isConnected(src, dst));
}

TEST(NodeSceneExtension, ConnectUnknownNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.connect";
    cmd.args["src"] = "888";
    cmd.args["dst"] = "999";
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
}

TEST(NodeSceneExtension, ConnectDuplicateEdgeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addSceneNode(h, ctx, "a");
    const uint32_t b = addSceneNode(h, ctx, "b");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.connect";
    cmd.args["src"] = std::to_string(a);
    cmd.args["dst"] = std::to_string(b);

    std::vector<std::string> m1, m2;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, m1));
    EXPECT_FALSE(h.registry().execute(ctx, cmd, m2));
}

TEST(NodeSceneExtension, ConnectMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addSceneNode(h, ctx, "a");
    const uint32_t b = addSceneNode(h, ctx, "b");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.connect";
    cmd.args["src"] = std::to_string(a);
    cmd.args["dst"] = std::to_string(b);

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── scene.disconnect ──────────────────────────────────────────────────────────

TEST(NodeSceneExtension, DisconnectSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addSceneNode(h, ctx, "a");
    const uint32_t b = addSceneNode(h, ctx, "b");

    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.connect";
        cmd.args["src"] = std::to_string(a);
        cmd.args["dst"] = std::to_string(b);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.disconnect";
    cmd.args["src"] = std::to_string(a);
    cmd.args["dst"] = std::to_string(b);
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_FALSE(ctx.nodeScene.isConnected(a, b));
}

TEST(NodeSceneExtension, DisconnectAbsentEdgeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addSceneNode(h, ctx, "a");
    const uint32_t b = addSceneNode(h, ctx, "b");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.disconnect";
    cmd.args["src"] = std::to_string(a);
    cmd.args["dst"] = std::to_string(b);
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
}

// ── scene.set_parent ──────────────────────────────────────────────────────────

TEST(NodeSceneExtension, SetParentSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t parent = addSceneNode(h, ctx, "root");
    const uint32_t child  = addSceneNode(h, ctx, "child");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.set_parent";
    cmd.args["child"]  = std::to_string(child);
    cmd.args["parent"] = std::to_string(parent);

    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_EQ(ctx.nodeScene.parent(child), parent);
}

TEST(NodeSceneExtension, SetParentUnknownNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t parent = addSceneNode(h, ctx, "root");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.set_parent";
    cmd.args["child"]  = "9999";
    cmd.args["parent"] = std::to_string(parent);
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
}

TEST(NodeSceneExtension, SetParentMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t p = addSceneNode(h, ctx, "p");
    const uint32_t c = addSceneNode(h, ctx, "c");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.set_parent";
    cmd.args["child"]  = std::to_string(c);
    cmd.args["parent"] = std::to_string(p);

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── scene.clear_parent ────────────────────────────────────────────────────────

TEST(NodeSceneExtension, ClearParentSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t p = addSceneNode(h, ctx, "p");
    const uint32_t c = addSceneNode(h, ctx, "c");

    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.set_parent";
        cmd.args["child"]  = std::to_string(c);
        cmd.args["parent"] = std::to_string(p);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.clear_parent";
    cmd.args["child"] = std::to_string(c);
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    EXPECT_EQ(ctx.nodeScene.parent(c), nexus::kInvalidSceneNodeId);
}

TEST(NodeSceneExtension, ClearParentUnknownNodeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.clear_parent";
    cmd.args["child"] = "7777";
    std::vector<std::string> msgs;
    EXPECT_FALSE(h.registry().execute(ctx, cmd, msgs));
}

// ── scene.evaluate ────────────────────────────────────────────────────────────

TEST(NodeSceneExtension, EvaluateEmptySceneSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.evaluate";
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("scene.evaluate ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(NodeSceneExtension, EvaluateLinearChainReportsNodes)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t a = addSceneNode(h, ctx, "a", "constant");
    const uint32_t b = addSceneNode(h, ctx, "b", "transform");
    const uint32_t c = addSceneNode(h, ctx, "c", "geometry");

    auto connect = [&](uint32_t s, uint32_t d) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.connect";
        cmd.args["src"] = std::to_string(s);
        cmd.args["dst"] = std::to_string(d);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    };
    connect(a, b); connect(b, c);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.evaluate";
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool has3 = false;
    for (const auto& m : msgs)
        if (m.find("nodes=3") != std::string::npos) { has3 = true; break; }
    EXPECT_TRUE(has3);
}

TEST(NodeSceneExtension, EvaluateCycleReturnsWarn)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t a = addSceneNode(h, ctx, "a");
    const uint32_t b = addSceneNode(h, ctx, "b");

    [[maybe_unused]] bool c1 = ctx.nodeScene.connect(a, b);
    [[maybe_unused]] bool c2 = ctx.nodeScene.connect(b, a);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.evaluate";
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    bool foundCycle = false;
    for (const auto& m : msgs)
        if (m.find("cycle_detected") != std::string::npos) { foundCycle = true; break; }
    EXPECT_TRUE(foundCycle);
}

TEST(NodeSceneExtension, EvaluateMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addSceneNode(h, ctx, "x");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.evaluate";
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── scene.describe ────────────────────────────────────────────────────────────

TEST(NodeSceneExtension, DescribeEmptyScene)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.describe";
    std::vector<std::string> msgs;
    EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("scene.describe") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(NodeSceneExtension, DescribeReportsNodeCount)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addSceneNode(h, ctx, "a");
    addSceneNode(h, ctx, "b");
    addSceneNode(h, ctx, "c");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool found3 = false;
    for (const auto& m : msgs)
        if (m.find("nodes=3") != std::string::npos) { found3 = true; break; }
    EXPECT_TRUE(found3);
}

TEST(NodeSceneExtension, DescribeListsNodeDetails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addSceneNode(h, ctx, "my_geo", "geometry");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool foundNode = false;
    for (const auto& m : msgs)
        if (m.find("scene.describe node") != std::string::npos) { foundNode = true; break; }
    EXPECT_TRUE(foundNode);
}

TEST(NodeSceneExtension, DescribeShowsHierarchy)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    const uint32_t p = addSceneNode(h, ctx, "parent");
    const uint32_t c = addSceneNode(h, ctx, "child");

    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.set_parent";
        cmd.args["child"]  = std::to_string(c);
        cmd.args["parent"] = std::to_string(p);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool foundParent = false;
    for (const auto& m : msgs)
        if (m.find("parent=" + std::to_string(p)) != std::string::npos) { foundParent = true; break; }
    EXPECT_TRUE(foundParent);
}

TEST(NodeSceneExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    addSceneNode(h, ctx, "n1"); addSceneNode(h, ctx, "n2");

    nexus::automation::ScriptCommand cmd;
    cmd.name = "scene.describe";
    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);
    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── Pipeline: build hierarchy → evaluate → describe ──────────────────────────

TEST(NodeSceneExtension, FullSceneHierarchyPipeline)
{
    // world → group → [mesh_a, mesh_b], group2 → light
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;

    const uint32_t world  = addSceneNode(h, ctx, "world",  "constant");
    const uint32_t group  = addSceneNode(h, ctx, "group",  "merge");
    const uint32_t meshA  = addSceneNode(h, ctx, "mesh_a", "geometry");
    const uint32_t meshB  = addSceneNode(h, ctx, "mesh_b", "geometry");
    const uint32_t group2 = addSceneNode(h, ctx, "group2", "merge");
    const uint32_t light  = addSceneNode(h, ctx, "light",  "constant");

    ASSERT_GT(world, 0u);

    // Dependency edges: world feeds group and group2
    auto connect = [&](uint32_t s, uint32_t d) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.connect";
        cmd.args["src"] = std::to_string(s);
        cmd.args["dst"] = std::to_string(d);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    };
    connect(world, group);
    connect(world, group2);
    connect(group, meshA);
    connect(group, meshB);
    connect(group2, light);

    // Parent hierarchy
    auto setParent = [&](uint32_t child, uint32_t parent) {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.set_parent";
        cmd.args["child"]  = std::to_string(child);
        cmd.args["parent"] = std::to_string(parent);
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    };
    setParent(group,  world);
    setParent(group2, world);
    setParent(meshA,  group);
    setParent(meshB,  group);
    setParent(light,  group2);

    // Evaluate
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.evaluate";
        std::vector<std::string> msgs;
        EXPECT_TRUE(h.registry().execute(ctx, cmd, msgs));
        bool has6 = false;
        for (const auto& m : msgs)
            if (m.find("nodes=6") != std::string::npos) { has6 = true; break; }
        EXPECT_TRUE(has6);
    }

    // Verify hierarchy via describe
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "scene.describe";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
        bool has6n = false;
        for (const auto& m : msgs)
            if (m.find("nodes=6") != std::string::npos) { has6n = true; break; }
        EXPECT_TRUE(has6n);
    }

    // children of world should include group and group2
    const auto worldChildren = ctx.nodeScene.children(world);
    EXPECT_EQ(worldChildren.size(), 2u);
}
