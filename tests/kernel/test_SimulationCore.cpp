#include <gtest/gtest.h>
#include "nexus/sim/SimulationCore.h"

#include <cmath>

using namespace nexus;

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool approxEq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static bool vec3Eq(const SimVec3& a, const SimVec3& b, float eps = 1e-4f) {
    return approxEq(a.x, b.x, eps) && approxEq(a.y, b.y, eps) && approxEq(a.z, b.z, eps);
}

// ── Body management ───────────────────────────────────────────────────────────

TEST(SimulationCore, EmptySolverHasZeroBodies) {
    RigidBodySolver s;
    EXPECT_EQ(s.bodyCount(), 0u);
}

TEST(SimulationCore, AddBodyIncreasesCount) {
    RigidBodySolver s;
    s.addBody({1.0f});
    EXPECT_EQ(s.bodyCount(), 1u);
}

TEST(SimulationCore, AddedBodyIsKnown) {
    RigidBodySolver s;
    BodyId id = s.addBody({1.0f});
    EXPECT_TRUE(s.hasBody(id));
    EXPECT_NE(id, kInvalidBodyId);
}

TEST(SimulationCore, UnknownBodyReturnsFalse) {
    RigidBodySolver s;
    EXPECT_FALSE(s.hasBody(99u));
}

TEST(SimulationCore, BodyIdsAreUniqueAndNonZero) {
    RigidBodySolver s;
    BodyId a = s.addBody({1.0f});
    BodyId b = s.addBody({2.0f});
    EXPECT_NE(a, b);
    EXPECT_NE(a, kInvalidBodyId);
    EXPECT_NE(b, kInvalidBodyId);
}

TEST(SimulationCore, RemoveBodyDecreasesCount) {
    RigidBodySolver s;
    BodyId id = s.addBody({1.0f});
    EXPECT_TRUE(s.removeBody(id));
    EXPECT_EQ(s.bodyCount(), 0u);
    EXPECT_FALSE(s.hasBody(id));
}

TEST(SimulationCore, RemoveUnknownBodyReturnsFalse) {
    RigidBodySolver s;
    EXPECT_FALSE(s.removeBody(99u));
}

TEST(SimulationCore, GetBodyStateReturnsInitialValues) {
    RigidBodySolver s;
    SimBodyDesc desc;
    desc.mass     = 2.0f;
    desc.position = {1.0f, 2.0f, 3.0f};
    desc.velocity = {0.1f, 0.2f, 0.3f};
    BodyId id = s.addBody(desc);

    SimVec3 pos, vel;
    EXPECT_TRUE(s.getBodyState(id, pos, vel));
    EXPECT_TRUE(vec3Eq(pos, {1.0f, 2.0f, 3.0f}));
    EXPECT_TRUE(vec3Eq(vel, {0.1f, 0.2f, 0.3f}));
}

TEST(SimulationCore, GetBodyStateReturnsFalseForUnknownId) {
    RigidBodySolver s;
    SimVec3 p, v;
    EXPECT_FALSE(s.getBodyState(99u, p, v));
}

TEST(SimulationCore, ClearRemovesAllBodies) {
    RigidBodySolver s;
    s.addBody({1.0f});
    s.addBody({2.0f});
    s.clear();
    EXPECT_EQ(s.bodyCount(), 0u);
}

TEST(SimulationCore, ClearResetsSimulationTime) {
    RigidBodySolver s;
    s.addBody({1.0f});
    s.step(0.016);
    s.clear();
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.0);
}

// ── Static bodies ─────────────────────────────────────────────────────────────

TEST(SimulationCore, StaticBodyIsNotIntegrated) {
    RigidBodySolver s;
    SimBodyDesc desc;
    desc.mass     = 0.0f; // static
    desc.position = {0.0f, 10.0f, 0.0f};
    BodyId id = s.addBody(desc);

    auto r = s.step(1.0);
    EXPECT_EQ(r.bodiesIntegrated, 0u);

    SimVec3 pos, vel;
    s.getBodyState(id, pos, vel);
    EXPECT_TRUE(vec3Eq(pos, {0.0f, 10.0f, 0.0f}));
}

TEST(SimulationCore, ApplyForceReturnsFalseForStaticBody) {
    RigidBodySolver s;
    BodyId id = s.addBody({0.0f}); // static
    EXPECT_FALSE(s.applyForce(id, {0.0f, 100.0f, 0.0f}));
}

// ── Gravity ───────────────────────────────────────────────────────────────────

TEST(SimulationCore, DefaultGravityIsNegativeY) {
    RigidBodySolver s;
    SimVec3 g = s.gravity();
    EXPECT_FLOAT_EQ(g.x, 0.0f);
    EXPECT_LT(g.y, 0.0f);
    EXPECT_FLOAT_EQ(g.z, 0.0f);
}

