// Tests for NodeSceneSerializer (NodeScene Slice 5).
//
// Covers:
//   - Empty scene round-trip.
//   - Nodes only: kind and name preserved.
//   - All NodeKind variants round-trip.
//   - Data-flow edges preserved.
//   - Parent-child hierarchy preserved.
//   - Asset payloads: ScalarF32, IntegerI64, Boolean, ReconstructionDiagnostic,
//     SimTransform.
//   - Float precision preserved (~1e-6).
//   - Nodes with no asset omit the A record.
//   - allNodeIds() returns sorted ascending ids.
//   - outgoingEdges() returns sorted ascending ids.
//   - Invalid header rejected.
//   - Malformed N record rejected.
//   - Malformed E record referencing unknown node rejected.
//   - Unknown top-level tag silently skipped (forward-compatible).
//   - nodeCount and edgeCount tracked in report.
//   - Node name with spaces causes serialize error.

#include <nexus/scene/NodeSceneSerializer.h>
#include <nexus/eval/EvalGraph.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus;

// ── Empty scene ───────────────────────────────────────────────────────────────

TEST(NodeSceneSerializer, EmptySceneRoundTrip)
{
    NodeScene s;
    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.nodeCount, 0u);
    EXPECT_EQ(r.edgeCount, 0u);
    EXPECT_FALSE(data.empty());

    NodeScene s2;
    const auto r2 = NodeSceneSerializer::deserialize(data, s2);
    EXPECT_TRUE(r2.ok);
    EXPECT_EQ(s2.nodeCount(), 0u);
}

// ── Node round-trip ───────────────────────────────────────────────────────────

TEST(NodeSceneSerializer, SingleNodeRoundTrip)
{
    NodeScene s;
    s.addNode("mesh_a", NodeKind::Geometry);

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.nodeCount, 1u);

    NodeScene s2;
    const auto r2 = NodeSceneSerializer::deserialize(data, s2);
    ASSERT_TRUE(r2.ok);
    ASSERT_EQ(s2.nodeCount(), 1u);
    const SceneNodeId id = s2.nodeByName("mesh_a");
    EXPECT_NE(id, kInvalidSceneNodeId);
    EXPECT_EQ(s2.nodeKind(id), NodeKind::Geometry);
}

TEST(NodeSceneSerializer, AllNodeKindsRoundTrip)
{
    const std::vector<std::pair<std::string, NodeKind>> kinds = {
        {"geo",    NodeKind::Geometry},
        {"anim",   NodeKind::Animation},
        {"xform",  NodeKind::Transform},
        {"merge",  NodeKind::Merge},
        {"proxy",  NodeKind::ProxyGeometry},
        {"recon",  NodeKind::Reconstruction},
        {"cst",    NodeKind::Constant},
        {"expr",   NodeKind::Expression},
    };

    NodeScene s;
    for (const auto& [name, kind] : kinds) s.addNode(name, kind);

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.nodeCount, kinds.size());

    NodeScene s2;
    ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);
    ASSERT_EQ(s2.nodeCount(), kinds.size());
    for (const auto& [name, kind] : kinds) {
        const SceneNodeId id = s2.nodeByName(name);
        EXPECT_NE(id, kInvalidSceneNodeId) << name;
        EXPECT_EQ(s2.nodeKind(id), kind)   << name;
    }
}

// ── Edge round-trip ───────────────────────────────────────────────────────────

TEST(NodeSceneSerializer, EdgesRoundTrip)
{
    NodeScene s;
    const SceneNodeId a = s.addNode("a", NodeKind::Constant);
    const SceneNodeId b = s.addNode("b", NodeKind::Geometry);
    const SceneNodeId c = s.addNode("c", NodeKind::Merge);
    ASSERT_TRUE(s.connect(a, b));
    ASSERT_TRUE(s.connect(a, c));
    ASSERT_TRUE(s.connect(b, c));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.edgeCount, 3u);

    NodeScene s2;
    const auto r2 = NodeSceneSerializer::deserialize(data, s2);
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(r2.edgeCount, 3u);

    const SceneNodeId a2 = s2.nodeByName("a");
    const SceneNodeId b2 = s2.nodeByName("b");
    const SceneNodeId c2 = s2.nodeByName("c");
    EXPECT_TRUE(s2.isConnected(a2, b2));
    EXPECT_TRUE(s2.isConnected(a2, c2));
    EXPECT_TRUE(s2.isConnected(b2, c2));
    EXPECT_FALSE(s2.isConnected(b2, a2));
}

// ── Hierarchy round-trip ──────────────────────────────────────────────────────

