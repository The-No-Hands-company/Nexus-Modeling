// Tests for SubgraphTemplate port type contracts (Eval Extension Slice 4).
//
// Covers:
//   - Default contract is NodePayloadType::None (unconstrained).
//   - declareInputPort/declareOutputPort with explicit contract stores it.
//   - inputPortContract/outputPortContract round-trip.
//   - setInputPortContract/setOutputPortContract update an existing port.
//   - setPortContract returns false for unknown port names.
//   - validate() passes when payload matches the declared contract.
//   - validate() fails when payload type conflicts with declared contract.
//   - validate() passes when no payload is set (contract is a declaration, not a requirement).
//   - Contract None is equivalent to unconstrained (no validate failure).
//   - instantiateSubgraph() succeeds for templates with valid contracts.
//   - instantiateSubgraph() fails for templates with violated contracts.
//   - Serialization round-trip preserves non-None contracts (IC/OC records).
//   - Serialization of None contracts omits IC/OC records.

#include <nexus/eval_ext/Subgraph.h>
#include <nexus/eval_ext/SubgraphSerialization.h>

#include <gtest/gtest.h>

using namespace nexus::eval_ext;
using nexus::NodeKind;
using nexus::NodePayload;
using nexus::NodePayloadType;

// ── Default contract ──────────────────────────────────────────────────────────

TEST(SubgraphPortContracts, DefaultContractIsNone)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    LocalNodeId b = tmpl.addNode(NodeKind::Geometry,  "dst");
    [[maybe_unused]] bool ci = tmpl.declareInputPort("in", a);
    [[maybe_unused]] bool co = tmpl.declareOutputPort("out", b);

    EXPECT_EQ(tmpl.inputPortContract("in"),  NodePayloadType::None);
    EXPECT_EQ(tmpl.outputPortContract("out"), NodePayloadType::None);
}

TEST(SubgraphPortContracts, ContractAccessorReturnsNoneForUnknownPort)
{
    SubgraphTemplate tmpl;
    EXPECT_EQ(tmpl.inputPortContract("no_such_port"),  NodePayloadType::None);
    EXPECT_EQ(tmpl.outputPortContract("no_such_port"), NodePayloadType::None);
}

// ── declarePort with contract ─────────────────────────────────────────────────

TEST(SubgraphPortContracts, DeclareInputPortWithContractStoresIt)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool r = tmpl.declareInputPort("in", a, NodePayloadType::ScalarF32);
    EXPECT_EQ(tmpl.inputPortContract("in"), NodePayloadType::ScalarF32);
}

TEST(SubgraphPortContracts, DeclareOutputPortWithContractStoresIt)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Geometry, "dst");
    [[maybe_unused]] bool r = tmpl.declareOutputPort("out", a, NodePayloadType::IntegerI64);
    EXPECT_EQ(tmpl.outputPortContract("out"), NodePayloadType::IntegerI64);
}

TEST(SubgraphPortContracts, NoneContractIsUnconstrained)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool r = tmpl.declareInputPort("in", a, NodePayloadType::None);
    EXPECT_EQ(tmpl.inputPortContract("in"), NodePayloadType::None);
}

// ── setPortContract ───────────────────────────────────────────────────────────

TEST(SubgraphPortContracts, SetInputPortContractUpdates)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool r = tmpl.declareInputPort("in", a);
    EXPECT_TRUE(tmpl.setInputPortContract("in", NodePayloadType::Boolean));
    EXPECT_EQ(tmpl.inputPortContract("in"), NodePayloadType::Boolean);
}

TEST(SubgraphPortContracts, SetOutputPortContractUpdates)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Geometry, "dst");
    [[maybe_unused]] bool r = tmpl.declareOutputPort("out", a);
    EXPECT_TRUE(tmpl.setOutputPortContract("out", NodePayloadType::ScalarF32));
    EXPECT_EQ(tmpl.outputPortContract("out"), NodePayloadType::ScalarF32);
}