TEST(SimulationCore, SetGravityChangesGravity) {
    RigidBodySolver s;
    s.setGravity({0.0f, -20.0f, 0.0f});
    SimVec3 g = s.gravity();
    EXPECT_FLOAT_EQ(g.y, -20.0f);
}

TEST(SimulationCore, ZeroGravityBodyFallsNoFurtherThanInitialVelocity) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});

    SimBodyDesc desc;
    desc.mass     = 1.0f;
    desc.position = {0.0f, 0.0f, 0.0f};
    desc.velocity = {1.0f, 0.0f, 0.0f};
    BodyId id = s.addBody(desc);

    s.step(1.0); // dt = 1 s, v = (1,0,0), no gravity, no forces
    SimVec3 pos, vel;
    s.getBodyState(id, pos, vel);
    EXPECT_TRUE(vec3Eq(pos, {1.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(vec3Eq(vel, {1.0f, 0.0f, 0.0f})); // no acceleration
}

// ── Simulation step ───────────────────────────────────────────────────────────

TEST(SimulationCore, StepRejectsZeroDt) {
    RigidBodySolver s;
    s.addBody({1.0f});
    auto r = s.step(0.0);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.bodiesIntegrated, 0u);
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.0);
}

TEST(SimulationCore, StepRejectsNegativeDt) {
    RigidBodySolver s;
    s.addBody({1.0f});
    auto r = s.step(-0.016);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.bodiesIntegrated, 0u);
}

TEST(SimulationCore, StepAdvancesSimulationTime) {
    RigidBodySolver s;
    s.step(0.016);
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.016);
}

TEST(SimulationCore, StepCountsOnlyDynamicBodies) {
    RigidBodySolver s;
    s.addBody({1.0f}); // dynamic
    s.addBody({0.0f}); // static
    auto r = s.step(0.016);
    EXPECT_EQ(r.bodiesIntegrated, 1u);
}

TEST(SimulationCore, GravityAcceleratesBodyDownward) {
    RigidBodySolver s;
    s.setGravity({0.0f, -10.0f, 0.0f}); // g = 10 m/s² down

    SimBodyDesc desc;
    desc.mass     = 1.0f;
    desc.position = {0.0f, 100.0f, 0.0f};
    desc.velocity = {0.0f, 0.0f, 0.0f};
    BodyId id = s.addBody(desc);

    // dt = 1 s: a = (0,-10,0); v = (0,-10,0); p = (0,100,0) + (0,-10,0) = (0,90,0)
    s.step(1.0);
    SimVec3 pos, vel;
    s.getBodyState(id, pos, vel);
    EXPECT_TRUE(vec3Eq(vel, {0.0f, -10.0f, 0.0f}));
    EXPECT_TRUE(vec3Eq(pos, {0.0f, 90.0f, 0.0f}));
}

TEST(SimulationCore, AppliedForceClearedAfterStep) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f}); // no gravity

    BodyId id = s.addBody({1.0f, {0,0,0}, {0,0,0}});
    s.applyForce(id, {10.0f, 0.0f, 0.0f}); // F = 10 N, a = 10 m/s²

    // Step 1: v becomes 10*dt, position moves
    s.step(1.0);
    SimVec3 pos1, vel1;
    s.getBodyState(id, pos1, vel1);
    EXPECT_FLOAT_EQ(vel1.x, 10.0f); // a=10, dt=1 → v=10

    // Step 2: no force applied → velocity should remain constant (no gravity, no force)
    s.step(1.0);
    SimVec3 pos2, vel2;
    s.getBodyState(id, pos2, vel2);
    EXPECT_FLOAT_EQ(vel2.x, 10.0f); // unchanged — force was cleared
}

TEST(SimulationCore, ApplyForceReturnsFalseForUnknownBody) {
    RigidBodySolver s;
    EXPECT_FALSE(s.applyForce(99u, {1.0f, 0.0f, 0.0f}));
}

// ── State capture and rollback ────────────────────────────────────────────────

