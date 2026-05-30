// ─────────────────────────────────────────────────────────────────────────────
//  Tests for SubgraphExtension — subgraph.template_new, add_node, connect,
//  declare_input/output, validate, instantiate, describe,
//  registry_add/list/remove
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SubgraphExtension.h>
#include <nexus/automation/AutomationScript.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerSubgraphCommands(h);
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
//  subgraph.template_new
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, TemplateNewDefault)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.template_new ok name=tmpl"));
}

TEST(SubgraphExtension, TemplateNewCustomName)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","myTmpl"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.template_new ok name=myTmpl"));
}

TEST(SubgraphExtension, TemplateNewResetsState)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Create a template with nodes, then reset.
    [[maybe_unused]] bool n1 = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","A"}}), msgs);
    [[maybe_unused]] bool n2 = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","fresh"}}), msgs);
    msgs.clear();

    // Describe — fresh template should have 0 nodes.
    h.registry().execute(ctx, makeCmd("subgraph.describe"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.describe template name=fresh nodes=0"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.add_node
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, AddNodeNoTemplateFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.add_node"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.add_node error: no template"));
}

TEST(SubgraphExtension, AddNodeReturnsLocalId)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","src"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.add_node ok local_id=1"));
}

TEST(SubgraphExtension, AddNodeMultipleIds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","A"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","B"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.add_node ok local_id=2"));
}

TEST(SubgraphExtension, AddNodeKindGeometry)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    msgs.clear();

    h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","g"},{"kind","geometry"}}), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.add_node ok local_id=1 name=g kind=geometry"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.connect
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, ConnectNoTemplateFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    EXPECT_FALSE(ok);
}

TEST(SubgraphExtension, ConnectTwoNodes)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n  = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","src"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","dst"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.connect ok src=1 dst=2"));
}

TEST(SubgraphExtension, ConnectDuplicateEdgeFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n  = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","A"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","B"}}), msgs);
    [[maybe_unused]] bool c1 = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    EXPECT_FALSE(ok);
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.declare_input / subgraph.declare_output
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, DeclareInputOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","in_node"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.declare_input", {{"port","x"},{"node","1"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.declare_input ok port=x node=1"));
}

TEST(SubgraphExtension, DeclareInputEmptyPortFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","n"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.declare_input", {{"node","1"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.declare_input error:"));
}

TEST(SubgraphExtension, DeclareOutputOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","out_node"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.declare_output", {{"port","result"},{"node","1"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.declare_output ok port=result node=1"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.validate
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, ValidateNoTemplate)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.validate"), msgs);
    EXPECT_TRUE(ok); // informational — always true
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.validate warn no_template"));
}

TEST(SubgraphExtension, ValidateEmptyTemplateOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.validate"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.validate ok nodes=0"));
}

TEST(SubgraphExtension, ValidateWithNodesAndEdge)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n  = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","A"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","B"}}), msgs);
    [[maybe_unused]] bool c  = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.validate"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.validate ok nodes=2 edges=1"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.instantiate
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, InstantiateNoTemplateFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.instantiate"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.instantiate error:"));
}

TEST(SubgraphExtension, InstantiateCreatesHostNodes)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n  = h.registry().execute(ctx, makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","A"}}), msgs);
    [[maybe_unused]] bool a2 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","B"}}), msgs);
    [[maybe_unused]] bool c  = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.instantiate", {{"prefix","run1"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.instantiate ok prefix=run1 host_nodes=2"));
    EXPECT_TRUE(ctx.hasEvalGraph);
    EXPECT_EQ(ctx.evalGraph.nodeCount(), 2u);
}

TEST(SubgraphExtension, InstantiateSetsHasEvalGraph)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    EXPECT_FALSE(ctx.hasEvalGraph);
    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","x"}}), msgs);
    [[maybe_unused]] bool i = h.registry().execute(ctx,
        makeCmd("subgraph.instantiate", {{"prefix","p"}}), msgs);
    EXPECT_TRUE(ctx.hasEvalGraph);
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.describe
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, DescribeNoTemplate)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.describe"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.describe template none"));
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.describe registry size=0 instances=0"));
}

TEST(SubgraphExtension, DescribeAfterTemplateNew)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","t1"}}), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","x"}}), msgs);
    msgs.clear();

    h.registry().execute(ctx, makeCmd("subgraph.describe"), msgs);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.describe template name=t1 nodes=1"));
}

