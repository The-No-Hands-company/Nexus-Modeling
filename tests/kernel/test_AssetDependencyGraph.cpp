#include <nexus/asset/AssetDependencyGraph.h>

#include <gtest/gtest.h>

using namespace nexus::asset;

// ─────────────────────────────────────────────────────────────────────────────
//  Node management
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetDependencyGraph, EmptyGraphHasZeroAssets)
{
    AssetDependencyGraph g;
    EXPECT_EQ(g.assetCount(), 0u);
}

TEST(AssetDependencyGraph, AddAssetIncreasesCount)
{
    AssetDependencyGraph g;
    EXPECT_TRUE(g.addAsset({"a.nxas", "A"}));
    EXPECT_EQ(g.assetCount(), 1u);
}

TEST(AssetDependencyGraph, AddDuplicateAssetReturnsFalse)
{
    AssetDependencyGraph g;
    ASSERT_TRUE(g.addAsset({"a.nxas", "A"}));
    EXPECT_FALSE(g.addAsset({"a.nxas", "A2"}));
    EXPECT_EQ(g.assetCount(), 1u);
}

TEST(AssetDependencyGraph, AddAssetWithEmptyPathReturnsFalse)
{
    AssetDependencyGraph g;
    EXPECT_FALSE(g.addAsset({"", "No path"}));
    EXPECT_EQ(g.assetCount(), 0u);
}

TEST(AssetDependencyGraph, HasAssetReturnsTrueAfterAdd)
{
    AssetDependencyGraph g;
    ASSERT_TRUE(g.addAsset({"x.nxas", "X"}));
    EXPECT_TRUE(g.hasAsset("x.nxas"));
    EXPECT_FALSE(g.hasAsset("y.nxas"));
}

TEST(AssetDependencyGraph, RemoveAssetDecreasesCount)
{
    AssetDependencyGraph g;
    ASSERT_TRUE(g.addAsset({"a.nxas", "A"}));
    EXPECT_TRUE(g.removeAsset("a.nxas"));
    EXPECT_EQ(g.assetCount(), 0u);
    EXPECT_FALSE(g.hasAsset("a.nxas"));
}

TEST(AssetDependencyGraph, RemoveUnknownAssetReturnsFalse)
{
    AssetDependencyGraph g;
    EXPECT_FALSE(g.removeAsset("nonexistent.nxas"));
}

TEST(AssetDependencyGraph, ClearRemovesAllAssets)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.clear();
    EXPECT_EQ(g.assetCount(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Edge management
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetDependencyGraph, AddDependencyBetweenKnownNodes)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    EXPECT_TRUE(g.addDependency("a.nxas", "b.nxas"));
    const auto& deps = g.dependenciesOf("a.nxas");
    ASSERT_EQ(deps.size(), 1u);
    EXPECT_EQ(deps[0], "b.nxas");
}

TEST(AssetDependencyGraph, AddDependencyUnknownSourceReturnsFalse)
{
    AssetDependencyGraph g;
    g.addAsset({"b.nxas", "B"});
    EXPECT_FALSE(g.addDependency("missing.nxas", "b.nxas"));
}

TEST(AssetDependencyGraph, AddDependencyUnknownTargetReturnsFalse)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    EXPECT_FALSE(g.addDependency("a.nxas", "missing.nxas"));
}

TEST(AssetDependencyGraph, AddSelfDependencyReturnsFalse)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    EXPECT_FALSE(g.addDependency("a.nxas", "a.nxas"));
}

TEST(AssetDependencyGraph, AddDuplicateDependencyReturnsFalse)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    ASSERT_TRUE(g.addDependency("a.nxas", "b.nxas"));
    EXPECT_FALSE(g.addDependency("a.nxas", "b.nxas"));
    EXPECT_EQ(g.dependenciesOf("a.nxas").size(), 1u);
}

TEST(AssetDependencyGraph, DependenciesAreStoredLexicographically)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"z.nxas", "Z"});
    g.addAsset({"m.nxas", "M"});
    g.addAsset({"b.nxas", "B"});

    ASSERT_TRUE(g.addDependency("a.nxas", "z.nxas"));
    ASSERT_TRUE(g.addDependency("a.nxas", "m.nxas"));
    ASSERT_TRUE(g.addDependency("a.nxas", "b.nxas"));

    const auto& deps = g.dependenciesOf("a.nxas");
    ASSERT_EQ(deps.size(), 3u);
    EXPECT_EQ(deps[0], "b.nxas");
    EXPECT_EQ(deps[1], "m.nxas");
    EXPECT_EQ(deps[2], "z.nxas");
}

