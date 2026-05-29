#include <nexus/eval_ext/SubgraphSerialization.h>
#include <nexus/eval_ext/Subgraph.h>

#include <gtest/gtest.h>

using namespace nexus::eval_ext;
using nexus::NodeKind;
using nexus::NodePayload;
using nexus::NodePayloadType;

// ── Helpers ───────────────────────────────────────────────────────────────────

static SubgraphTemplate makeSimpleTemplate()
{
    SubgraphTemplate t;
    LocalNodeId a = t.addNode(NodeKind::Geometry, "geo");
    LocalNodeId b = t.addNode(NodeKind::Transform, "xform");
    [[maybe_unused]] bool ce = t.connect(a, b);
    [[maybe_unused]] bool ci = t.declareInputPort("in_geo", a);
    [[maybe_unused]] bool co = t.declareOutputPort("out_xform", b);
    return t;
}

// Instantiate `tmpl` into a temp graph and read the payload for localId `id`.
static const nexus::NodePayload* getPayloadViaInstantiation(
    nexus::EvalGraph& tempGraph, const SubgraphTemplate& tmpl, LocalNodeId id)
{
    auto instOpt = instantiateSubgraph(tempGraph, tmpl, "_read");
    if (!instOpt) return nullptr;
    auto it = instOpt->localToHost.find(id);
    if (it == instOpt->localToHost.end()) return nullptr;
    return tempGraph.nodeOutputPayload(it->second);
}

// ── Round-trip ────────────────────────────────────────────────────────────────

TEST(SubgraphSerialization, RoundTripPreservesNodeCount)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok) << report.errors[0];
    ASSERT_FALSE(data.empty());

    SubgraphTemplate out;
    auto r2 = SubgraphSerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok);
    EXPECT_EQ(out.nodeCount(), tmpl.nodeCount());
}

TEST(SubgraphSerialization, RoundTripPreservesNodeKinds)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    // Ids are assigned in order; geo=1, xform=2
    EXPECT_EQ(out.nodeKind(1), NodeKind::Geometry);
    EXPECT_EQ(out.nodeKind(2), NodeKind::Transform);
}

TEST(SubgraphSerialization, RoundTripPreservesNodeNames)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    EXPECT_EQ(out.nodeName(1), "geo");
    EXPECT_EQ(out.nodeName(2), "xform");
}

TEST(SubgraphSerialization, RoundTripPreservesEdge)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);
    EXPECT_EQ(out.edgeCount(), 1u);
}

TEST(SubgraphSerialization, RoundTripPreservesInputPort)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    ASSERT_EQ(out.inputPortNames().size(), 1u);
    EXPECT_EQ(out.inputPortNames()[0], "in_geo");
    EXPECT_EQ(out.inputPortTarget("in_geo"), 1u);
}

TEST(SubgraphSerialization, RoundTripPreservesOutputPort)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    ASSERT_EQ(out.outputPortNames().size(), 1u);
    EXPECT_EQ(out.outputPortNames()[0], "out_xform");
    EXPECT_EQ(out.outputPortTarget("out_xform"), 2u);
}

// ── Payload round-trips ───────────────────────────────────────────────────────

TEST(SubgraphSerialization, RoundTripScalarF32Payload)
{
    SubgraphTemplate tmpl;
    LocalNodeId c = tmpl.addNode(NodeKind::Constant, "val");
    NodePayload p;
    p.value = 3.14f;
    [[maybe_unused]] bool sp = tmpl.setNodeOutputPayload(c, std::move(p));

    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    nexus::EvalGraph tg;
    const NodePayload* rp = getPayloadViaInstantiation(tg, out, 1);
    ASSERT_NE(rp, nullptr);
    ASSERT_EQ(rp->type(), NodePayloadType::ScalarF32);
    EXPECT_NEAR(*rp->scalarF32(), 3.14f, 1e-5f);
}

TEST(SubgraphSerialization, RoundTripIntegerI64Payload)
{
    SubgraphTemplate tmpl;
    LocalNodeId c = tmpl.addNode(NodeKind::Constant, "ival");
    NodePayload p;
    p.value = int64_t{-42};
    [[maybe_unused]] bool sp = tmpl.setNodeOutputPayload(c, std::move(p));

    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    nexus::EvalGraph tg;
    const NodePayload* rp = getPayloadViaInstantiation(tg, out, 1);
    ASSERT_NE(rp, nullptr);
    ASSERT_EQ(rp->type(), NodePayloadType::IntegerI64);
    EXPECT_EQ(*rp->integerI64(), int64_t{-42});
}

TEST(SubgraphSerialization, RoundTripBooleanPayload)
{
    SubgraphTemplate tmpl;
    LocalNodeId c = tmpl.addNode(NodeKind::Constant, "flag");
    NodePayload p;
    p.value = true;
    [[maybe_unused]] bool sp = tmpl.setNodeOutputPayload(c, std::move(p));

    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    nexus::EvalGraph tg;
    const NodePayload* rp = getPayloadViaInstantiation(tg, out, 1);
    ASSERT_NE(rp, nullptr);
    ASSERT_EQ(rp->type(), NodePayloadType::Boolean);
    EXPECT_TRUE(*rp->boolean());
}

TEST(SubgraphSerialization, RoundTripTextUtf8Payload)
{
    SubgraphTemplate tmpl;
    LocalNodeId c = tmpl.addNode(NodeKind::Constant, "label");
    NodePayload p;
    p.value = std::string{"hello_world"};
    [[maybe_unused]] bool sp = tmpl.setNodeOutputPayload(c, std::move(p));

    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);

    nexus::EvalGraph tg;
    const NodePayload* rp = getPayloadViaInstantiation(tg, out, 1);
    ASSERT_NE(rp, nullptr);
    ASSERT_EQ(rp->type(), NodePayloadType::TextUtf8);
    EXPECT_EQ(*rp->textUtf8(), "hello_world");
}

