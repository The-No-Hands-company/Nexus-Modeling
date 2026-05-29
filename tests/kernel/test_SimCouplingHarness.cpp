// Tests for nexus::sim::SimCouplingHarness (Month 13 Track C prototype).
//
// Verifies the BodyId → SceneNodeId registry, syncFromSolver snapshot
// correctness, determinism, and isolation between two independent harnesses.

#include <nexus/sim/SimCouplingHarness.h>
#include <nexus/gfx/RenderContext.h>

#include <gtest/gtest.h>

using namespace nexus;
using namespace nexus::sim;

// ── Helpers ──────────────────────────────────────────────────────────────────

static RigidBodySolver makeSolver() {
    RigidBodySolver s;
    s.setGravity({0.0f, 0.0f, 0.0f}); // disable gravity for predictable positions
    return s;
}

static BodyId addBody(RigidBodySolver& s, SimVec3 pos, SimVec3 vel = {}) {
    return s.addBody(SimBodyDesc{1.0f, pos, vel});
}

// ── Registration ─────────────────────────────────────────────────────────────

TEST(SimCouplingHarness, RegisterSucceeds) {
    SimCouplingHarness h;
    EXPECT_TRUE(h.registerBody(1u, 10u));
    EXPECT_EQ(h.registrationCount(), 1u);
    EXPECT_TRUE(h.isRegistered(1u));
    EXPECT_EQ(h.sceneNodeFor(1u), 10u);
    EXPECT_EQ(h.bodyForSceneNode(10u), 1u);
}

TEST(SimCouplingHarness, RegisterInvalidBodyIdFails) {
    SimCouplingHarness h;
    EXPECT_FALSE(h.registerBody(kInvalidBodyId, 10u));
    EXPECT_EQ(h.registrationCount(), 0u);
}

TEST(SimCouplingHarness, RegisterInvalidSceneNodeIdFails) {
    SimCouplingHarness h;
    EXPECT_FALSE(h.registerBody(1u, kInvalidSceneNodeId));
    EXPECT_EQ(h.registrationCount(), 0u);
}

TEST(SimCouplingHarness, RegisterDuplicateBodyFails) {
    SimCouplingHarness h;
    EXPECT_TRUE(h.registerBody(1u, 10u));
    EXPECT_FALSE(h.registerBody(1u, 20u)); // body already registered
    EXPECT_EQ(h.registrationCount(), 1u);
}

TEST(SimCouplingHarness, RegisterDuplicateSceneNodeFails) {
    SimCouplingHarness h;
    EXPECT_TRUE(h.registerBody(1u, 10u));
    EXPECT_FALSE(h.registerBody(2u, 10u)); // scene node already registered
    EXPECT_EQ(h.registrationCount(), 1u);
}

TEST(SimCouplingHarness, MultipleRegistrations) {
    SimCouplingHarness h;
    EXPECT_TRUE(h.registerBody(1u, 10u));
    EXPECT_TRUE(h.registerBody(2u, 20u));
    EXPECT_TRUE(h.registerBody(3u, 30u));
    EXPECT_EQ(h.registrationCount(), 3u);
    EXPECT_EQ(h.sceneNodeFor(2u), 20u);
    EXPECT_EQ(h.bodyForSceneNode(30u), 3u);
}

TEST(SimCouplingHarness, UnregisterSucceeds) {
    SimCouplingHarness h;
    h.registerBody(1u, 10u);
    EXPECT_TRUE(h.unregisterBody(1u));
    EXPECT_EQ(h.registrationCount(), 0u);
    EXPECT_FALSE(h.isRegistered(1u));
    EXPECT_EQ(h.sceneNodeFor(1u), kInvalidSceneNodeId);
    EXPECT_EQ(h.bodyForSceneNode(10u), kInvalidBodyId);
}

TEST(SimCouplingHarness, UnregisterUnknownBodyFails) {
    SimCouplingHarness h;
    EXPECT_FALSE(h.unregisterBody(99u));
}

TEST(SimCouplingHarness, UnregisterFreesSceneNodeSlot) {
    SimCouplingHarness h;
    h.registerBody(1u, 10u);
    h.unregisterBody(1u);
    // scene node 10 is now free
    EXPECT_TRUE(h.registerBody(2u, 10u));
    EXPECT_EQ(h.registrationCount(), 1u);
}

