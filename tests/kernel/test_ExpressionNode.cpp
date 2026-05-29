// Tests for nexus::eval::ExpressionNodeAdapter (Slice 4).
//
// Verifies that a compiled nexus::script::Expression can be embedded as a
// NodeKind::Expression node in an EvalGraph, with its free variables resolved
// from upstream ScalarF32 payload slots.

#include <nexus/eval/EvalGraph.h>
#include <nexus/eval/ExpressionNode.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus;
using namespace nexus::eval;

// ── Helpers ──────────────────────────────────────────────────────────────────

static void setScalar(EvalGraph& g, NodeId id, float v) {
    NodePayload p;
    p.value = v;
    g.setNodeOutputPayload(id, p);
}

static float scalarOf(const EvalGraph& g, NodeId id) {
    const NodePayload* p = g.nodeOutputPayload(id);
    EXPECT_NE(p, nullptr);
    if (!p) return 0.0f;
    const float* f = p->scalarF32();
    EXPECT_NE(f, nullptr);
    if (!f) return 0.0f;
    return *f;
}

static EvalGraph::NodeComputeFn makeDispatch(const ExpressionNodeAdapter& adapter) {
    return [&adapter](NodeComputeContext& ctx) -> bool {
        if (ctx.kind == NodeKind::Expression && ctx.id == adapter.nodeId())
            return adapter.compute(ctx);
        return true;
    };
}

// ── Creation ─────────────────────────────────────────────────────────────────

TEST(ExpressionNode, CreateSucceeds) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");

    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x + 1", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());
    EXPECT_NE(adapter->nodeId(), kInvalidNodeId);
    EXPECT_TRUE(g.hasNode(adapter->nodeId()));
    EXPECT_EQ(g.nodeKind(adapter->nodeId()), NodeKind::Expression);
    EXPECT_TRUE(g.isConnected(x, adapter->nodeId()));
}

TEST(ExpressionNode, CompileFailureReturnsEmpty) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    auto adapter = ExpressionNodeAdapter::create(g, "bad", "x ++ 1 @@", {{"x", x}});
    EXPECT_FALSE(adapter.has_value());
    EXPECT_EQ(g.nodeCount(), 1u);
}

TEST(ExpressionNode, UnknownSourceNodeReturnsEmpty) {
    EvalGraph g;
    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x * 2", {{"x", 99u}});
    EXPECT_FALSE(adapter.has_value());
    EXPECT_EQ(g.nodeCount(), 0u);
}

TEST(ExpressionNode, EmptyVariableNameReturnsEmpty) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x", {{"", x}});
    EXPECT_FALSE(adapter.has_value());
}

TEST(ExpressionNode, NoBindingsConstantExpression) {
    EvalGraph g;
    auto adapter = ExpressionNodeAdapter::create(g, "const", "pi * 2", {});
    ASSERT_TRUE(adapter.has_value());
    EXPECT_TRUE(adapter->bindings().empty());
}

// ── Accessors ────────────────────────────────────────────────────────────────

TEST(ExpressionNode, AccessorsAreConsistent) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Constant, "b");

    auto adapter = ExpressionNodeAdapter::create(
        g, "sum", "a + b", {{"a", a}, {"b", b}});
    ASSERT_TRUE(adapter.has_value());

    EXPECT_EQ(adapter->expression().source(), "a + b");
    EXPECT_EQ(adapter->bindings().size(), 2u);
    EXPECT_EQ(g.nodeName(adapter->nodeId()), "sum");
}

// ── Evaluate via EvalGraph ───────────────────────────────────────────────────

TEST(ExpressionNode, EvaluateSingleVariable) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    setScalar(g, x, 3.0f);

    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x * 2 + 1", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());

    g.setComputeCallback(makeDispatch(*adapter));
    auto report = g.evaluate();
    EXPECT_TRUE(report.ok);

    EXPECT_NEAR(scalarOf(g, adapter->nodeId()), 7.0f, 1e-5f);
}

TEST(ExpressionNode, EvaluateMultiVariable) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Constant, "b");
    setScalar(g, a, 4.0f);
    setScalar(g, b, 3.0f);

    auto adapter = ExpressionNodeAdapter::create(
        g, "hyp", "sqrt(a^2 + b^2)", {{"a", a}, {"b", b}});
    ASSERT_TRUE(adapter.has_value());

    g.setComputeCallback(makeDispatch(*adapter));
    g.evaluate();

    EXPECT_NEAR(scalarOf(g, adapter->nodeId()), 5.0f, 1e-5f);
}

