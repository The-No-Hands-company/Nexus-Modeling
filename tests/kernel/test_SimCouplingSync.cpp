// Tests for SimCouplingHarness::syncToScene and NodePayload::SimTransform.
//
// Covers:
//   - SimTransform payload type accessor round-trip in NodePayload.
//   - syncToScene writes SimTransform payloads to scene nodes.
//   - syncToScene returns correct updated count.
//   - Scene nodes absent from the NodeScene are skipped gracefully.
//   - Multiple bodies update independent nodes.
//   - syncToScene is idempotent (second call overwrites with same values).
//   - Determinism: two independent harnesses produce the same scene state.
//   - syncToScene does NOT write to nodes whose body is absent from the solver.
//   - NodePayloadType::SimTransform is correctly identified.
//   - SimTransform default values are zero.

#include <nexus/sim/SimCouplingHarness.h>
#include <nexus/scene/NodeScene.h>
#include <nexus/eval/EvalGraph.h>

#include <gtest/gtest.h>

using namespace nexus;
using namespace nexus::sim;
using nexus::NodeScene;
using nexus::NodePayload;
using nexus::NodePayloadType;

// ── Helpers ───────────────────────────────────────────────────────────────────

static RigidBodySolver makeSolver()
{
    RigidBodySolver s;
    s.setGravity({0.f, 0.f, 0.f});
    return s;
}

static BodyId addBody(RigidBodySolver& s, SimVec3 pos, SimVec3 vel = {})
{
    return s.addBody(SimBodyDesc{1.f, pos, vel});
}

// ── SimTransform payload type ─────────────────────────────────────────────────

TEST(SimCouplingSync, SimTransformPayloadDefaultIsZero)
{
    NodePayload::SimTransform st{};
    EXPECT_FLOAT_EQ(st.px, 0.f);
    EXPECT_FLOAT_EQ(st.py, 0.f);
    EXPECT_FLOAT_EQ(st.pz, 0.f);
    EXPECT_FLOAT_EQ(st.vx, 0.f);
    EXPECT_FLOAT_EQ(st.vy, 0.f);
    EXPECT_FLOAT_EQ(st.vz, 0.f);
}

TEST(SimCouplingSync, NodePayloadTypeSimTransform)
{
    NodePayload p;
    p.value = NodePayload::SimTransform{1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    EXPECT_EQ(p.type(), NodePayloadType::SimTransform);
}

TEST(SimCouplingSync, SimTransformAccessorReturnsCorrectValues)
{
    NodePayload p;
    p.value = NodePayload::SimTransform{1.f, 2.f, 3.f, 0.1f, 0.2f, 0.3f};
    const auto* st = p.simTransform();
    ASSERT_NE(st, nullptr);
    EXPECT_FLOAT_EQ(st->px, 1.f);
    EXPECT_FLOAT_EQ(st->py, 2.f);
    EXPECT_FLOAT_EQ(st->pz, 3.f);
    EXPECT_FLOAT_EQ(st->vx, 0.1f);
    EXPECT_FLOAT_EQ(st->vy, 0.2f);
    EXPECT_FLOAT_EQ(st->vz, 0.3f);
}

TEST(SimCouplingSync, SimTransformAccessorNullForOtherTypes)
{
    NodePayload p;
    p.value = 3.14f;
    EXPECT_EQ(p.simTransform(), nullptr);
}

TEST(SimCouplingSync, MutableSimTransformAccessor)
{
    NodePayload p;
    p.value = NodePayload::SimTransform{};
    auto* st = p.simTransform();
    ASSERT_NE(st, nullptr);
    st->px = 99.f;
    EXPECT_FLOAT_EQ(p.simTransform()->px, 99.f);
}

// ── syncToScene basic behaviour ───────────────────────────────────────────────

TEST(SimCouplingSync, SyncToSceneWritesPositionToNode)
{
    auto solver = makeSolver();
    BodyId b = addBody(solver, {10.f, 20.f, 30.f}, {1.f, 2.f, 3.f});

    NodeScene scene;
    SceneNodeId n = scene.addNode("body_node", NodeKind::Geometry);

    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b, n));

    std::size_t updated = h.syncToScene(solver, scene);
    EXPECT_EQ(updated, 1u);

    const NodePayload* p = scene.asset(n);
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(p->type(), NodePayloadType::SimTransform);
    const auto* st = p->simTransform();
    ASSERT_NE(st, nullptr);
    EXPECT_FLOAT_EQ(st->px, 10.f);
    EXPECT_FLOAT_EQ(st->py, 20.f);
    EXPECT_FLOAT_EQ(st->pz, 30.f);
}

TEST(SimCouplingSync, SyncToSceneWritesVelocityToNode)
{
    auto solver = makeSolver();
    BodyId b = addBody(solver, {0.f, 0.f, 0.f}, {5.f, 6.f, 7.f});

    NodeScene scene;
    SceneNodeId n = scene.addNode("body_node", NodeKind::Geometry);

    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b, n));
    h.syncToScene(solver, scene);

    const auto* st = scene.asset(n)->simTransform();
    ASSERT_NE(st, nullptr);
    EXPECT_FLOAT_EQ(st->vx, 5.f);
    EXPECT_FLOAT_EQ(st->vy, 6.f);
    EXPECT_FLOAT_EQ(st->vz, 7.f);
}

TEST(SimCouplingSync, SyncToSceneReturnsZeroForNoRegistrations)
{
    auto solver = makeSolver();
    addBody(solver, {1.f, 2.f, 3.f});

    NodeScene scene;
    SimCouplingHarness h;
    EXPECT_EQ(h.syncToScene(solver, scene), 0u);
}