// ── syncFromSolver ────────────────────────────────────────────────────────────

TEST(SimCouplingHarness, SyncFromSolverCapturesPosition) {
    auto s = makeSolver();
    BodyId b = addBody(s, {1.0f, 2.0f, 3.0f});

    SimCouplingHarness h;
    h.registerBody(b, 100u);
    std::size_t count = h.syncFromSolver(s);

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(h.syncedCount(), 1u);

    auto pos = h.lastPosition(b);
    ASSERT_TRUE(pos.has_value());
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(pos->y, 2.0f);
    EXPECT_FLOAT_EQ(pos->z, 3.0f);
}

TEST(SimCouplingHarness, SyncFromSolverCapturesVelocity) {
    auto s = makeSolver();
    BodyId b = addBody(s, {}, {4.0f, 5.0f, 6.0f});

    SimCouplingHarness h;
    h.registerBody(b, 100u);
    h.syncFromSolver(s);

    auto vel = h.lastVelocity(b);
    ASSERT_TRUE(vel.has_value());
    EXPECT_FLOAT_EQ(vel->x, 4.0f);
    EXPECT_FLOAT_EQ(vel->y, 5.0f);
    EXPECT_FLOAT_EQ(vel->z, 6.0f);
}

TEST(SimCouplingHarness, SyncFromSolverReflectsMovement) {
    auto s = makeSolver();
    BodyId b = addBody(s, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}); // moving at 1 m/s in X

    SimCouplingHarness h;
    h.registerBody(b, 100u);

    h.syncFromSolver(s);
    auto pos0 = h.lastPosition(b);
    ASSERT_TRUE(pos0.has_value());
    EXPECT_FLOAT_EQ(pos0->x, 0.0f);

    s.step(2.0); // 2 seconds → x should be 2.0
    h.syncFromSolver(s);
    auto pos2 = h.lastPosition(b);
    ASSERT_TRUE(pos2.has_value());
    EXPECT_FLOAT_EQ(pos2->x, 2.0f);
}

TEST(SimCouplingHarness, SyncBodyAbsentFromSolverSkipped) {
    auto s = makeSolver();
    SimCouplingHarness h;
    // Register a body ID that was never added to the solver.
    h.registerBody(42u, 100u);

    std::size_t count = h.syncFromSolver(s);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(h.syncedCount(), 0u);
    EXPECT_FALSE(h.lastPosition(42u).has_value());
}

TEST(SimCouplingHarness, SyncMultipleBodies) {
    auto s = makeSolver();
    BodyId b1 = addBody(s, {1.0f, 0.0f, 0.0f});
    BodyId b2 = addBody(s, {0.0f, 2.0f, 0.0f});
    BodyId b3 = addBody(s, {0.0f, 0.0f, 3.0f});

    SimCouplingHarness h;
    h.registerBody(b1, 10u);
    h.registerBody(b2, 20u);
    h.registerBody(b3, 30u);

    EXPECT_EQ(h.syncFromSolver(s), 3u);
    EXPECT_FLOAT_EQ(h.lastPosition(b1)->x, 1.0f);
    EXPECT_FLOAT_EQ(h.lastPosition(b2)->y, 2.0f);
    EXPECT_FLOAT_EQ(h.lastPosition(b3)->z, 3.0f);
}

TEST(SimCouplingHarness, SyncRebuildsSnanphotTableEachCall) {
    auto s = makeSolver();
    BodyId b = addBody(s, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});

    SimCouplingHarness h;
    h.registerBody(b, 100u);

    h.syncFromSolver(s);
    s.step(1.0);
    h.syncFromSolver(s);

    // After two syncs only the last snapshot is visible.
    EXPECT_FLOAT_EQ(h.lastPosition(b)->x, 1.0f);
    EXPECT_EQ(h.syncedCount(), 1u);
}

// ── Queries before sync ───────────────────────────────────────────────────────

TEST(SimCouplingHarness, QueryBeforeSyncReturnsNullopt) {
    SimCouplingHarness h;
    h.registerBody(1u, 10u);
    EXPECT_FALSE(h.lastState(1u).has_value());
    EXPECT_FALSE(h.lastPosition(1u).has_value());
    EXPECT_FALSE(h.lastVelocity(1u).has_value());
    EXPECT_EQ(h.syncedCount(), 0u);
}

