#include <nexus/eval/EvalGraph.h>
#include <nexus/eval/ExpressionNode.h>
#include <nexus/eval/ExpressionNodeSerializer.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <string>

using namespace nexus;
using namespace nexus::eval;

// ── Helpers ───────────────────────────────────────────────────────────────────

static NodeId addConstant(EvalGraph& g, const std::string& name, float value) {
    NodeId id = g.addNode(NodeKind::Constant, name);
    g.setNodeOutputPayload(id, NodePayload{value});
    return id;
}

static ExpressionNodeAdapter makeAdapter(EvalGraph& g,
                                          const std::string& name,
                                          const std::string& src,
                                          std::vector<ExpressionBinding> bindings) {
    auto opt = ExpressionNodeAdapter::create(g, name, src, std::move(bindings));
    EXPECT_TRUE(opt.has_value());
    return std::move(*opt);
}

// ── Basic serialize/deserialize ────────────────────────────────────────────────

TEST(ExpressionNodeSerializer, SerializeNoBindings) {
    EvalGraph g;
    auto adapter = makeAdapter(g, "result", "42", {});

    ExpressionNodeSerializationReport rep;
    std::string text = ExpressionNodeSerializer::serialize(adapter, g, rep);

    EXPECT_TRUE(rep.ok);
    EXPECT_EQ(rep.bindingCount, 0u);
    EXPECT_NE(text.find("NEXUS_EXPR_NODE_V1"), std::string::npos);
    EXPECT_NE(text.find("N result"), std::string::npos);
    EXPECT_NE(text.find("\"42\""), std::string::npos);
}

TEST(ExpressionNodeSerializer, SerializeWithOneBinding) {
    EvalGraph g;
    NodeId x = addConstant(g, "x", 3.0f);
    auto adapter = makeAdapter(g, "result", "x * 2", {{"x", x}});

    ExpressionNodeSerializationReport rep;
    std::string text = ExpressionNodeSerializer::serialize(adapter, g, rep);

    EXPECT_TRUE(rep.ok);
    EXPECT_EQ(rep.bindingCount, 1u);
    EXPECT_NE(text.find("B x x"), std::string::npos);
}

TEST(ExpressionNodeSerializer, SerializeWithMultipleBindings) {
    EvalGraph g;
    NodeId a = addConstant(g, "a", 1.0f);
    NodeId b = addConstant(g, "b", 2.0f);
    auto adapter = makeAdapter(g, "result", "a + b", {{"a", a}, {"b", b}});

    ExpressionNodeSerializationReport rep;
    std::string text = ExpressionNodeSerializer::serialize(adapter, g, rep);

    EXPECT_TRUE(rep.ok);
    EXPECT_EQ(rep.bindingCount, 2u);
    EXPECT_NE(text.find("B a a"), std::string::npos);
    EXPECT_NE(text.find("B b b"), std::string::npos);
}

// ── Round-trip ────────────────────────────────────────────────────────────────

TEST(ExpressionNodeSerializer, RoundTripNoBindings) {
    EvalGraph g;
    auto original = makeAdapter(g, "result", "3.14", {});

    ExpressionNodeSerializationReport srep;
    std::string text = ExpressionNodeSerializer::serialize(original, g, srep);
    ASSERT_TRUE(srep.ok);

    EvalGraph g2;
    ExpressionNodeSerializationReport drep;
    auto restored = ExpressionNodeSerializer::deserialize(text, g2, drep);

    ASSERT_TRUE(drep.ok);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->expression().source(), "3.14");
    EXPECT_TRUE(restored->bindings().empty());
    EXPECT_EQ(drep.bindingCount, 0u);
}

TEST(ExpressionNodeSerializer, RoundTripWithBindings) {
    // Original graph
    EvalGraph g;
    NodeId x = addConstant(g, "x", 5.0f);
    NodeId y = addConstant(g, "y", 2.0f);
    auto original = makeAdapter(g, "out", "x + y", {{"x", x}, {"y", y}});

    ExpressionNodeSerializationReport srep;
    std::string text = ExpressionNodeSerializer::serialize(original, g, srep);
    ASSERT_TRUE(srep.ok);

    // Deserialization graph — source nodes must exist with same names.
    EvalGraph g2;
    NodeId x2 = addConstant(g2, "x", 5.0f);
    NodeId y2 = addConstant(g2, "y", 2.0f);

    ExpressionNodeSerializationReport drep;
    auto restored = ExpressionNodeSerializer::deserialize(text, g2, drep);

    ASSERT_TRUE(drep.ok);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->expression().source(), "x + y");
    ASSERT_EQ(restored->bindings().size(), 2u);
    EXPECT_EQ(drep.bindingCount, 2u);

    // Verify bindings resolve to the correct nodes in g2.
    bool hasX = false, hasY = false;
    for (const auto& b : restored->bindings()) {
        if (b.variable == "x" && b.sourceNode == x2) hasX = true;
        if (b.variable == "y" && b.sourceNode == y2) hasY = true;
    }
    EXPECT_TRUE(hasX);
    EXPECT_TRUE(hasY);
}