TEST(SimulationCore, CaptureStatePreservesAllBodies) {
    RigidBodySolver s;
    BodyId a = s.addBody({1.0f, {1,2,3}, {4,5,6}});
    BodyId b = s.addBody({2.0f, {7,8,9}, {0,0,0}});

    SimState state = s.captureState();
    EXPECT_EQ(state.bodies.size(), 2u);
    EXPECT_DOUBLE_EQ(state.simulationTime, 0.0);

    bool foundA = false, foundB = false;
    for (const auto& snap : state.bodies) {
        if (snap.id == a) { foundA = true; EXPECT_TRUE(vec3Eq(snap.position, {1,2,3})); }
        if (snap.id == b) { foundB = true; EXPECT_TRUE(vec3Eq(snap.position, {7,8,9})); }
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

TEST(SimulationCore, CaptureStatePreservesSimulationTime) {
    RigidBodySolver s;
    s.addBody({1.0f});
    s.step(0.5);
    SimState state = s.captureState();
    EXPECT_DOUBLE_EQ(state.simulationTime, 0.5);
}

TEST(SimulationCore, RestoreStateRollsBackPosition) {
    RigidBodySolver s;
    s.setGravity({0.0f, -10.0f, 0.0f});

    SimBodyDesc desc;
    desc.mass     = 1.0f;
    desc.position = {0.0f, 100.0f, 0.0f};
    BodyId id = s.addBody(desc);

    SimState snapshot = s.captureState(); // position = (0, 100, 0)

    s.step(1.0); // falls to (0, 90, 0)
    s.step(1.0); // falls further

    EXPECT_TRUE(s.restoreState(snapshot));

    SimVec3 pos, vel;
    s.getBodyState(id, pos, vel);
    EXPECT_TRUE(vec3Eq(pos, {0.0f, 100.0f, 0.0f}));
    EXPECT_DOUBLE_EQ(s.simulationTime(), 0.0);
}

TEST(SimulationCore, RestoreStateAllowsIdenticalReplay) {
    // Same initial state + same steps must produce identical trajectories
    // (determinism + rollback correctness).
    RigidBodySolver s;
    s.setGravity({0.0f, -9.81f, 0.0f});

    SimBodyDesc desc;
    desc.mass     = 3.0f;
    desc.position = {5.0f, 50.0f, 2.0f};
    desc.velocity = {1.0f, 0.0f, 0.0f};
    BodyId id = s.addBody(desc);

    SimState snapshot = s.captureState();

    // First run: 10 steps of 0.016 s
    std::vector<SimVec3> firstRunPositions;
    for (int i = 0; i < 10; ++i) {
        s.step(0.016);
        SimVec3 p, v;
        s.getBodyState(id, p, v);
        firstRunPositions.push_back(p);
    }

    // Restore and replay
    ASSERT_TRUE(s.restoreState(snapshot));
    for (int i = 0; i < 10; ++i) {
        s.step(0.016);
        SimVec3 p, v;
        s.getBodyState(id, p, v);
        EXPECT_TRUE(vec3Eq(p, firstRunPositions[static_cast<std::size_t>(i)], 1e-5f))
            << "Mismatch at step " << i;
    }
}

TEST(SimulationCore, RestoreStateReturnsFalseWhenSnapshotIsEmptyButSolverHasBodies) {
    RigidBodySolver s;
    s.addBody({1.0f});
    SimState empty;
    EXPECT_FALSE(s.restoreState(empty));
}

TEST(SimulationCore, EmptySolverRestoresFromEmptyStateSuccessfully) {
    RigidBodySolver s;
    SimState empty;
    EXPECT_TRUE(s.restoreState(empty)); // both empty — ok
}

// ── Determinism ───────────────────────────────────────────────────────────────

TEST(SimulationCore, IdenticalSetupProducesIdenticalTrajectory) {
    auto run = []() {
        RigidBodySolver s;
        s.setGravity({0.0f, -9.81f, 0.0f});
        SimBodyDesc desc;
        desc.mass     = 2.0f;
        desc.position = {0.0f, 50.0f, 0.0f};
        desc.velocity = {3.0f, 5.0f, 1.0f};
        BodyId id = s.addBody(desc);
        for (int i = 0; i < 60; ++i) {
            s.applyForce(id, {0.5f, 0.0f, 0.0f}); // constant thrust
            s.step(0.016);
        }
        SimVec3 p, v;
        s.getBodyState(id, p, v);
        return p;
    };

    SimVec3 a = run();
    SimVec3 b = run();
    EXPECT_TRUE(vec3Eq(a, b, 0.0f)); // bit-exact
}

TEST(SimulationCore, MultiBodyStepAllBodiesIntegrated) {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f});
    const int N = 5;
    for (int i = 0; i < N; ++i) s.addBody({1.0f});
    auto r = s.step(0.016);
    EXPECT_EQ(r.bodiesIntegrated, static_cast<std::size_t>(N));
}

TEST(SimulationCore, SimulationTimeAccumulatesAcrossMultipleSteps) {
    RigidBodySolver s;
    for (int i = 0; i < 10; ++i) s.step(0.016);
    // 0.016f is not exactly representable; repeated float→double conversion
    // accumulates ~1e-8 error — use a tolerance that reflects this reality.
    EXPECT_NEAR(s.simulationTime(), 10.0 * 0.016, 1e-6);
}
