// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Eval Extension — SubgraphTemplate behavior tests (Slice 2)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval/EvalGraph.h>
#include <nexus/eval_ext/Subgraph.h>

#include <gtest/gtest.h>

#include <algorithm>

namespace nexus::eval_ext::testing {

namespace {

NodePayload scalarPayload(float v)
{
    NodePayload p;
    p.value = v;
    return p;
}

float readScalar(const EvalGraph& g, NodeId id)
{
    const auto* p = g.nodeOutputPayload(id);
    if (!p) return 0.f;
    const auto* s = p->scalarF32();
    return s ? *s : 0.f;
}

} // namespace

// ── Validation ──────────────────────────────────────────────────────────────

TEST(SubgraphTemplate, EmptyTemplateValidatesAndHasNoPorts)
{
    SubgraphTemplate t;
    const auto v = t.validate();
    EXPECT_TRUE(v.ok);
    EXPECT_FALSE(v.hasCycle);
    EXPECT_TRUE(v.issues.empty());
    EXPECT_EQ(t.nodeCount(), 0u);
    EXPECT_EQ(t.edgeCount(), 0u);
    EXPECT_TRUE(t.inputPortNames().empty());
    EXPECT_TRUE(t.outputPortNames().empty());
}

TEST(SubgraphTemplate, SimpleChainValidatesOk)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    const auto b = t.addNode(NodeKind::Transform, "b");
    const auto c = t.addNode(NodeKind::Geometry, "c");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.connect(b, c));
    EXPECT_TRUE(t.declareInputPort("source", a));
    EXPECT_TRUE(t.declareOutputPort("result", c));

    const auto v = t.validate();
    EXPECT_TRUE(v.ok);
    EXPECT_FALSE(v.hasCycle);
    EXPECT_TRUE(v.issues.empty());
}

TEST(SubgraphTemplate, CycleIsRejected)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    const auto b = t.addNode(NodeKind::Transform, "b");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.connect(b, a));

    const auto v = t.validate();
    EXPECT_FALSE(v.ok);
    EXPECT_TRUE(v.hasCycle);
    ASSERT_FALSE(v.issues.empty());
    EXPECT_NE(v.issues[0].find("cycle"), std::string::npos);
}

TEST(SubgraphTemplate, InputPortMustBeSourceNode)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    const auto b = t.addNode(NodeKind::Transform, "b");
    EXPECT_TRUE(t.connect(a, b));
    // b has an incoming edge; declaring it as an input port is allowed at
    // declare-time but validate() must flag it.
    EXPECT_TRUE(t.declareInputPort("entry", b));

    const auto v = t.validate();
    EXPECT_FALSE(v.ok);
    EXPECT_FALSE(v.hasCycle);
    bool found = false;
    for (const auto& s : v.issues) {
        if (s.find("input port 'entry'") != std::string::npos &&
            s.find("incoming edges") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "expected issue about 'entry' with incoming edges";
}

TEST(SubgraphTemplate, DuplicatePortNamesAreRejectedAtDeclare)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    const auto b = t.addNode(NodeKind::Constant, "b");
    EXPECT_TRUE(t.declareInputPort("x", a));
    EXPECT_FALSE(t.declareInputPort("x", b));
    EXPECT_TRUE(t.declareOutputPort("y", a));
    EXPECT_FALSE(t.declareOutputPort("y", b));
    EXPECT_FALSE(t.declareInputPort("", a));
    EXPECT_FALSE(t.declareInputPort("z", 9999u));  // unknown local id
}

// ── Instantiation ──────────────────────────────────────────────────────────