TEST(NodeSceneSerializer, HierarchyRoundTrip)
{
    NodeScene s;
    const SceneNodeId root  = s.addNode("root",  NodeKind::Geometry);
    const SceneNodeId child = s.addNode("child", NodeKind::Geometry);
    const SceneNodeId grand = s.addNode("grand", NodeKind::Geometry);
    ASSERT_TRUE(s.setParent(child, root));
    ASSERT_TRUE(s.setParent(grand, child));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);

    NodeScene s2;
    ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);

    const SceneNodeId r2 = s2.nodeByName("root");
    const SceneNodeId c2 = s2.nodeByName("child");
    const SceneNodeId g2 = s2.nodeByName("grand");
    EXPECT_EQ(s2.parent(c2), r2);
    EXPECT_EQ(s2.parent(g2), c2);
    EXPECT_EQ(s2.parent(r2), kInvalidSceneNodeId);
}

// ── Asset payloads ────────────────────────────────────────────────────────────

TEST(NodeSceneSerializer, ScalarF32AssetRoundTrip)
{
    NodeScene s;
    const SceneNodeId id = s.addNode("n", NodeKind::Constant);
    NodePayload p; p.value = 3.14f;
    ASSERT_TRUE(s.setAsset(id, p));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);

    NodeScene s2;
    ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);
    const SceneNodeId id2 = s2.nodeByName("n");
    const NodePayload* a = s2.asset(id2);
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->type(), NodePayloadType::ScalarF32);
    EXPECT_NEAR(*a->scalarF32(), 3.14f, 1e-5f);
}

TEST(NodeSceneSerializer, IntegerI64AssetRoundTrip)
{
    NodeScene s;
    const SceneNodeId id = s.addNode("n", NodeKind::Constant);
    NodePayload p; p.value = int64_t{-42};
    ASSERT_TRUE(s.setAsset(id, p));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);

    NodeScene s2;
    ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);
    const NodePayload* a = s2.asset(s2.nodeByName("n"));
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->type(), NodePayloadType::IntegerI64);
    EXPECT_EQ(*a->integerI64(), -42);
}

TEST(NodeSceneSerializer, BooleanAssetRoundTrip)
{
    for (bool val : {false, true}) {
        NodeScene s;
        const SceneNodeId id = s.addNode("flag", NodeKind::Constant);
        NodePayload p; p.value = val;
        ASSERT_TRUE(s.setAsset(id, p));

        NodeSceneSerializationReport r;
        const std::string data = NodeSceneSerializer::serialize(s, r);
        ASSERT_TRUE(r.ok);

        NodeScene s2;
        ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);
        const NodePayload* a = s2.asset(s2.nodeByName("flag"));
        ASSERT_NE(a, nullptr);
        ASSERT_EQ(a->type(), NodePayloadType::Boolean);
        EXPECT_EQ(*a->boolean(), val);
    }
}

TEST(NodeSceneSerializer, ReconstructionDiagnosticAssetRoundTrip)
{
    NodeScene s;
    const SceneNodeId id = s.addNode("r", NodeKind::Reconstruction);
    NodePayload p; p.value = NodePayload::ReconstructionDiagnostic{0.123f, 0.876f};
    ASSERT_TRUE(s.setAsset(id, p));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);

    NodeScene s2;
    ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);
    const NodePayload* a = s2.asset(s2.nodeByName("r"));
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->type(), NodePayloadType::ReconstructionDiagnostic);
    const auto& rd = *a->reconstructionDiagnostic();
    EXPECT_NEAR(rd.residual,   0.123f, 1e-5f);
    EXPECT_NEAR(rd.confidence, 0.876f, 1e-5f);
}

TEST(NodeSceneSerializer, SimTransformAssetRoundTrip)
{
    NodeScene s;
    const SceneNodeId id = s.addNode("body", NodeKind::Geometry);
    NodePayload p; p.value = NodePayload::SimTransform{1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    ASSERT_TRUE(s.setAsset(id, p));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);

    NodeScene s2;
    ASSERT_TRUE(NodeSceneSerializer::deserialize(data, s2).ok);
    const NodePayload* a = s2.asset(s2.nodeByName("body"));
    ASSERT_NE(a, nullptr);
    const auto& st = *a->simTransform();
    EXPECT_NEAR(st.px, 1.f, 1e-5f); EXPECT_NEAR(st.py, 2.f, 1e-5f); EXPECT_NEAR(st.pz, 3.f, 1e-5f);
    EXPECT_NEAR(st.vx, 4.f, 1e-5f); EXPECT_NEAR(st.vy, 5.f, 1e-5f); EXPECT_NEAR(st.vz, 6.f, 1e-5f);
}

TEST(NodeSceneSerializer, NodeWithoutAssetHasNoARecord)
{
    NodeScene s;
    s.addNode("bare", NodeKind::Constant);

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(data.find("\nA "), std::string::npos);
}

// ── Graph iteration helpers ───────────────────────────────────────────────────

TEST(NodeSceneSerializer, AllNodeIdsSortedAscending)
{
    NodeScene s;
    s.addNode("c", NodeKind::Constant);
    s.addNode("a", NodeKind::Constant);
    s.addNode("b", NodeKind::Constant);

    const auto ids = s.allNodeIds();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_LE(ids[0], ids[1]);
    EXPECT_LE(ids[1], ids[2]);
}