TEST(SubgraphPortContracts, SetInputPortContractReturnsFalseForUnknown)
{
    SubgraphTemplate tmpl;
    EXPECT_FALSE(tmpl.setInputPortContract("no_such_port", NodePayloadType::ScalarF32));
}

TEST(SubgraphPortContracts, SetOutputPortContractReturnsFalseForUnknown)
{
    SubgraphTemplate tmpl;
    EXPECT_FALSE(tmpl.setOutputPortContract("no_such_port", NodePayloadType::ScalarF32));
}

// ── validate(): contract checks ───────────────────────────────────────────────

TEST(SubgraphPortContracts, ValidatePassesWhenNoPayloadSet)
{
    // Contract declared but no payload on the node — no violation.
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool r = tmpl.declareInputPort("in", a, NodePayloadType::ScalarF32);
    const auto v = tmpl.validate();
    EXPECT_TRUE(v.ok);
    EXPECT_TRUE(v.issues.empty());
}

TEST(SubgraphPortContracts, ValidatePassesWhenPayloadMatchesContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in", a, NodePayloadType::ScalarF32);
    NodePayload p;
    p.value = 1.5f;
    [[maybe_unused]] bool rp = tmpl.setNodeOutputPayload(a, p);

    const auto v = tmpl.validate();
    EXPECT_TRUE(v.ok) << (v.issues.empty() ? "" : v.issues[0]);
}

TEST(SubgraphPortContracts, ValidateFailsWhenPayloadViolatesInputContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in", a, NodePayloadType::ScalarF32);
    NodePayload p;
    p.value = int64_t{42}; // IntegerI64 ≠ ScalarF32
    [[maybe_unused]] bool rp = tmpl.setNodeOutputPayload(a, p);

    const auto v = tmpl.validate();
    EXPECT_FALSE(v.ok);
    ASSERT_FALSE(v.issues.empty());
    EXPECT_NE(v.issues[0].find("input port"), std::string::npos);
    EXPECT_NE(v.issues[0].find("contract violation"), std::string::npos);
}

TEST(SubgraphPortContracts, ValidateFailsWhenPayloadViolatesOutputContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Geometry, "dst");
    [[maybe_unused]] bool ro = tmpl.declareOutputPort("out", a, NodePayloadType::Boolean);
    NodePayload p;
    p.value = std::string{"hello"}; // TextUtf8 ≠ Boolean
    [[maybe_unused]] bool rp = tmpl.setNodeOutputPayload(a, p);

    const auto v = tmpl.validate();
    EXPECT_FALSE(v.ok);
    ASSERT_FALSE(v.issues.empty());
    EXPECT_NE(v.issues[0].find("output port"), std::string::npos);
}

TEST(SubgraphPortContracts, ValidatePassesForNoneContractAnyPayload)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in", a); // contract = None
    NodePayload p;
    p.value = int64_t{99};
    [[maybe_unused]] bool rp = tmpl.setNodeOutputPayload(a, p);

    EXPECT_TRUE(tmpl.validate().ok);
}

TEST(SubgraphPortContracts, ValidateIssuesAreSorted)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "alpha");
    LocalNodeId b = tmpl.addNode(NodeKind::Constant, "zeta");
    [[maybe_unused]] bool ri1 = tmpl.declareInputPort("z_port", a, NodePayloadType::ScalarF32);
    [[maybe_unused]] bool ri2 = tmpl.declareInputPort("a_port", b, NodePayloadType::Boolean);
    NodePayload pa; pa.value = int64_t{1};
    NodePayload pb; pb.value = std::string{"x"};
    [[maybe_unused]] bool rpa = tmpl.setNodeOutputPayload(a, pa);
    [[maybe_unused]] bool rpb = tmpl.setNodeOutputPayload(b, pb);

    const auto v = tmpl.validate();
    EXPECT_FALSE(v.ok);
    ASSERT_GE(v.issues.size(), 2u);
    EXPECT_LE(v.issues[0], v.issues[1]);
}

// ── instantiateSubgraph() with contracts ─────────────────────────────────────

