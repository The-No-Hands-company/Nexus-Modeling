#include "nexus/sim/SimulationCore.h"

#include <algorithm>

namespace nexus {

RigidBodySolver::RigidBodySolver()  = default;
RigidBodySolver::~RigidBodySolver() = default;

// ── Body management ───────────────────────────────────────────────────────────

BodyId RigidBodySolver::addBody(const SimBodyDesc& desc) {
    BodyId id = m_nextId++;
    m_bodies.emplace(id, Body{
        id,
        desc.mass,
        desc.position,
        desc.velocity,
        /*force=*/{0.0f, 0.0f, 0.0f}
    });
    return id;
}

bool RigidBodySolver::removeBody(BodyId id) noexcept {
    return m_bodies.erase(id) > 0;
}

bool RigidBodySolver::hasBody(BodyId id) const noexcept {
    return findBody(id) != nullptr;
}

std::size_t RigidBodySolver::bodyCount() const noexcept {
    return m_bodies.size();
}

bool RigidBodySolver::getBodyState(BodyId id,
                                   SimVec3& outPosition,
                                   SimVec3& outVelocity) const noexcept {
    const Body* b = findBody(id);
    if (!b) return false;
    outPosition = b->position;
    outVelocity = b->velocity;
    return true;
}

// ── Force accumulation ────────────────────────────────────────────────────────

bool RigidBodySolver::applyForce(BodyId id, SimVec3 force) noexcept {
    Body* b = findBody(id);
    if (!b || b->mass == 0.0f) return false;
    b->force += force;
    return true;
}

void RigidBodySolver::setGravity(SimVec3 gravity) noexcept {
    m_gravity = gravity;
}

SimVec3 RigidBodySolver::gravity() const noexcept {
    return m_gravity;
}

// ── Simulation step ───────────────────────────────────────────────────────────

StepReport RigidBodySolver::step(double dt) {
    StepReport report;
    report.simulationTime   = m_time;
    report.bodiesIntegrated = 0;

    if (dt <= 0.0) {
        report.ok = false;
        return report;
    }

    // Physics integration runs at float precision (positions/velocities are float).
    // Time accumulation uses full double precision.
    const float fdt = static_cast<float>(dt);

    for (auto& [id, b] : m_bodies) {
        if (b.mass == 0.0f) continue; // static — skip

        // Gravity acceleration (uniform, applied every step).
        SimVec3 gravity_force = m_gravity * b.mass;

        // Total force = gravity + accumulated external forces.
        SimVec3 total = gravity_force + b.force;

        // Explicit Euler integration.
        SimVec3 acceleration = total * (1.0f / b.mass);
        b.velocity = b.velocity + acceleration * fdt;
        b.position = b.position + b.velocity * fdt;

        // Clear per-step accumulated force.
        b.force = {0.0f, 0.0f, 0.0f};

        ++report.bodiesIntegrated;
    }

    m_time += dt;
    report.simulationTime = m_time;
    return report;
}

// ── Replay and rollback ───────────────────────────────────────────────────────

SimState RigidBodySolver::captureState() const {
    SimState state;
    state.simulationTime = m_time;
    state.bodies.reserve(m_bodies.size());
    for (const auto& [id, b] : m_bodies) {
        state.bodies.push_back(SimBodySnapshot{b.id, b.position, b.velocity});
    }
    return state;
}

bool RigidBodySolver::restoreState(const SimState& state) {
    if (state.bodies.empty() && !m_bodies.empty()) {
        // Structurally invalid restore: snapshot has no bodies but solver does.
        return false;
    }

    m_time = state.simulationTime;
    for (const SimBodySnapshot& snap : state.bodies) {
        Body* b = findBody(snap.id);
        if (!b) continue; // body absent in solver — ignore
        b->position = snap.position;
        b->velocity = snap.velocity;
        b->force    = {0.0f, 0.0f, 0.0f}; // clear accumulated force on restore
    }
    return true;
}

// ── Time ──────────────────────────────────────────────────────────────────────

double RigidBodySolver::simulationTime() const noexcept {
    return m_time;
}

// ── Lifetime ──────────────────────────────────────────────────────────────────

void RigidBodySolver::clear() noexcept {
    m_bodies.clear();
    m_time = 0.0;
    // m_nextId not reset; m_gravity preserved.
}

// ── Private helpers ───────────────────────────────────────────────────────────

RigidBodySolver::Body* RigidBodySolver::findBody(BodyId id) noexcept {
    auto it = m_bodies.find(id);
    return it != m_bodies.end() ? &it->second : nullptr;
}

const RigidBodySolver::Body* RigidBodySolver::findBody(BodyId id) const noexcept {
    auto it = m_bodies.find(id);
    return it != m_bodies.end() ? &it->second : nullptr;
}

} // namespace nexus