// ── Error paths ───────────────────────────────────────────────────────────────

TEST(SubgraphSerialization, InvalidNodeNameFailsSerialize)
{
    SubgraphTemplate tmpl;
    [[maybe_unused]] auto _n = tmpl.addNode(NodeKind::Geometry, "bad name!");
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(data.empty());
    EXPECT_FALSE(report.errors.empty());
}

TEST(SubgraphSerialization, InvalidInputPortNameFailsSerialize)
{
    SubgraphTemplate tmpl;
    LocalNodeId n = tmpl.addNode(NodeKind::Geometry, "geo");
    [[maybe_unused]] bool _r = tmpl.declareInputPort("bad port", n);
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(data.empty());
}

TEST(SubgraphSerialization, MissingHeaderFailsDeserialize)
{
    SubgraphTemplate out;
    auto r = SubgraphSerializer::deserialize("NOT_A_HEADER\nN 1 Geometry geo\n", out);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errors.empty());
}

TEST(SubgraphSerialization, EmptyStringFailsDeserialize)
{
    SubgraphTemplate out;
    auto r = SubgraphSerializer::deserialize("", out);
    EXPECT_FALSE(r.ok);
}

TEST(SubgraphSerialization, MalformedNRecordFailsDeserialize)
{
    std::string data = "NEXUS_SUBGRAPH_V1\nN\n";
    SubgraphTemplate out;
    auto r = SubgraphSerializer::deserialize(data, out);
    EXPECT_FALSE(r.ok);
}

TEST(SubgraphSerialization, UnknownNodeKindFailsDeserialize)
{
    std::string data = "NEXUS_SUBGRAPH_V1\nN 1 Bogus mynode\n";
    SubgraphTemplate out;
    auto r = SubgraphSerializer::deserialize(data, out);
    EXPECT_FALSE(r.ok);
}

TEST(SubgraphSerialization, UnknownTagIsSkippedGracefully)
{
    SubgraphTemplate tmpl;
    [[maybe_unused]] auto _n = tmpl.addNode(NodeKind::Geometry, "geo");
    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    // Inject an unknown tag line
    data += "Z future_extension_data 42\n";

    SubgraphTemplate out;
    auto r = SubgraphSerializer::deserialize(data, out);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(out.nodeCount(), 1u);
}

// ── Determinism ───────────────────────────────────────────────────────────────

TEST(SubgraphSerialization, SerializeIsIdempotent)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport r1, r2;
    std::string s1 = SubgraphSerializer::serialize(tmpl, r1);
    std::string s2 = SubgraphSerializer::serialize(tmpl, r2);
    EXPECT_TRUE(r1.ok);
    EXPECT_TRUE(r2.ok);
    EXPECT_EQ(s1, s2);
}

TEST(SubgraphSerialization, RoundTripIsIdempotent)
{
    auto tmpl = makeSimpleTemplate();
    SubgraphSerializationReport r1;
    std::string data1 = SubgraphSerializer::serialize(tmpl, r1);
    ASSERT_TRUE(r1.ok);

    SubgraphTemplate out1;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data1, out1).ok);

    SubgraphSerializationReport r2;
    std::string data2 = SubgraphSerializer::serialize(out1, r2);
    EXPECT_TRUE(r2.ok);
    EXPECT_EQ(data1, data2);
}

// ── All NodeKind variants ─────────────────────────────────────────────────────

TEST(SubgraphSerialization, AllNodeKindsRoundTrip)
{
    struct KindName { NodeKind kind; const char* name; };
    const KindName kinds[] = {
        {NodeKind::Geometry,       "n_geo"},
        {NodeKind::Animation,      "n_anim"},
        {NodeKind::Transform,      "n_xform"},
        {NodeKind::Merge,          "n_merge"},
        {NodeKind::ProxyGeometry,  "n_proxy"},
        {NodeKind::Reconstruction, "n_recon"},
        {NodeKind::Constant,       "n_const"},
        {NodeKind::Expression,     "n_expr"},
    };

    SubgraphTemplate tmpl;
    for (const auto& kn : kinds)
        [[maybe_unused]] auto _id = tmpl.addNode(kn.kind, kn.name);

    SubgraphSerializationReport report;
    std::string data = SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok) << (!report.errors.empty() ? report.errors[0] : "");

    SubgraphTemplate out;
    ASSERT_TRUE(SubgraphSerializer::deserialize(data, out).ok);
    EXPECT_EQ(out.nodeCount(), std::size(kinds));

    LocalNodeId id = 1;
    for (const auto& kn : kinds) {
        EXPECT_EQ(out.nodeKind(id), kn.kind) << kn.name;
        EXPECT_EQ(out.nodeName(id), kn.name) << kn.name;
        ++id;
    }
}

// ── Report sorted errors ──────────────────────────────────────────────────────

TEST(SubgraphSerialization, ErrorsAreSorted)
{
    SubgraphTemplate tmpl;
    [[maybe_unused]] auto id1 = tmpl.addNode(NodeKind::Geometry, "z bad");
    [[maybe_unused]] auto id2 = tmpl.addNode(NodeKind::Geometry, "a bad!");
    SubgraphSerializationReport report;
    [[maybe_unused]] auto _ = SubgraphSerializer::serialize(tmpl, report);
    EXPECT_FALSE(report.ok);
    ASSERT_GE(report.errors.size(), 2u);
    EXPECT_LE(report.errors[0], report.errors[1]);
}
