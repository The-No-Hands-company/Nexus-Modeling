#include <gtest/gtest.h>
#include "nexus/scene/NodeScene.h"

using namespace nexus;

// ── Node management ───────────────────────────────────────────────────────────

TEST(NodeScene, EmptySceneHasZeroNodes) {
    NodeScene s;
    EXPECT_EQ(s.nodeCount(), 0u);
}

TEST(NodeScene, AddNodeReturnsStableId) {
    NodeScene s;
    SceneNodeId id = s.addNode("geo", NodeKind::Geometry);
    EXPECT_NE(id, kInvalidSceneNodeId);
    EXPECT_TRUE(s.hasNode(id));
    EXPECT_EQ(s.nodeCount(), 1u);
}

TEST(NodeScene, AddDuplicateNameReturnsInvalidId) {
    NodeScene s;
    s.addNode("geo", NodeKind::Geometry);
    EXPECT_EQ(s.addNode("geo", NodeKind::Transform), kInvalidSceneNodeId);
    EXPECT_EQ(s.nodeCount(), 1u);
}

TEST(NodeScene, NodeByNameReturnsCorrectId) {
    NodeScene s;
    SceneNodeId id = s.addNode("anim", NodeKind::Animation);
    EXPECT_EQ(s.nodeByName("anim"), id);
}

TEST(NodeScene, NodeByNameReturnInvalidForAbsentName) {
    NodeScene s;
    EXPECT_EQ(s.nodeByName("missing"), kInvalidSceneNodeId);
}

TEST(NodeScene, NodeKindAndNameRoundTrip) {
    NodeScene s;
    SceneNodeId id = s.addNode("tx", NodeKind::Transform);
    EXPECT_EQ(s.nodeKind(id), NodeKind::Transform);
    EXPECT_EQ(s.nodeName(id), "tx");
}

TEST(NodeScene, RemoveNodeDecreasesCountAndClearsName) {
    NodeScene s;
    SceneNodeId id = s.addNode("geo", NodeKind::Geometry);
    ASSERT_TRUE(s.removeNode(id));
    EXPECT_EQ(s.nodeCount(), 0u);
    EXPECT_FALSE(s.hasNode(id));
    EXPECT_EQ(s.nodeByName("geo"), kInvalidSceneNodeId);
}

TEST(NodeScene, RemoveUnknownNodeReturnsFalse) {
    NodeScene s;
    EXPECT_FALSE(s.removeNode(kInvalidSceneNodeId));
    EXPECT_FALSE(s.removeNode(999u));
}

TEST(NodeScene, ClearRemovesAllNodesAndNames) {
    NodeScene s;
    s.addNode("a", NodeKind::Constant);
    s.addNode("b", NodeKind::Geometry);
    s.clear();
    EXPECT_EQ(s.nodeCount(), 0u);
    EXPECT_EQ(s.nodeByName("a"), kInvalidSceneNodeId);
    EXPECT_EQ(s.nodeByName("b"), kInvalidSceneNodeId);
}

TEST(NodeScene, RemovedNameCanBeReused) {
    NodeScene s;
    SceneNodeId id1 = s.addNode("node", NodeKind::Constant);
    ASSERT_TRUE(s.removeNode(id1));
    SceneNodeId id2 = s.addNode("node", NodeKind::Geometry);
    EXPECT_NE(id2, kInvalidSceneNodeId);
    EXPECT_NE(id2, id1);
    EXPECT_EQ(s.nodeByName("node"), id2);
}

// ── Dependency edges ──────────────────────────────────────────────────────────

TEST(NodeScene, ConnectAndIsConnected) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    ASSERT_TRUE(s.connect(a, b));
    EXPECT_TRUE(s.isConnected(a, b));
    EXPECT_FALSE(s.isConnected(b, a));
}

TEST(NodeScene, ConnectUnknownNodeReturnsFalse) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_FALSE(s.connect(a, 999u));
    EXPECT_FALSE(s.connect(999u, a));
}

TEST(NodeScene, DisconnectRemovesEdge) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    ASSERT_TRUE(s.connect(a, b));
    ASSERT_TRUE(s.disconnect(a, b));
    EXPECT_FALSE(s.isConnected(a, b));
}

// ── Asset slots ───────────────────────────────────────────────────────────────