TEST(ExpressionNode, ConstantExpressionNoBindings) {
    EvalGraph g;
    auto adapter = ExpressionNodeAdapter::create(g, "pi2", "pi * 2", {});
    ASSERT_TRUE(adapter.has_value());

    g.setComputeCallback(makeDispatch(*adapter));
    g.evaluate();

    constexpr float kPi2 = 6.2831853f;
    EXPECT_NEAR(scalarOf(g, adapter->nodeId()), kPi2, 1e-4f);
}

TEST(ExpressionNode, ExpressionNodeFeedsDownstream) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    setScalar(g, x, 2.0f);

    auto adapter = ExpressionNodeAdapter::create(g, "double", "x * 2", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());

    NodeId down = g.addNode(NodeKind::Constant, "downstream");
    g.connect(adapter->nodeId(), down);

    NodeId exprId = adapter->nodeId();
    g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.kind == NodeKind::Expression && ctx.id == exprId)
            return adapter->compute(ctx);
        if (ctx.id == down) {
            if (ctx.inputPayloads.empty() || !ctx.inputPayloads[0].payload) return false;
            const float* f = ctx.inputPayloads[0].payload->scalarF32();
            if (!f) return false;
            ctx.outputPayload->value = *f * 2.0f;
        }
        return true;
    });
    g.evaluate();

    EXPECT_NEAR(scalarOf(g, exprId), 4.0f, 1e-5f);
    EXPECT_NEAR(scalarOf(g, down),   8.0f, 1e-5f);
}

// ── Error paths ──────────────────────────────────────────────────────────────

TEST(ExpressionNode, MissingUpstreamPayloadReturnsFalse) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    // No payload set on x.

    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x + 1", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());

    bool computeOk = true;
    g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.kind == NodeKind::Expression) {
            computeOk = adapter->compute(ctx);
            return computeOk;
        }
        return true;
    });
    auto report = g.evaluate();

    EXPECT_FALSE(computeOk);
    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(report.executionFailed);
}

TEST(ExpressionNode, DomainErrorReturnsFalse) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    setScalar(g, x, -1.0f);

    auto adapter = ExpressionNodeAdapter::create(g, "expr", "sqrt(x)", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());

    bool computeOk = true;
    g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.kind == NodeKind::Expression) {
            computeOk = adapter->compute(ctx);
            return computeOk;
        }
        return true;
    });
    auto report = g.evaluate();

    EXPECT_FALSE(computeOk);
    EXPECT_FALSE(report.ok);
}

TEST(ExpressionNode, UpstreamNonScalarPayloadReturnsFalse) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    NodePayload p;
    p.value = std::string("hello");
    g.setNodeOutputPayload(x, p);

    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());

    bool computeOk = true;
    g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.kind == NodeKind::Expression) {
            computeOk = adapter->compute(ctx);
            return computeOk;
        }
        return true;
    });
    auto report = g.evaluate();

    EXPECT_FALSE(computeOk);
    EXPECT_FALSE(report.ok);
}

// ── Determinism ──────────────────────────────────────────────────────────────

TEST(ExpressionNode, DeterministicAcrossTwoGraphs) {
    auto run = [](float xVal) -> float {
        EvalGraph g;
        NodeId x = g.addNode(NodeKind::Constant, "x");
        NodePayload p; p.value = xVal;
        g.setNodeOutputPayload(x, p);
        auto adapter = ExpressionNodeAdapter::create(
            g, "expr", "sin(x) * cos(x)", {{"x", x}});
        g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
            if (ctx.kind == NodeKind::Expression) return adapter->compute(ctx);
            return true;
        });
        g.evaluate();
        return scalarOf(g, adapter->nodeId());
    };

    EXPECT_EQ(run(1.0f), run(1.0f));
}

TEST(ExpressionNode, ReEvaluationAfterUpstreamChange) {
    EvalGraph g;
    NodeId x = g.addNode(NodeKind::Constant, "x");
    setScalar(g, x, 2.0f);

    auto adapter = ExpressionNodeAdapter::create(g, "expr", "x * x", {{"x", x}});
    ASSERT_TRUE(adapter.has_value());
    g.setComputeCallback(makeDispatch(*adapter));

    g.evaluate();
    EXPECT_NEAR(scalarOf(g, adapter->nodeId()), 4.0f, 1e-5f);

    setScalar(g, x, 5.0f);
    g.markDirty(x);
    g.evaluate();
    EXPECT_NEAR(scalarOf(g, adapter->nodeId()), 25.0f, 1e-5f);
}

// ── NodeKind::Expression ──────────────────────────────────────────────────────

TEST(ExpressionNode, NodeKindExpressionIsRecognized) {
    EvalGraph g;
    NodeId id = g.addNode(NodeKind::Expression, "test");
    EXPECT_EQ(g.nodeKind(id), NodeKind::Expression);
}
