// Tests for SubgraphRegistry and SubgraphRegistrySerializer (Eval Extension Slice 5).
//
// Covers:
//   - Empty registry: size, empty, names, find, contains.
//   - add: success, duplicate rejected, invalid name rejected.
//   - remove: success, unknown returns false.
//   - find: const and mutable overloads.
//   - contains.
//   - clear.
//   - names() sorted ascending.
//   - Serialization: empty registry round-trip.
//   - Serialization: single template round-trip (contracts preserved).
//   - Serialization: multiple templates round-trip.
//   - Serialization: template count in report.
//   - Serialization: invalid header rejected.
//   - Serialization: duplicate template name reported.
//   - Serialization: template with contracts round-trips contracts.
//   - Serialization: None contracts omitted (no IC record).
//   - Serialization: deserialized templates pass validate().

#include <nexus/eval_ext/SubgraphRegistry.h>

#include <gtest/gtest.h>

using namespace nexus::eval_ext;
using nexus::NodeKind;
using nexus::NodePayloadType;

// ── Helpers ───────────────────────────────────────────────────────────────────

static SubgraphTemplate makeSimpleTemplate()
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    LocalNodeId b = tmpl.addNode(NodeKind::Geometry,  "dst");
    [[maybe_unused]] bool ci = tmpl.declareInputPort("in", a);
    [[maybe_unused]] bool co = tmpl.declareOutputPort("out", b);
    return tmpl;
}

static SubgraphTemplate makeContractTemplate()
{
    SubgraphTemplate tmpl;
    LocalNodeId a = tmpl.addNode(NodeKind::Constant, "src");
    LocalNodeId b = tmpl.addNode(NodeKind::Geometry,  "dst");
    [[maybe_unused]] bool ci = tmpl.declareInputPort("in_f",  a, NodePayloadType::ScalarF32);
    [[maybe_unused]] bool co = tmpl.declareOutputPort("out_i", b, NodePayloadType::IntegerI64);
    return tmpl;
}

// ── Empty registry ────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, EmptyIsEmpty)
{
    SubgraphRegistry reg;
    EXPECT_EQ(reg.size(), 0u);
    EXPECT_TRUE(reg.empty());
    EXPECT_TRUE(reg.names().empty());
}

TEST(SubgraphRegistry, FindOnEmptyReturnsNull)
{
    SubgraphRegistry reg;
    EXPECT_EQ(reg.find("no_such"), nullptr);
}

TEST(SubgraphRegistry, ContainsOnEmptyReturnsFalse)
{
    SubgraphRegistry reg;
    EXPECT_FALSE(reg.contains("no_such"));
}

// ── add ───────────────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, AddSucceeds)
{
    SubgraphRegistry reg;
    EXPECT_TRUE(reg.add("tmpl_a", makeSimpleTemplate()));
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_FALSE(reg.empty());
    EXPECT_TRUE(reg.contains("tmpl_a"));
}

TEST(SubgraphRegistry, AddDuplicateReturnsFalse)
{
    SubgraphRegistry reg;
    EXPECT_TRUE(reg.add("tmpl_a", makeSimpleTemplate()));
    EXPECT_FALSE(reg.add("tmpl_a", makeSimpleTemplate()));
    EXPECT_EQ(reg.size(), 1u);
}

TEST(SubgraphRegistry, AddEmptyNameReturnsFalse)
{
    SubgraphRegistry reg;
    EXPECT_FALSE(reg.add("", makeSimpleTemplate()));
    EXPECT_EQ(reg.size(), 0u);
}

TEST(SubgraphRegistry, AddInvalidNameReturnsFalse)
{
    SubgraphRegistry reg;
    EXPECT_FALSE(reg.add("bad name!", makeSimpleTemplate())); // space + !
    EXPECT_EQ(reg.size(), 0u);
}

// ── remove ────────────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, RemoveSucceeds)
{
    SubgraphRegistry reg;
    reg.add("tmpl_a", makeSimpleTemplate());
    EXPECT_TRUE(reg.remove("tmpl_a"));
    EXPECT_EQ(reg.size(), 0u);
    EXPECT_FALSE(reg.contains("tmpl_a"));
}

TEST(SubgraphRegistry, RemoveUnknownReturnsFalse)
{
    SubgraphRegistry reg;
    EXPECT_FALSE(reg.remove("no_such"));
}

// ── find ──────────────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, FindReturnsPointerForKnownName)
{
    SubgraphRegistry reg;
    reg.add("tmpl_a", makeSimpleTemplate());
    const SubgraphTemplate* p = reg.find("tmpl_a");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->nodeCount(), 2u);
}

TEST(SubgraphRegistry, MutableFindAllowsModification)
{
    SubgraphRegistry reg;
    reg.add("tmpl_a", makeSimpleTemplate());
    SubgraphTemplate* p = reg.find("tmpl_a");
    ASSERT_NE(p, nullptr);
    [[maybe_unused]] LocalNodeId extra = p->addNode(NodeKind::Merge, "extra");
    EXPECT_EQ(reg.find("tmpl_a")->nodeCount(), 3u);
}

TEST(SubgraphRegistry, FindReturnsNullForUnknownName)
{
    SubgraphRegistry reg;
    reg.add("tmpl_a", makeSimpleTemplate());
    EXPECT_EQ(reg.find("tmpl_b"), nullptr);
}

// ── clear ─────────────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, ClearEmptiesRegistry)
{
    SubgraphRegistry reg;
    reg.add("a", makeSimpleTemplate());
    reg.add("b", makeSimpleTemplate());
    reg.clear();
    EXPECT_EQ(reg.size(), 0u);
    EXPECT_TRUE(reg.empty());
}