TEST(AssetDependencyGraph, DependenciesOfUnknownPathReturnsEmpty)
{
    AssetDependencyGraph g;
    EXPECT_TRUE(g.dependenciesOf("ghost.nxas").empty());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load-order resolution — success cases
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetDependencyGraph, SingleNodeLoadOrder)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    const auto rep = g.resolveLoadOrder();
    EXPECT_TRUE(rep.valid());
    ASSERT_EQ(rep.loadOrder.size(), 1u);
    EXPECT_EQ(rep.loadOrder[0], "a.nxas");
    EXPECT_TRUE(rep.cycles.empty());
    EXPECT_TRUE(rep.missing.empty());
}

TEST(AssetDependencyGraph, LinearChainLoadOrder)
{
    // c → b → a  (c depends on b; b depends on a; load order: a, b, c)
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.addAsset({"c.nxas", "C"});
    g.addDependency("b.nxas", "a.nxas");
    g.addDependency("c.nxas", "b.nxas");

    const auto rep = g.resolveLoadOrder();
    EXPECT_TRUE(rep.valid());
    ASSERT_EQ(rep.loadOrder.size(), 3u);
    EXPECT_EQ(rep.loadOrder[0], "a.nxas");
    EXPECT_EQ(rep.loadOrder[1], "b.nxas");
    EXPECT_EQ(rep.loadOrder[2], "c.nxas");
}

TEST(AssetDependencyGraph, DiamondDependencyLoadOrder)
{
    // d depends on b and c; b and c both depend on a.
    // Load order: a first, then b and c (lexicographic), then d.
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.addAsset({"c.nxas", "C"});
    g.addAsset({"d.nxas", "D"});
    g.addDependency("b.nxas", "a.nxas");
    g.addDependency("c.nxas", "a.nxas");
    g.addDependency("d.nxas", "b.nxas");
    g.addDependency("d.nxas", "c.nxas");

    const auto rep = g.resolveLoadOrder();
    EXPECT_TRUE(rep.valid());
    ASSERT_EQ(rep.loadOrder.size(), 4u);
    EXPECT_EQ(rep.loadOrder[0], "a.nxas");
    // b and c must both precede d; lexicographic order within same tier.
    EXPECT_EQ(rep.loadOrder[1], "b.nxas");
    EXPECT_EQ(rep.loadOrder[2], "c.nxas");
    EXPECT_EQ(rep.loadOrder[3], "d.nxas");
}

TEST(AssetDependencyGraph, IndependentNodesOrderedLexicographically)
{
    AssetDependencyGraph g;
    g.addAsset({"z.nxas", "Z"});
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"m.nxas", "M"});

    const auto rep = g.resolveLoadOrder();
    EXPECT_TRUE(rep.valid());
    ASSERT_EQ(rep.loadOrder.size(), 3u);
    EXPECT_EQ(rep.loadOrder[0], "a.nxas");
    EXPECT_EQ(rep.loadOrder[1], "m.nxas");
    EXPECT_EQ(rep.loadOrder[2], "z.nxas");
}