TEST(SimCouplingSync, SyncToSceneSkipsMissingSceneNode)
{
    auto solver = makeSolver();
    BodyId b = addBody(solver, {1.f, 0.f, 0.f});

    NodeScene scene;
    // Register against a scene-node id that does not exist in the scene.
    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b, 999u));

    // Should not crash, returns 0 updated.
    EXPECT_EQ(h.syncToScene(solver, scene), 0u);
}

TEST(SimCouplingSync, SyncToSceneMultipleBodies)
{
    auto solver = makeSolver();
    BodyId b1 = addBody(solver, {1.f, 0.f, 0.f});
    BodyId b2 = addBody(solver, {0.f, 2.f, 0.f});
    BodyId b3 = addBody(solver, {0.f, 0.f, 3.f});

    NodeScene scene;
    SceneNodeId n1 = scene.addNode("n1", NodeKind::Geometry);
    SceneNodeId n2 = scene.addNode("n2", NodeKind::Geometry);
    SceneNodeId n3 = scene.addNode("n3", NodeKind::Geometry);

    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b1, n1));
    ASSERT_TRUE(h.registerBody(b2, n2));
    ASSERT_TRUE(h.registerBody(b3, n3));

    EXPECT_EQ(h.syncToScene(solver, scene), 3u);

    EXPECT_FLOAT_EQ(scene.asset(n1)->simTransform()->px, 1.f);
    EXPECT_FLOAT_EQ(scene.asset(n2)->simTransform()->py, 2.f);
    EXPECT_FLOAT_EQ(scene.asset(n3)->simTransform()->pz, 3.f);
}

TEST(SimCouplingSync, SyncToSceneIsIdempotent)
{
    auto solver = makeSolver();
    BodyId b = addBody(solver, {7.f, 8.f, 9.f});

    NodeScene scene;
    SceneNodeId n = scene.addNode("n", NodeKind::Geometry);

    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b, n));

    h.syncToScene(solver, scene);
    h.syncToScene(solver, scene); // second call overwrites with same values

    const auto* st = scene.asset(n)->simTransform();
    ASSERT_NE(st, nullptr);
    EXPECT_FLOAT_EQ(st->px, 7.f);
    EXPECT_FLOAT_EQ(st->py, 8.f);
    EXPECT_FLOAT_EQ(st->pz, 9.f);
}

TEST(SimCouplingSync, SyncToSceneReflectsSolverStep)
{
    auto solver = makeSolver();
    solver.setGravity({0.f, -10.f, 0.f});
    BodyId b = addBody(solver, {0.f, 100.f, 0.f}, {0.f, 0.f, 0.f});

    NodeScene scene;
    SceneNodeId n = scene.addNode("falling", NodeKind::Geometry);

    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b, n));

    h.syncToScene(solver, scene);
    const float y0 = scene.asset(n)->simTransform()->py;

    solver.step(0.1f);
    h.syncToScene(solver, scene);
    const float y1 = scene.asset(n)->simTransform()->py;

    EXPECT_LT(y1, y0); // body fell under gravity
}

// ── syncToScene also updates the snapshot cache ───────────────────────────────

TEST(SimCouplingSync, SyncToSceneAlsoUpdatesSnapshotCache)
{
    auto solver = makeSolver();
    BodyId b = addBody(solver, {3.f, 4.f, 5.f});

    NodeScene scene;
    SceneNodeId n = scene.addNode("n", NodeKind::Geometry);

    SimCouplingHarness h;
    ASSERT_TRUE(h.registerBody(b, n));

    h.syncToScene(solver, scene);

    // Snapshot cache should also be populated.
    auto snap = h.lastState(b);
    ASSERT_TRUE(snap.has_value());
    EXPECT_FLOAT_EQ(snap->position.x, 3.f);
    EXPECT_FLOAT_EQ(snap->position.y, 4.f);
    EXPECT_FLOAT_EQ(snap->position.z, 5.f);
}

// ── Determinism ───────────────────────────────────────────────────────────────

TEST(SimCouplingSync, SyncToSceneIsDeterministicAcrossHarnesses)
{
    auto solver1 = makeSolver();
    auto solver2 = makeSolver();
    BodyId b1 = addBody(solver1, {1.f, 2.f, 3.f}, {0.1f, 0.2f, 0.3f});
    BodyId b2 = addBody(solver2, {1.f, 2.f, 3.f}, {0.1f, 0.2f, 0.3f});

    NodeScene scene1, scene2;
    SceneNodeId n1 = scene1.addNode("n", NodeKind::Geometry);
    SceneNodeId n2 = scene2.addNode("n", NodeKind::Geometry);

    SimCouplingHarness h1, h2;
    ASSERT_TRUE(h1.registerBody(b1, n1));
    ASSERT_TRUE(h2.registerBody(b2, n2));

    h1.syncToScene(solver1, scene1);
    h2.syncToScene(solver2, scene2);

    const auto* st1 = scene1.asset(n1)->simTransform();
    const auto* st2 = scene2.asset(n2)->simTransform();
    ASSERT_NE(st1, nullptr);
    ASSERT_NE(st2, nullptr);
    EXPECT_FLOAT_EQ(st1->px, st2->px);
    EXPECT_FLOAT_EQ(st1->py, st2->py);
    EXPECT_FLOAT_EQ(st1->pz, st2->pz);
    EXPECT_FLOAT_EQ(st1->vx, st2->vx);
    EXPECT_FLOAT_EQ(st1->vy, st2->vy);
    EXPECT_FLOAT_EQ(st1->vz, st2->vz);
}

// ── NodePayloadType coverage ──────────────────────────────────────────────────

TEST(SimCouplingSync, NonePayloadHasNoSimTransform)
{
    NodePayload p;
    EXPECT_EQ(p.type(), NodePayloadType::None);
    EXPECT_EQ(p.simTransform(), nullptr);
}