TEST(SubgraphExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new"), msgs);
    msgs.clear();

    h.registry().execute(ctx, makeCmd("subgraph.describe"), msgs);
    EXPECT_TRUE(std::is_sorted(msgs.begin(), msgs.end()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  subgraph.registry_*
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, RegistryAddNoTemplateFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.registry_add"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_add error:"));
}

TEST(SubgraphExtension, RegistryAddAndList)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","myTmpl"}}), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","x"}}), msgs);
    msgs.clear();

    {
        bool ok = h.registry().execute(ctx, makeCmd("subgraph.registry_add"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_add ok name=myTmpl registry_size=1"));
    }

    msgs.clear();
    {
        bool ok = h.registry().execute(ctx, makeCmd("subgraph.registry_list"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_list ok count=1"));
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_list name=myTmpl"));
    }
}

TEST(SubgraphExtension, RegistryDuplicateAddFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","dup"}}), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","x"}}), msgs);
    [[maybe_unused]] bool r1 = h.registry().execute(ctx,
        makeCmd("subgraph.registry_add"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.registry_add"), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_add error:"));
}

TEST(SubgraphExtension, RegistryRemoveOk)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    [[maybe_unused]] bool n = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","t"}}), msgs);
    [[maybe_unused]] bool a = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","x"}}), msgs);
    [[maybe_unused]] bool ra = h.registry().execute(ctx,
        makeCmd("subgraph.registry_add"), msgs);
    msgs.clear();

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.registry_remove", {{"name","t"}}), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_remove ok name=t registry_size=0"));
}

TEST(SubgraphExtension, RegistryRemoveUnknownFails)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.registry_remove", {{"name","ghost"}}), msgs);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_remove error:"));
}

TEST(SubgraphExtension, RegistryListEmpty)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    const bool ok = h.registry().execute(ctx,
        makeCmd("subgraph.registry_list"), msgs);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_list ok count=0"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full pipeline: build template → declare ports → validate → instantiate
// ─────────────────────────────────────────────────────────────────────────────

TEST(SubgraphExtension, FullPortedPipeline)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    std::vector<std::string> msgs;

    // Build: in_node → proc_node → out_node
    [[maybe_unused]] bool tn = h.registry().execute(ctx,
        makeCmd("subgraph.template_new", {{"name","linear"}}), msgs);
    [[maybe_unused]] bool n1 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","in"},{"kind","constant"}}), msgs);
    [[maybe_unused]] bool n2 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","proc"},{"kind","geometry"}}), msgs);
    [[maybe_unused]] bool n3 = h.registry().execute(ctx,
        makeCmd("subgraph.add_node", {{"name","out"},{"kind","merge"}}), msgs);
    [[maybe_unused]] bool c1 = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","1"},{"dst","2"}}), msgs);
    [[maybe_unused]] bool c2 = h.registry().execute(ctx,
        makeCmd("subgraph.connect", {{"src","2"},{"dst","3"}}), msgs);

    // Declare ports.
    [[maybe_unused]] bool pi = h.registry().execute(ctx,
        makeCmd("subgraph.declare_input",  {{"port","data"},{"node","1"}}), msgs);
    [[maybe_unused]] bool po = h.registry().execute(ctx,
        makeCmd("subgraph.declare_output", {{"port","result"},{"node","3"}}), msgs);

    EXPECT_TRUE(pi);
    EXPECT_TRUE(po);

    // Validate.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx, makeCmd("subgraph.validate"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.validate ok nodes=3 edges=2 inputs=1 outputs=1"));
    }

    // Register.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx, makeCmd("subgraph.registry_add"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.registry_add ok name=linear registry_size=1"));
    }

    // Instantiate into evalGraph.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx,
            makeCmd("subgraph.instantiate", {{"prefix","run1"}}), msgs);
        EXPECT_TRUE(ok);
        // 3 template nodes cloned (input port node is its own proxy)
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.instantiate ok prefix=run1 host_nodes=3"));
        EXPECT_EQ(ctx.evalGraph.nodeCount(), 3u);
    }

    // Describe.
    msgs.clear();
    {
        bool ok = h.registry().execute(ctx, makeCmd("subgraph.describe"), msgs);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(hasPrefix(msgs, "subgraph.describe registry size=1 instances=1"));
    }
}