TEST(NodeSceneSerializer, OutgoingEdgesSortedAscending)
{
    NodeScene s;
    const SceneNodeId a = s.addNode("a", NodeKind::Constant);
    const SceneNodeId b = s.addNode("b", NodeKind::Constant);
    const SceneNodeId c = s.addNode("c", NodeKind::Constant);
    ASSERT_TRUE(s.connect(a, c));
    ASSERT_TRUE(s.connect(a, b));

    const auto edges = s.outgoingEdges(a);
    ASSERT_EQ(edges.size(), 2u);
    EXPECT_LE(edges[0], edges[1]);
}

// ── Error paths ───────────────────────────────────────────────────────────────

TEST(NodeSceneSerializer, InvalidHeaderRejected)
{
    NodeScene s;
    const auto r = NodeSceneSerializer::deserialize("WRONG_HEADER\n", s);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errors.empty());
    EXPECT_EQ(s.nodeCount(), 0u);
}

TEST(NodeSceneSerializer, MalformedNRecordRejected)
{
    const std::string data =
        "NEXUS_NODE_SCENE_V1\n"
        "N\n";           // missing id, kind, name
    NodeScene s;
    const auto r = NodeSceneSerializer::deserialize(data, s);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errors.empty());
}

TEST(NodeSceneSerializer, EdgeReferencingUnknownNodeIsError)
{
    const std::string data =
        "NEXUS_NODE_SCENE_V1\n"
        "N 1 Constant node_a\n"
        "E 1 99\n";      // 99 not declared
    NodeScene s;
    const auto r = NodeSceneSerializer::deserialize(data, s);
    EXPECT_FALSE(r.ok);
}

TEST(NodeSceneSerializer, UnknownTagSilentlySkipped)
{
    const std::string data =
        "NEXUS_NODE_SCENE_V1\n"
        "FUTURE_TAG some_value\n"
        "N 1 Constant node_a\n";
    NodeScene s;
    const auto r = NodeSceneSerializer::deserialize(data, s);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(s.nodeCount(), 1u);
    EXPECT_NE(s.nodeByName("node_a"), kInvalidSceneNodeId);
}

TEST(NodeSceneSerializer, NodeCountAndEdgeCountTracked)
{
    NodeScene s;
    const SceneNodeId a = s.addNode("a", NodeKind::Constant);
    const SceneNodeId b = s.addNode("b", NodeKind::Constant);
    ASSERT_TRUE(s.connect(a, b));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.nodeCount, 2u);
    EXPECT_EQ(r.edgeCount, 1u);

    NodeScene s2;
    const auto r2 = NodeSceneSerializer::deserialize(data, s2);
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(r2.nodeCount, 2u);
    EXPECT_EQ(r2.edgeCount, 1u);
}

TEST(NodeSceneSerializer, NodeNameWithSpaceCausesSerializeError)
{
    NodeScene s;
    // NodeScene allows any name; serializer rejects spaces.
    s.addNode("bad name", NodeKind::Constant);

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(data.empty());
}

// ── Full scene round-trip ─────────────────────────────────────────────────────

TEST(NodeSceneSerializer, FullSceneRoundTrip)
{
    NodeScene s;
    const SceneNodeId root  = s.addNode("root_geo",  NodeKind::Geometry);
    const SceneNodeId xform = s.addNode("xform",     NodeKind::Transform);
    const SceneNodeId cst   = s.addNode("cst",       NodeKind::Constant);
    ASSERT_TRUE(s.connect(cst, xform));
    ASSERT_TRUE(s.connect(xform, root));
    ASSERT_TRUE(s.setParent(xform, root));

    NodePayload p; p.value = 7.5f;
    ASSERT_TRUE(s.setAsset(cst, p));

    NodeSceneSerializationReport r;
    const std::string data = NodeSceneSerializer::serialize(s, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.nodeCount, 3u);
    // setParent(xform, root) registers a root→xform dependency edge internally,
    // so total edges = cst→xform + xform→root + root→xform (hierarchy).
    EXPECT_EQ(r.edgeCount, 3u);

    NodeScene s2;
    const auto r2 = NodeSceneSerializer::deserialize(data, s2);
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(s2.nodeCount(), 3u);

    const SceneNodeId c2 = s2.nodeByName("cst");
    const SceneNodeId x2 = s2.nodeByName("xform");
    const SceneNodeId ro = s2.nodeByName("root_geo");
    EXPECT_TRUE(s2.isConnected(c2, x2));
    EXPECT_TRUE(s2.isConnected(x2, ro));
    EXPECT_EQ(s2.parent(x2), ro);
    const NodePayload* a = s2.asset(c2);
    ASSERT_NE(a, nullptr);
    EXPECT_NEAR(*a->scalarF32(), 7.5f, 1e-5f);
}
