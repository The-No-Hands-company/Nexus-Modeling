#include <nexus/sim/SimCouplingHarness.h>

namespace nexus::sim {

SimCouplingHarness::SimCouplingHarness() = default;
SimCouplingHarness::~SimCouplingHarness() = default;

// ── Registration ─────────────────────────────────────────────────────────────

bool SimCouplingHarness::registerBody(BodyId bodyId, SceneNodeId sceneNodeId) {
    if (bodyId == kInvalidBodyId) return false;
    if (sceneNodeId == kInvalidSceneNodeId) return false;
    if (m_bodyToNode.count(bodyId)) return false;
    if (m_nodeToBody.count(sceneNodeId)) return false;

    m_bodyToNode[bodyId]       = sceneNodeId;
    m_nodeToBody[sceneNodeId]  = bodyId;
    return true;
}

bool SimCouplingHarness::unregisterBody(BodyId bodyId) noexcept {
    auto it = m_bodyToNode.find(bodyId);
    if (it == m_bodyToNode.end()) return false;

    m_nodeToBody.erase(it->second);
    m_bodyToNode.erase(it);
    m_snapshots.erase(bodyId);
    return true;
}

bool SimCouplingHarness::isRegistered(BodyId bodyId) const noexcept {
    return m_bodyToNode.count(bodyId) > 0;
}

SceneNodeId SimCouplingHarness::sceneNodeFor(BodyId bodyId) const noexcept {
    auto it = m_bodyToNode.find(bodyId);
    return it != m_bodyToNode.end() ? it->second : kInvalidSceneNodeId;
}

BodyId SimCouplingHarness::bodyForSceneNode(SceneNodeId id) const noexcept {
    auto it = m_nodeToBody.find(id);
    return it != m_nodeToBody.end() ? it->second : kInvalidBodyId;
}

std::size_t SimCouplingHarness::registrationCount() const noexcept {
    return m_bodyToNode.size();
}

// ── Sync ─────────────────────────────────────────────────────────────────────

std::size_t SimCouplingHarness::syncFromSolver(const RigidBodySolver& solver) {
    m_snapshots.clear();
    std::size_t synced = 0;

    for (const auto& [bodyId, nodeId] : m_bodyToNode) {
        SimVec3 pos{}, vel{};
        if (!solver.getBodyState(bodyId, pos, vel)) continue;
        m_snapshots[bodyId] = SimCoupledState{pos, vel};
        ++synced;
    }
    return synced;
}

// ── Snapshot queries ──────────────────────────────────────────────────────────

std::optional<SimCoupledState> SimCouplingHarness::lastState(BodyId bodyId) const noexcept {
    auto it = m_snapshots.find(bodyId);
    if (it == m_snapshots.end()) return std::nullopt;
    return it->second;
}

std::optional<SimVec3> SimCouplingHarness::lastPosition(BodyId bodyId) const noexcept {
    auto s = lastState(bodyId);
    if (!s) return std::nullopt;
    return s->position;
}

std::optional<SimVec3> SimCouplingHarness::lastVelocity(BodyId bodyId) const noexcept {
    auto s = lastState(bodyId);
    if (!s) return std::nullopt;
    return s->velocity;
}

std::size_t SimCouplingHarness::syncedCount() const noexcept {
    return m_snapshots.size();
}

// ── Lifetime ──────────────────────────────────────────────────────────────────

void SimCouplingHarness::clear() noexcept {
    m_bodyToNode.clear();
    m_nodeToBody.clear();
    m_snapshots.clear();
}

} // namespace nexus::sim