TEST(SimCouplingHarness, QueryUnregisteredBodyReturnsNullopt) {
    SimCouplingHarness h;
    EXPECT_FALSE(h.lastState(99u).has_value());
    EXPECT_FALSE(h.lastPosition(99u).has_value());
    EXPECT_FALSE(h.lastVelocity(99u).has_value());
}

TEST(SimCouplingHarness, UnregisterClearsSnapshot) {
    auto s = makeSolver();
    BodyId b = addBody(s, {5.0f, 0.0f, 0.0f});

    SimCouplingHarness h;
    h.registerBody(b, 100u);
    h.syncFromSolver(s);
    EXPECT_TRUE(h.lastPosition(b).has_value());

    h.unregisterBody(b);
    EXPECT_FALSE(h.lastPosition(b).has_value());
    EXPECT_EQ(h.syncedCount(), 0u);
}

// ── Determinism ───────────────────────────────────────────────────────────────

TEST(SimCouplingHarness, DeterministicAcrossTwoHarnesses) {
    auto runSync = [](float initX) -> SimVec3 {
        RigidBodySolver s;
        s.setGravity({0.0f, 0.0f, 0.0f});
        BodyId b = s.addBody(SimBodyDesc{1.0f, {initX, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}});
        s.step(3.0);

        SimCouplingHarness h;
        h.registerBody(b, 1u);
        h.syncFromSolver(s);
        return *h.lastPosition(b);
    };

    SimVec3 r1 = runSync(0.0f);
    SimVec3 r2 = runSync(0.0f);
    EXPECT_EQ(r1.x, r2.x);
    EXPECT_EQ(r1.y, r2.y);
    EXPECT_EQ(r1.z, r2.z);
}

TEST(SimCouplingHarness, TwoHarnessesAreIsolated) {
    auto s = makeSolver();
    BodyId b1 = addBody(s, {1.0f, 0.0f, 0.0f});
    BodyId b2 = addBody(s, {2.0f, 0.0f, 0.0f});

    SimCouplingHarness h1, h2;
    h1.registerBody(b1, 10u);
    h2.registerBody(b2, 20u);

    h1.syncFromSolver(s);
    h2.syncFromSolver(s);

    EXPECT_FLOAT_EQ(h1.lastPosition(b1)->x, 1.0f);
    EXPECT_FALSE(h1.lastPosition(b2).has_value());

    EXPECT_FLOAT_EQ(h2.lastPosition(b2)->x, 2.0f);
    EXPECT_FALSE(h2.lastPosition(b1).has_value());
}

// ── Clear ─────────────────────────────────────────────────────────────────────

TEST(SimCouplingHarness, ClearResetsEverything) {
    auto s = makeSolver();
    BodyId b = addBody(s, {1.0f, 2.0f, 3.0f});

    SimCouplingHarness h;
    h.registerBody(b, 100u);
    h.syncFromSolver(s);

    h.clear();
    EXPECT_EQ(h.registrationCount(), 0u);
    EXPECT_EQ(h.syncedCount(), 0u);
    EXPECT_FALSE(h.isRegistered(b));
    EXPECT_EQ(h.sceneNodeFor(b), kInvalidSceneNodeId);
    EXPECT_FALSE(h.lastPosition(b).has_value());
}

TEST(SimCouplingHarness, RegisterAfterClear) {
    SimCouplingHarness h;
    h.registerBody(1u, 10u);
    h.clear();
    EXPECT_TRUE(h.registerBody(1u, 10u));
    EXPECT_EQ(h.registrationCount(), 1u);
}

// ── Feature toggle: RenderContextDesc::enableSimCoupling ────────────────────

TEST(SimCouplingHarness, RenderContextDescToggleDefaultsFalse) {
    nexus::gfx::RenderContextDesc desc;
    EXPECT_FALSE(desc.enableSimCoupling);
}

TEST(SimCouplingHarness, RenderContextDescToggleCanBeEnabled) {
    nexus::gfx::RenderContextDesc desc;
    desc.enableSimCoupling = true;
    EXPECT_TRUE(desc.enableSimCoupling);
}