TEST(InstantiateSubgraph, BuildsHostNodesAndEdges)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "src");
    const auto b = t.addNode(NodeKind::Transform, "xform");
    const auto c = t.addNode(NodeKind::Geometry, "out");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.connect(b, c));
    EXPECT_TRUE(t.declareInputPort("in", a));
    EXPECT_TRUE(t.declareOutputPort("out", c));

    EvalGraph g;
    auto inst = instantiateSubgraph(g, t, "instA");
    ASSERT_TRUE(inst.has_value());
    EXPECT_EQ(inst->prefix, "instA");

    // Expect 1 input proxy + 2 cloned nodes (b, c). The input-port local (a)
    // is replaced by the host proxy.
    EXPECT_EQ(inst->hostNodes.size(), 3u);
    EXPECT_EQ(g.nodeCount(), 3u);

    const NodeId proxy = subgraphInputProxy(*inst, "in");
    const NodeId out   = subgraphOutput(*inst, "out");
    ASSERT_NE(proxy, kInvalidNodeId);
    ASSERT_NE(out,   kInvalidNodeId);

    EXPECT_EQ(g.nodeKind(proxy), NodeKind::Constant);
    EXPECT_EQ(g.nodeName(proxy), "instA/in/in");
    EXPECT_EQ(g.nodeKind(out), NodeKind::Geometry);
    EXPECT_EQ(g.nodeName(out), "instA/out");

    // Topology: proxy→xform→out should be reachable via evaluate().
    auto report = g.evaluate();
    EXPECT_TRUE(report.ok);
    EXPECT_FALSE(report.hasCycle);
    EXPECT_EQ(report.evaluationOrder.size(), 3u);
}

TEST(InstantiateSubgraph, UnknownPortReturnsInvalidId)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    EXPECT_TRUE(t.declareInputPort("only", a));
    EXPECT_TRUE(t.declareOutputPort("only", a));

    EvalGraph g;
    auto inst = instantiateSubgraph(g, t, "i");
    ASSERT_TRUE(inst.has_value());

    EXPECT_EQ(subgraphInputProxy(*inst,  "nope"), kInvalidNodeId);
    EXPECT_EQ(subgraphOutput(*inst, "nope"), kInvalidNodeId);
}

TEST(InstantiateSubgraph, InvalidTemplateReturnsNullopt)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    const auto b = t.addNode(NodeKind::Transform, "b");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.connect(b, a));  // cycle

    EvalGraph g;
    auto inst = instantiateSubgraph(g, t, "bad");
    EXPECT_FALSE(inst.has_value());
    EXPECT_EQ(g.nodeCount(), 0u);  // best-effort rollback left host untouched
}

TEST(InstantiateSubgraph, TwoInstancesUseDistinctNamespaces)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "src");
    const auto b = t.addNode(NodeKind::Transform, "xform");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.declareInputPort("in", a));
    EXPECT_TRUE(t.declareOutputPort("out", b));

    EvalGraph g;
    auto i1 = instantiateSubgraph(g, t, "p1");
    auto i2 = instantiateSubgraph(g, t, "p2");
    ASSERT_TRUE(i1.has_value());
    ASSERT_TRUE(i2.has_value());

    const NodeId p1in  = subgraphInputProxy(*i1, "in");
    const NodeId p1out = subgraphOutput(*i1, "out");
    const NodeId p2in  = subgraphInputProxy(*i2, "in");
    const NodeId p2out = subgraphOutput(*i2, "out");

    EXPECT_NE(p1in,  p2in);
    EXPECT_NE(p1out, p2out);
    EXPECT_EQ(g.nodeName(p1in),  "p1/in/in");
    EXPECT_EQ(g.nodeName(p2in),  "p2/in/in");
    EXPECT_EQ(g.nodeName(p1out), "p1/xform");
    EXPECT_EQ(g.nodeName(p2out), "p2/xform");
    EXPECT_EQ(g.nodeCount(), 4u);
}

