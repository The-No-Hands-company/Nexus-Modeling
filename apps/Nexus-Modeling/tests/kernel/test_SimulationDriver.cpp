#include <gtest/gtest.h>

#include <nexus/sim/SimulationDriver.h>
#include <nexus/sim/SimulationCore.h>
#include <nexus/sim/SimulationCoupling.h>
#include <nexus/render/SceneGraph.h>

#include <cmath>
#include <limits>
#include <span>

using namespace nexus;

namespace {

bool approxEq(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

bool quatIsUnit(const SimQuat& q, float eps = 1e-4f) {
    const float l = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    return std::fabs(l - 1.0f) <= eps;
}

} // namespace

// ── Fixed-timestep accumulation ─────────────────────────────────────────────

TEST(SimulationDriver, AdvanceTakesFixedStepsAndExposesAlpha) {
    RigidBodySolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    (void)solver.addBody({1.0f});

    sim::SimulationDriver driver(0.1);
    const std::uint32_t steps = driver.advance(solver, 0.25); // 0.25 / 0.1 -> 2 steps, 0.05 left

    EXPECT_EQ(steps, 2u);
    EXPECT_TRUE(approxEq(driver.alpha(), 0.5f)); // 0.05 / 0.1
    EXPECT_NEAR(solver.simulationTime(), 0.2, 1e-9);
}

TEST(SimulationDriver, FrameRateIndependenceProducesIdenticalSolverState) {
    // The whole point of fixed-timestep: trajectory is independent of frame
    // pacing. One big frame vs many small frames covering the same wall time
    // must leave the solver in bit-identical state.
    auto makeSolver = []() {
        RigidBodySolver s;
        s.setGravity({0.0f, -10.0f, 0.0f});
        SimBodyDesc d; d.mass = 1.0f; d.position = {0.0f, 100.0f, 0.0f};
        (void)s.addBody(d);
        return s;
    };

    RigidBodySolver coarse = makeSolver();
    RigidBodySolver fine   = makeSolver();

    // Cap above the 10-step catch-up so neither path is throttled by the
    // spiral-of-death guard (the property holds within the substep budget).
    sim::SimulationDriver dc(0.1, 32);
    sim::SimulationDriver df(0.1, 32);

    EXPECT_EQ(dc.advance(coarse, 1.0), 10u);     // one 1.0s frame
    std::uint32_t fineSteps = 0;
    for (int i = 0; i < 10; ++i) fineSteps += df.advance(fine, 0.1); // ten 0.1s frames
    EXPECT_EQ(fineSteps, 10u);

    EXPECT_EQ(coarse.captureState(), fine.captureState());
}

TEST(SimulationDriver, AdvanceIgnoresNonFiniteOrNonPositiveFrameDt) {
    RigidBodySolver solver;
    (void)solver.addBody({1.0f});
    sim::SimulationDriver driver(0.1);

    EXPECT_EQ(driver.advance(solver, 0.0), 0u);
    EXPECT_EQ(driver.advance(solver, -1.0), 0u);
    EXPECT_EQ(driver.advance(solver, std::numeric_limits<double>::quiet_NaN()), 0u);
    EXPECT_EQ(driver.advance(solver, std::numeric_limits<double>::infinity()), 0u);
    EXPECT_DOUBLE_EQ(solver.simulationTime(), 0.0);
}

TEST(SimulationDriver, SpiralOfDeathGuardCapsSubstepsAndDoesNotAccumulateBacklog) {
    RigidBodySolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    (void)solver.addBody({1.0f});

    sim::SimulationDriver driver(0.01, /*maxSubstepsPerAdvance=*/4);

    EXPECT_EQ(driver.advance(solver, 1.0), 4u);   // would be 100 steps; capped at 4
    EXPECT_LE(driver.accumulator(), driver.fixedTimestep()); // backlog discarded
    EXPECT_EQ(driver.advance(solver, 1.0), 4u);   // still capped, no runaway
}

// ── Interpolation ───────────────────────────────────────────────────────────

TEST(SimulationDriver, InterpolatedStateBlendsPreviousAndCurrent) {
    RigidBodySolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d; d.mass = 1.0f; d.velocity = {10.0f, 0.0f, 0.0f}; // 10 units/s along +X
    const BodyId id = solver.addBody(d);

    sim::SimulationDriver driver(0.1);
    const std::uint32_t steps = driver.advance(solver, 0.15); // 1 step, 0.05 remainder

    EXPECT_EQ(steps, 1u);
    EXPECT_TRUE(approxEq(driver.alpha(), 0.5f));

    const SimState interp = driver.interpolatedState();
    ASSERT_EQ(interp.bodies.size(), 1u);
    EXPECT_EQ(interp.bodies[0].id, id);
    // previous x = 0, current x = 1.0 (10 * 0.1); halfway -> 0.5
    EXPECT_TRUE(approxEq(interp.bodies[0].position.x, 0.5f));
}

TEST(SimulationDriver, InterpolateSimStateNlerpsOrientationAndHitsEndpoints) {
    SimState from; from.simulationTime = 0.0;
    SimState to;   to.simulationTime = 1.0;

    SimBodySnapshot a{}; a.id = 1; a.position = {0,0,0};
    a.orientation = {0.0f, 0.0f, 0.0f, 1.0f};                 // identity
    SimBodySnapshot b{}; b.id = 1; b.position = {2,0,0};
    b.orientation = {0.0f, 0.70710678f, 0.0f, 0.70710678f};  // 90 deg about Y
    from.bodies.push_back(a);
    to.bodies.push_back(b);

    const SimState at0  = sim::interpolateSimState(from, to, 0.0f);
    const SimState at1  = sim::interpolateSimState(from, to, 1.0f);
    const SimState atHalf = sim::interpolateSimState(from, to, 0.5f);

    EXPECT_EQ(at0.bodies[0].orientation, a.orientation);
    EXPECT_TRUE(approxEq(at1.bodies[0].orientation.y, 0.70710678f));
    EXPECT_TRUE(approxEq(at1.bodies[0].orientation.w, 0.70710678f));
    EXPECT_TRUE(approxEq(atHalf.bodies[0].position.x, 1.0f));
    EXPECT_TRUE(quatIsUnit(atHalf.bodies[0].orientation)); // nlerp stays on the unit sphere
}

TEST(SimulationDriver, AlphaIsClampedAndZeroBeforeInitialization) {
    sim::SimulationDriver driver(0.1);
    EXPECT_FALSE(driver.initialized());
    EXPECT_GE(driver.alpha(), 0.0f);
    EXPECT_LE(driver.alpha(), 1.0f);
    EXPECT_TRUE(driver.interpolatedState().bodies.empty());
}

TEST(SimulationDriver, ResetSeedsSnapshotsFromSolver) {
    RigidBodySolver solver;
    SimBodyDesc d; d.mass = 1.0f; d.position = {3.0f, 4.0f, 5.0f};
    (void)solver.addBody(d);

    sim::SimulationDriver driver(0.1);
    driver.reset(solver);

    EXPECT_TRUE(driver.initialized());
    ASSERT_EQ(driver.currentState().bodies.size(), 1u);
    EXPECT_TRUE(approxEq(driver.currentState().bodies[0].position.y, 4.0f));
    EXPECT_EQ(driver.previousState(), driver.currentState());
}

// ── End-to-end: driver feeds interpolated transform into the coupling ────────

TEST(SimulationDriver, DriverInterpolatedStateDrivesCoupledSceneNode) {
    render::SceneGraph scene;
    render::Node* node = scene.createNode("body", &scene.root());
    ASSERT_NE(node, nullptr);

    RigidBodySolver solver;
    solver.setGravity({0.0f, 0.0f, 0.0f});
    SimBodyDesc d; d.mass = 1.0f; d.velocity = {10.0f, 0.0f, 0.0f};
    const BodyId id = solver.addBody(d);

    sim::SimulationSceneCoupling coupling(scene);
    sim::SimulationSceneBinding binding{};
    binding.simBodyId   = id;
    binding.sceneNodeId = node->id();
    coupling.setBindings(std::span{&binding, 1});

    sim::SimulationDriver driver(0.1);
    (void)driver.advance(solver, 0.15); // 1 step, alpha 0.5

    coupling.applyState(driver.interpolatedState());

    // Smooth, framerate-decoupled transform: halfway between 0 and 1.0.
    EXPECT_TRUE(approxEq(node->localTransform().translation.x, 0.5f));
}