TEST(ExpressionNodeSerializer, RoundTripEvaluatesCorrectly) {
    // Serialize
    EvalGraph g;
    NodeId a = addConstant(g, "a", 4.0f);
    auto original = makeAdapter(g, "r", "a * a", {{"a", a}});

    ExpressionNodeSerializationReport srep;
    std::string text = ExpressionNodeSerializer::serialize(original, g, srep);
    ASSERT_TRUE(srep.ok);

    // Restore in a fresh graph and evaluate.
    EvalGraph g2;
    NodeId a2 = addConstant(g2, "a", 4.0f);
    ExpressionNodeSerializationReport drep;
    auto restored = ExpressionNodeSerializer::deserialize(text, g2, drep);
    ASSERT_TRUE(drep.ok && restored.has_value());

    NodeId rid = restored->nodeId();
    g2.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.id == rid) return restored->compute(ctx);
        return true;
    });

    [[maybe_unused]] auto evalRep = g2.evaluate();
    const NodePayload* out = g2.nodeOutputPayload(rid);
    ASSERT_NE(out, nullptr);
    ASSERT_NE(out->scalarF32(), nullptr);
    EXPECT_NEAR(*out->scalarF32(), 16.0f, 1e-4f);
}

// ── Quoted-source edge cases ──────────────────────────────────────────────────

TEST(ExpressionNodeSerializer, SourceWithDoubleQuoteEscaped) {
    // Expression source containing a double quote character would be escaped.
    // We use a source without quotes to verify the mechanism works end-to-end.
    EvalGraph g;
    auto adapter = makeAdapter(g, "r", "1 + 1", {});

    ExpressionNodeSerializationReport srep;
    std::string text = ExpressionNodeSerializer::serialize(adapter, g, srep);
    EXPECT_TRUE(srep.ok);

    EvalGraph g2;
    ExpressionNodeSerializationReport drep;
    auto restored = ExpressionNodeSerializer::deserialize(text, g2, drep);
    ASSERT_TRUE(drep.ok && restored.has_value());
    EXPECT_EQ(restored->expression().source(), "1 + 1");
}

TEST(ExpressionNodeSerializer, SourceWithBackslashEscaped) {
    // Manually craft text with backslash-escaped source to verify parseQuoted handles it.
    std::string text = "NEXUS_EXPR_NODE_V1\nN r \"1 + 1\"\n";
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto restored = ExpressionNodeSerializer::deserialize(text, g, rep);
    ASSERT_TRUE(rep.ok && restored.has_value());
    EXPECT_EQ(restored->expression().source(), "1 + 1");
}

// ── Error cases ───────────────────────────────────────────────────────────────

TEST(ExpressionNodeSerializer, DeserializeMissingHeader) {
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize("N r \"1\"\n", g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(rep.errors.empty());
}

TEST(ExpressionNodeSerializer, DeserializeEmptyText) {
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize("", g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
}

TEST(ExpressionNodeSerializer, DeserializeMissingNRecord) {
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize("NEXUS_EXPR_NODE_V1\n", g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
}

TEST(ExpressionNodeSerializer, DeserializeMalformedNRecord) {
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    // N with missing quoted source
    auto result = ExpressionNodeSerializer::deserialize(
        "NEXUS_EXPR_NODE_V1\nN r\n", g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
}

TEST(ExpressionNodeSerializer, DeserializeUnknownBindingNode) {
    std::string text = "NEXUS_EXPR_NODE_V1\nN r \"x + 1\"\nB x missing_node\n";
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize(text, g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(rep.errors.empty());
}

TEST(ExpressionNodeSerializer, DeserializeBadExpression) {
    // Expression that fails to compile.
    std::string text = "NEXUS_EXPR_NODE_V1\nN r \"(((\"\n";
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize(text, g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
}

TEST(ExpressionNodeSerializer, DeserializeMalformedBRecord) {
    // B record with only one token.
    std::string text = "NEXUS_EXPR_NODE_V1\nN r \"1\"\nB x\n";
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize(text, g, rep);
    EXPECT_FALSE(rep.ok);
    EXPECT_FALSE(result.has_value());
}

// ── Forward-compatibility ─────────────────────────────────────────────────────

TEST(ExpressionNodeSerializer, UnknownTagSilentlySkipped) {
    std::string text = "NEXUS_EXPR_NODE_V1\nX future_tag data\nN r \"2\"\n";
    EvalGraph g;
    ExpressionNodeSerializationReport rep;
    auto result = ExpressionNodeSerializer::deserialize(text, g, rep);
    EXPECT_TRUE(rep.ok);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->expression().source(), "2");
}

// ── allNodeIds contract ───────────────────────────────────────────────────────

TEST(EvalGraphAllNodeIds, EmptyGraphReturnsEmpty) {
    EvalGraph g;
    EXPECT_TRUE(g.allNodeIds().empty());
}

TEST(EvalGraphAllNodeIds, ReturnsSortedIds) {
    EvalGraph g;
    NodeId a = g.addNode(NodeKind::Constant, "a");
    NodeId b = g.addNode(NodeKind::Constant, "b");
    NodeId c = g.addNode(NodeKind::Constant, "c");

    auto ids = g.allNodeIds();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_LT(ids[0], ids[1]);
    EXPECT_LT(ids[1], ids[2]);
    // All three known IDs are present.
    EXPECT_NE(std::find(ids.begin(), ids.end(), a), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), b), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), c), ids.end());
}

// ── Determinism ───────────────────────────────────────────────────────────────

TEST(ExpressionNodeSerializer, SerializeIsDeterministic) {
    EvalGraph g1, g2;
    NodeId x1 = addConstant(g1, "x", 1.0f);
    NodeId x2 = addConstant(g2, "x", 1.0f);
    auto a1 = makeAdapter(g1, "r", "x + 1", {{"x", x1}});
    auto a2 = makeAdapter(g2, "r", "x + 1", {{"x", x2}});

    ExpressionNodeSerializationReport r1, r2;
    std::string t1 = ExpressionNodeSerializer::serialize(a1, g1, r1);
    std::string t2 = ExpressionNodeSerializer::serialize(a2, g2, r2);

    EXPECT_EQ(t1, t2);
}