TEST(NodeScene, SetAndGetAsset) {
    NodeScene s;
    SceneNodeId id = s.addNode("src", NodeKind::Constant);

    NodePayload p;
    p.value = 42.0f;
    ASSERT_TRUE(s.setAsset(id, p));

    const NodePayload* a = s.asset(id);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), NodePayloadType::ScalarF32);
    ASSERT_NE(a->scalarF32(), nullptr);
    EXPECT_FLOAT_EQ(*a->scalarF32(), 42.0f);
}

TEST(NodeScene, AssetUnknownNodeReturnsNullptr) {
    NodeScene s;
    EXPECT_EQ(s.asset(999u), nullptr);
}

TEST(NodeScene, SetAssetUnknownNodeReturnsFalse) {
    NodeScene s;
    NodePayload p;
    p.value = 1.0f;
    EXPECT_FALSE(s.setAsset(999u, p));
}

TEST(NodeScene, ClearAssetResetsToNone) {
    NodeScene s;
    SceneNodeId id = s.addNode("n", NodeKind::Geometry);
    NodePayload p;
    p.value = std::string("data");
    ASSERT_TRUE(s.setAsset(id, p));
    ASSERT_TRUE(s.clearAsset(id));
    const NodePayload* a = s.asset(id);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), NodePayloadType::None);
}

TEST(NodeScene, AssetPersistedAfterRemoveAndReaddWithSameName) {
    NodeScene s;
    SceneNodeId id1 = s.addNode("n", NodeKind::Constant);
    NodePayload p;
    p.value = 7.0f;
    ASSERT_TRUE(s.setAsset(id1, p));
    ASSERT_TRUE(s.removeNode(id1));

    // Re-add same name — must get fresh slot, not stale payload.
    SceneNodeId id2 = s.addNode("n", NodeKind::Constant);
    ASSERT_NE(id2, kInvalidSceneNodeId);
    const NodePayload* a = s.asset(id2);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), NodePayloadType::None);
}

// ── Evaluation via internal EvalGraph ────────────────────────────────────────

TEST(NodeScene, EvaluateEmptySceneSucceeds) {
    NodeScene s;
    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_FALSE(r.hasCycle);
    EXPECT_TRUE(r.evaluationOrder.empty());
}

TEST(NodeScene, EvaluateProducesDependencyFirstOrder) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Transform);
    ASSERT_TRUE(s.connect(a, b));

    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);
    ASSERT_EQ(r.evaluationOrder.size(), 2u);
    auto posA = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), a);
    auto posB = std::find(r.evaluationOrder.begin(), r.evaluationOrder.end(), b);
    EXPECT_LT(posA, posB);
}

TEST(NodeScene, EvaluateWithCycleReportsFailure) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Geometry);
    SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    ASSERT_TRUE(s.connect(a, b));
    ASSERT_TRUE(s.connect(b, a));
    const auto r = s.evaluate();
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.hasCycle);
}

TEST(NodeScene, CallbackReceivesNodeContextWithCorrectId) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    SceneNodeId b = s.addNode("b", NodeKind::Transform);
    ASSERT_TRUE(s.connect(a, b));

    std::vector<SceneNodeId> fired;
    s.setComputeCallback([&](SceneNodeId id, NodeKind, const std::string&) {
        fired.push_back(id);
        return true;
    });

    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(fired, r.evaluationOrder);
}

TEST(NodeScene, AssetFlowsThroughCallbackContext) {
    NodeScene s;
    SceneNodeId src = s.addNode("src", NodeKind::Constant);
    SceneNodeId dst = s.addNode("dst", NodeKind::Transform);
    ASSERT_TRUE(s.connect(src, dst));

    NodePayload seed;
    seed.value = 3.0f;
    ASSERT_TRUE(s.setAsset(src, seed));

    s.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
        if (ctx.id == dst && ctx.outputPayload) {
            if (!ctx.inputPayloads.empty() && ctx.inputPayloads[0].payload) {
                const float* v = ctx.inputPayloads[0].payload->scalarF32();
                if (v) ctx.outputPayload->value = *v * 10.0f;
            }
        }
        return true;
    });

    const auto r = s.evaluate();
    EXPECT_TRUE(r.ok);

    const NodePayload* out = s.asset(dst);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->type(), NodePayloadType::ScalarF32);
    EXPECT_FLOAT_EQ(*out->scalarF32(), 30.0f);
}

TEST(NodeScene, EvalGraphAccessorReflectsSceneState) {
    NodeScene s;
    SceneNodeId a = s.addNode("a", NodeKind::Constant);
    EXPECT_TRUE(s.evalGraph().hasNode(a));
    EXPECT_EQ(s.evalGraph().nodeCount(), 1u);
}