TEST(AssetDependencyGraph, EmptyGraphReturnsEmptyValidReport)
{
    AssetDependencyGraph g;
    const auto rep = g.resolveLoadOrder();
    EXPECT_TRUE(rep.valid());
    EXPECT_TRUE(rep.loadOrder.empty());
    EXPECT_TRUE(rep.cycles.empty());
    EXPECT_TRUE(rep.missing.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load-order resolution — cycle detection
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetDependencyGraph, DirectCycleIsDetected)
{
    // a → b → a
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.addDependency("a.nxas", "b.nxas");
    g.addDependency("b.nxas", "a.nxas");

    const auto rep = g.resolveLoadOrder();
    EXPECT_EQ(rep.status, DependencyResolutionStatus::CycleDetected);
    EXPECT_FALSE(rep.cycles.empty());
    EXPECT_TRUE(rep.missing.empty());
}

TEST(AssetDependencyGraph, LongerCycleIsDetected)
{
    // a → b → c → a
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.addAsset({"c.nxas", "C"});
    g.addDependency("a.nxas", "b.nxas");
    g.addDependency("b.nxas", "c.nxas");
    g.addDependency("c.nxas", "a.nxas");

    const auto rep = g.resolveLoadOrder();
    EXPECT_EQ(rep.status, DependencyResolutionStatus::CycleDetected);
    EXPECT_EQ(rep.cycles.size(), 3u);
}

TEST(AssetDependencyGraph, CycleReportContainsCyclicPaths)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.addAsset({"independent.nxas", "I"});
    g.addDependency("a.nxas", "b.nxas");
    g.addDependency("b.nxas", "a.nxas");

    const auto rep = g.resolveLoadOrder();
    EXPECT_EQ(rep.status, DependencyResolutionStatus::CycleDetected);
    // Independent node is resolvable; it should appear in loadOrder.
    EXPECT_EQ(rep.loadOrder.size(), 1u);
    EXPECT_EQ(rep.loadOrder[0], "independent.nxas");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load-order resolution — missing dependency detection
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetDependencyGraph, AddDependencyRequiresBothNodesRegistered)
{
    AssetDependencyGraph g;
    g.addAsset({"a.nxas", "A"});
    // b is not registered — addDependency must reject it
    EXPECT_FALSE(g.addDependency("a.nxas", "b.nxas"));
    // After registering b, the edge should succeed
    g.addAsset({"b.nxas", "B"});
    EXPECT_TRUE(g.addDependency("a.nxas", "b.nxas"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Determinism — same graph always produces same order
// ─────────────────────────────────────────────────────────────────────────────

TEST(AssetDependencyGraph, ResolveLoadOrderIsDeterministic)
{
    AssetDependencyGraph g;
    g.addAsset({"c.nxas", "C"});
    g.addAsset({"a.nxas", "A"});
    g.addAsset({"b.nxas", "B"});
    g.addDependency("c.nxas", "a.nxas");
    g.addDependency("c.nxas", "b.nxas");

    const auto rep1 = g.resolveLoadOrder();
    const auto rep2 = g.resolveLoadOrder();

    ASSERT_EQ(rep1.loadOrder.size(), rep2.loadOrder.size());
    for (size_t i = 0u; i < rep1.loadOrder.size(); ++i) {
        EXPECT_EQ(rep1.loadOrder[i], rep2.loadOrder[i]);
    }
}

TEST(AssetDependencyGraph, LoadOrderIsIndependentOfDependencyInsertionOrder)
{
    AssetDependencyGraph g1;
    g1.addAsset({"a.nxas", "A"});
    g1.addAsset({"b.nxas", "B"});
    g1.addAsset({"c.nxas", "C"});
    g1.addAsset({"d.nxas", "D"});

    ASSERT_TRUE(g1.addDependency("d.nxas", "b.nxas"));
    ASSERT_TRUE(g1.addDependency("d.nxas", "c.nxas"));
    ASSERT_TRUE(g1.addDependency("d.nxas", "a.nxas"));

    AssetDependencyGraph g2;
    g2.addAsset({"a.nxas", "A"});
    g2.addAsset({"b.nxas", "B"});
    g2.addAsset({"c.nxas", "C"});
    g2.addAsset({"d.nxas", "D"});

    ASSERT_TRUE(g2.addDependency("d.nxas", "a.nxas"));
    ASSERT_TRUE(g2.addDependency("d.nxas", "c.nxas"));
    ASSERT_TRUE(g2.addDependency("d.nxas", "b.nxas"));

    const auto rep1 = g1.resolveLoadOrder();
    const auto rep2 = g2.resolveLoadOrder();

    EXPECT_TRUE(rep1.valid());
    EXPECT_TRUE(rep2.valid());
    ASSERT_EQ(rep1.loadOrder.size(), rep2.loadOrder.size());
    for (size_t i = 0u; i < rep1.loadOrder.size(); ++i) {
        EXPECT_EQ(rep1.loadOrder[i], rep2.loadOrder[i]);
    }
}