// ── names() sorted ────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, NamesAreSortedAscending)
{
    SubgraphRegistry reg;
    reg.add("zebra",  makeSimpleTemplate());
    reg.add("alpha",  makeSimpleTemplate());
    reg.add("middle", makeSimpleTemplate());

    const auto n = reg.names();
    ASSERT_EQ(n.size(), 3u);
    EXPECT_EQ(n[0], "alpha");
    EXPECT_EQ(n[1], "middle");
    EXPECT_EQ(n[2], "zebra");
}

// ── Serialization ─────────────────────────────────────────────────────────────

TEST(SubgraphRegistry, SerializeEmptyRoundTrip)
{
    SubgraphRegistry reg;
    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.templateCount, 0u);

    SubgraphRegistry out;
    const auto r2 = SubgraphRegistrySerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok) << (r2.errors.empty() ? "" : r2.errors[0]);
    EXPECT_EQ(r2.templateCount, 0u);
    EXPECT_TRUE(out.empty());
}

TEST(SubgraphRegistry, SerializeSingleTemplateRoundTrip)
{
    SubgraphRegistry reg;
    reg.add("simple", makeSimpleTemplate());

    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.templateCount, 1u);

    SubgraphRegistry out;
    const auto r2 = SubgraphRegistrySerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok) << (r2.errors.empty() ? "" : r2.errors[0]);
    EXPECT_EQ(r2.templateCount, 1u);
    ASSERT_TRUE(out.contains("simple"));
    EXPECT_EQ(out.find("simple")->nodeCount(), 2u);
}

TEST(SubgraphRegistry, SerializeMultipleTemplatesRoundTrip)
{
    SubgraphRegistry reg;
    reg.add("alpha",   makeSimpleTemplate());
    reg.add("beta",    makeContractTemplate());
    reg.add("gamma",   makeSimpleTemplate());

    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.templateCount, 3u);

    SubgraphRegistry out;
    const auto r2 = SubgraphRegistrySerializer::deserialize(data, out);
    EXPECT_TRUE(r2.ok) << (r2.errors.empty() ? "" : r2.errors[0]);
    EXPECT_EQ(r2.templateCount, 3u);
    EXPECT_EQ(out.size(), 3u);
    EXPECT_TRUE(out.contains("alpha"));
    EXPECT_TRUE(out.contains("beta"));
    EXPECT_TRUE(out.contains("gamma"));
}

TEST(SubgraphRegistry, SerializePreservesContracts)
{
    SubgraphRegistry reg;
    reg.add("typed", makeContractTemplate());

    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok);

    SubgraphRegistry out;
    ASSERT_TRUE(SubgraphRegistrySerializer::deserialize(data, out).ok);
    const SubgraphTemplate* tmpl = out.find("typed");
    ASSERT_NE(tmpl, nullptr);
    EXPECT_EQ(tmpl->inputPortContract("in_f"),  NodePayloadType::ScalarF32);
    EXPECT_EQ(tmpl->outputPortContract("out_i"), NodePayloadType::IntegerI64);
}

TEST(SubgraphRegistry, SerializeNoneContractsOmitted)
{
    SubgraphRegistry reg;
    reg.add("plain", makeSimpleTemplate()); // no contracts → None

    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok);

    // No IC/OC records should appear.
    EXPECT_EQ(data.find("IC"), std::string::npos);
    EXPECT_EQ(data.find("OC"), std::string::npos);
}

TEST(SubgraphRegistry, DeserializedTemplatesPassValidate)
{
    SubgraphRegistry reg;
    [[maybe_unused]] bool ra = reg.add("a", makeSimpleTemplate());
    [[maybe_unused]] bool rb = reg.add("b", makeContractTemplate());

    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok);

    SubgraphRegistry out;
    ASSERT_TRUE(SubgraphRegistrySerializer::deserialize(data, out).ok);
    for (const auto& name : out.names()) {
        const auto v = out.find(name)->validate();
        EXPECT_TRUE(v.ok) << "template '" << name << "' failed validate(): "
                          << (v.issues.empty() ? "" : v.issues[0]);
    }
}

TEST(SubgraphRegistry, DeserializeInvalidHeaderReturnsError)
{
    SubgraphRegistry out;
    const auto r = SubgraphRegistrySerializer::deserialize("WRONG_HEADER\n", out);
    EXPECT_FALSE(r.ok);
    ASSERT_FALSE(r.errors.empty());
}

TEST(SubgraphRegistry, DeserializeDuplicateNameIsReported)
{
    // Manually craft an archive with two T records of the same name.
    const std::string data =
        "NEXUS_SUBGRAPH_REGISTRY_V1\n"
        "T dup\n"
        "N 1 Constant src\n"
        "I in 1\n"
        "T dup\n"          // duplicate
        "N 1 Constant src\n"
        "I in 1\n";

    SubgraphRegistry out;
    const auto r = SubgraphRegistrySerializer::deserialize(data, out);
    EXPECT_FALSE(r.ok);
    ASSERT_FALSE(r.errors.empty());
    EXPECT_EQ(out.size(), 1u); // first one was added, second rejected
}

TEST(SubgraphRegistry, TemplateCountMatchesNamesCount)
{
    SubgraphRegistry reg;
    for (int i = 0; i < 5; ++i) {
        [[maybe_unused]] bool ok = reg.add("t" + std::to_string(i), makeSimpleTemplate());
    }

    SubgraphRegistrySerializationReport r;
    const std::string data = SubgraphRegistrySerializer::serialize(reg, r);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.templateCount, 5u);

    SubgraphRegistry out;
    const auto r2 = SubgraphRegistrySerializer::deserialize(data, out);
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(r2.templateCount, 5u);
    EXPECT_EQ(out.names().size(), 5u);
}