TEST(SubgraphPortContracts, InstantiateSucceedsWithMatchingContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    LocalNodeId b = tmpl.addNode(NodeKind::Geometry,  "dst");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in", a, NodePayloadType::ScalarF32);
    [[maybe_unused]] bool ro = tmpl.declareOutputPort("out", b);
    // No payload set — contract is satisfied (not violated).
    nexus::EvalGraph host;
    EXPECT_TRUE(instantiateSubgraph(host, tmpl, "test").has_value());
}

TEST(SubgraphPortContracts, InstantiateFailsWithViolatedContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in", a, NodePayloadType::ScalarF32);
    NodePayload p; p.value = int64_t{7}; // wrong type
    [[maybe_unused]] bool rp = tmpl.setNodeOutputPayload(a, p);

    nexus::EvalGraph host;
    EXPECT_FALSE(instantiateSubgraph(host, tmpl, "test").has_value());
}

// ── Serialization round-trip with contracts ───────────────────────────────────

TEST(SubgraphPortContracts, SerializeRoundTripPreservesInputContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in_val", a, NodePayloadType::ScalarF32);

    nexus::eval_ext::SubgraphSerializationReport report;
    std::string data = nexus::eval_ext::SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok) << (report.errors.empty() ? "" : report.errors[0]);

    SubgraphTemplate out;
    auto r2 = nexus::eval_ext::SubgraphSerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok) << (r2.errors.empty() ? "" : r2.errors[0]);
    EXPECT_EQ(out.inputPortContract("in_val"), NodePayloadType::ScalarF32);
}

TEST(SubgraphPortContracts, SerializeRoundTripPreservesOutputContract)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Geometry, "dst");
    [[maybe_unused]] bool ro = tmpl.declareOutputPort("out_geo", a, NodePayloadType::IntegerI64);

    nexus::eval_ext::SubgraphSerializationReport report;
    std::string data = nexus::eval_ext::SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    auto r2 = nexus::eval_ext::SubgraphSerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok);
    EXPECT_EQ(out.outputPortContract("out_geo"), NodePayloadType::IntegerI64);
}

TEST(SubgraphPortContracts, SerializeOmitsNoneContracts)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    [[maybe_unused]] bool ri = tmpl.declareInputPort("in", a); // None contract

    nexus::eval_ext::SubgraphSerializationReport report;
    std::string data = nexus::eval_ext::SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    // No IC record should appear for a None contract.
    EXPECT_EQ(data.find("IC"), std::string::npos);

    SubgraphTemplate out;
    ASSERT_TRUE(nexus::eval_ext::SubgraphSerializer::deserialize(data, out).ok);
    EXPECT_EQ(out.inputPortContract("in"), NodePayloadType::None);
}

TEST(SubgraphPortContracts, SerializeRoundTripMultipleContracts)
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src1");
    LocalNodeId b = tmpl.addNode(NodeKind::Constant, "src2");
    LocalNodeId c = tmpl.addNode(NodeKind::Geometry,  "dst");
    [[maybe_unused]] bool ri1 = tmpl.declareInputPort("in_f",  a, NodePayloadType::ScalarF32);
    [[maybe_unused]] bool ri2 = tmpl.declareInputPort("in_i",  b, NodePayloadType::IntegerI64);
    [[maybe_unused]] bool ro  = tmpl.declareOutputPort("out_g", c, NodePayloadType::Boolean);

    nexus::eval_ext::SubgraphSerializationReport report;
    std::string data = nexus::eval_ext::SubgraphSerializer::serialize(tmpl, report);
    ASSERT_TRUE(report.ok);

    SubgraphTemplate out;
    ASSERT_TRUE(nexus::eval_ext::SubgraphSerializer::deserialize(data, out).ok);
    EXPECT_EQ(out.inputPortContract("in_f"),  NodePayloadType::ScalarF32);
    EXPECT_EQ(out.inputPortContract("in_i"),  NodePayloadType::IntegerI64);
    EXPECT_EQ(out.outputPortContract("out_g"), NodePayloadType::Boolean);
}