TEST(InstantiateSubgraph, InputPortRoutesValueDownstream)
{
    // Build: in → passthrough → out (Transform kind used as passthrough).
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "src");
    const auto b = t.addNode(NodeKind::Transform, "pass");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.declareInputPort("in", a));
    EXPECT_TRUE(t.declareOutputPort("out", b));

    EvalGraph g;
    auto inst = instantiateSubgraph(g, t, "rt");
    ASSERT_TRUE(inst.has_value());

    const NodeId proxy = subgraphInputProxy(*inst, "in");
    const NodeId out   = subgraphOutput(*inst, "out");

    // Register a compute callback that copies the first scalar input into the
    // node's own scalar payload.
    g.setComputeCallback([](NodeComputeContext& ctx) {
        if (!ctx.outputPayload) return true;
        for (const auto& in : ctx.inputPayloads) {
            if (in.payload && in.payload->scalarF32()) {
                ctx.outputPayload->value = *in.payload->scalarF32();
                return true;
            }
        }
        return true;
    });

    // Seed proxy with a scalar; mark downstream dirty.
    EXPECT_TRUE(g.setNodeOutputPayload(proxy, scalarPayload(3.5f)));
    g.markDirty(out);

    const auto report = g.evaluate();
    ASSERT_TRUE(report.ok);

    EXPECT_FLOAT_EQ(readScalar(g, out), 3.5f);
}

TEST(InstantiateSubgraph, ClonesPreservePayloads)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "konst");
    EXPECT_TRUE(t.setNodeOutputPayload(a, scalarPayload(7.25f)));
    EXPECT_TRUE(t.declareOutputPort("v", a));

    EvalGraph g;
    auto inst = instantiateSubgraph(g, t, "k");
    ASSERT_TRUE(inst.has_value());

    const NodeId hostId = subgraphOutput(*inst, "v");
    EXPECT_FLOAT_EQ(readScalar(g, hostId), 7.25f);
}

TEST(InstantiateSubgraph, DeterministicTopoOrderAcrossInstantiations)
{
    auto build = []() {
        SubgraphTemplate t;
        const auto a = t.addNode(NodeKind::Constant,  "a");
        const auto b = t.addNode(NodeKind::Transform, "b");
        const auto c = t.addNode(NodeKind::Geometry,  "c");
        const auto d = t.addNode(NodeKind::Merge,     "d");
        EXPECT_TRUE(t.connect(a, b));
        EXPECT_TRUE(t.connect(a, c));
        EXPECT_TRUE(t.connect(b, d));
        EXPECT_TRUE(t.connect(c, d));
        EXPECT_TRUE(t.declareInputPort("x", a));
        EXPECT_TRUE(t.declareOutputPort("y", d));
        return t;
    };

    EvalGraph g1, g2;
    auto i1 = instantiateSubgraph(g1, build(), "x");
    auto i2 = instantiateSubgraph(g2, build(), "x");
    ASSERT_TRUE(i1.has_value());
    ASSERT_TRUE(i2.has_value());

    const auto r1 = g1.evaluate();
    const auto r2 = g2.evaluate();
    ASSERT_TRUE(r1.ok);
    ASSERT_TRUE(r2.ok);

    // Map evaluation order to (kind, name) tuples to compare cross-host.
    auto fingerprint = [](const EvalGraph& g, const std::vector<NodeId>& order) {
        std::vector<std::string> fp;
        fp.reserve(order.size());
        for (NodeId id : order) {
            fp.push_back(std::to_string(int(g.nodeKind(id))) + ":" + g.nodeName(id));
        }
        return fp;
    };
    EXPECT_EQ(fingerprint(g1, r1.evaluationOrder),
              fingerprint(g2, r2.evaluationOrder));
}

TEST(InstantiateSubgraph, HostRemovalCleansUpInstanceNodes)
{
    SubgraphTemplate t;
    const auto a = t.addNode(NodeKind::Constant, "a");
    const auto b = t.addNode(NodeKind::Transform, "b");
    EXPECT_TRUE(t.connect(a, b));
    EXPECT_TRUE(t.declareInputPort("in", a));
    EXPECT_TRUE(t.declareOutputPort("out", b));

    EvalGraph g;
    auto inst = instantiateSubgraph(g, t, "z");
    ASSERT_TRUE(inst.has_value());
    EXPECT_EQ(g.nodeCount(), 2u);

    for (NodeId hid : inst->hostNodes) {
        EXPECT_TRUE(g.removeNode(hid));
    }
    EXPECT_EQ(g.nodeCount(), 0u);
}

} // namespace nexus::eval_ext::testing
