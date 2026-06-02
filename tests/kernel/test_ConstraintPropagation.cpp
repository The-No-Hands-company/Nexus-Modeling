#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSolver.h>
#include <gtest/gtest.h>
#include <algorithm>

using namespace nexus::parametric;

// ─── buildDependencyOrder ─────────────────────────────────────────────────────

TEST(ConstraintPropagation, EmptyGraphEmptyOrder)
{
    ConstraintGraph g;
    const auto order = g.buildDependencyOrder();
    EXPECT_TRUE(order.empty());
}

TEST(ConstraintPropagation, SingleConstraintInOrder)
{
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({1.0, 0.0, 0.0});
    const auto cId = g.addDistanceConstraint(eA, eB, 1.0);
    const auto order = g.buildDependencyOrder();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], cId);
}

TEST(ConstraintPropagation, ChainOrderRespectsDependencies)
{
    // eA → c1 → eB → c2 → eC
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({1.0, 0.0, 0.0});
    const auto eC = g.addPoint({2.0, 0.0, 0.0});
    const auto c1 = g.addDistanceConstraint(eA, eB, 1.0);
    const auto c2 = g.addDistanceConstraint(eB, eC, 1.0);
    const auto order = g.buildDependencyOrder();
    ASSERT_EQ(order.size(), 2u);
    // c1 must appear before c2.
    const auto pos1 = std::find(order.begin(), order.end(), c1) - order.begin();
    const auto pos2 = std::find(order.begin(), order.end(), c2) - order.begin();
    EXPECT_LT(pos1, pos2);
}

TEST(ConstraintPropagation, AllConstraintsAppearInOrder)
{
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({1.0, 0.0, 0.0});
    const auto eC = g.addPoint({2.0, 0.0, 0.0});
    const auto c1 = g.addDistanceConstraint(eA, eB, 1.0);
    const auto c2 = g.addAxisAlignedDistanceConstraint(eA, eC, Axis::X, 2.0);
    const auto c3 = g.addCoincidentConstraint(eB, eC);
    const auto order = g.buildDependencyOrder();
    EXPECT_EQ(order.size(), 3u);
    EXPECT_NE(std::find(order.begin(), order.end(), c1), order.end());
    EXPECT_NE(std::find(order.begin(), order.end(), c2), order.end());
    EXPECT_NE(std::find(order.begin(), order.end(), c3), order.end());
}

// ─── cycleMembers ─────────────────────────────────────────────────────────────

TEST(ConstraintPropagation, NoCyclesEmptyCycleList)
{
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({1.0, 0.0, 0.0});
    (void)g.addDistanceConstraint(eA, eB, 1.0);
    const auto cycles = g.cycleMembers();
    EXPECT_TRUE(cycles.empty());
}

TEST(ConstraintPropagation, MutualConstraintDetectedAsCycle)
{
    // eA → c1 → eB and eB → c2 → eA: a 2-node cycle.
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({1.0, 0.0, 0.0});
    const auto c1 = g.addDistanceConstraint(eA, eB, 1.0);
    const auto c2 = g.addDistanceConstraint(eB, eA, 1.0);
    const auto cycles = g.cycleMembers();
    // Both constraints are in the cycle.
    EXPECT_EQ(cycles.size(), 2u);
    EXPECT_NE(std::find(cycles.begin(), cycles.end(), c1), cycles.end());
    EXPECT_NE(std::find(cycles.begin(), cycles.end(), c2), cycles.end());
}

// ─── Solver integration ───────────────────────────────────────────────────────

TEST(ConstraintPropagation, SolverReportHasPropagationOrder)
{
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({2.0, 0.0, 0.0});
    const auto cId = g.addDistanceConstraint(eA, eB, 1.0);
    ParametricSolverConfig cfg; cfg.useTopologicalOrder = true;
    const auto report = ParametricSolver::solve(g, cfg);
    EXPECT_EQ(report.propagationOrder.size(), 1u);
    EXPECT_EQ(report.propagationOrder[0], cId);
}

TEST(ConstraintPropagation, SolverReportNoPropagationOrderWhenDisabled)
{
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({2.0, 0.0, 0.0});
    (void)g.addDistanceConstraint(eA, eB, 1.0);
    ParametricSolverConfig cfg; cfg.useTopologicalOrder = false;
    const auto report = ParametricSolver::solve(g, cfg);
    EXPECT_TRUE(report.propagationOrder.empty());
}

TEST(ConstraintPropagation, SolverCycleConstraintsInReport)
{
    ConstraintGraph g;
    const auto eA = g.addPoint({0.0, 0.0, 0.0});
    const auto eB = g.addPoint({1.0, 0.0, 0.0});
    (void)g.addDistanceConstraint(eA, eB, 1.0);
    (void)g.addDistanceConstraint(eB, eA, 1.0); // cycle
    ParametricSolverConfig cfg; cfg.useTopologicalOrder = true;
    const auto report = ParametricSolver::solve(g, cfg);
    EXPECT_EQ(report.cycleConstraintIds.size(), 2u);
}
