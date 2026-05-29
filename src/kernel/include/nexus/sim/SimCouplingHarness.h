#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sim — SimCouplingHarness (Month 13 Track C prototype)
//
//  Cross-domain bridge that maps RigidBodySolver bodies to NodeScene nodes
//  and snapshots their state on demand.  CPU-only; no GPU dependency.
//  See docs/design-sim-coupling-month-13.md for the full design.
//
//  Determinism contract:
//    Identical solver state + identical registration sequence →
//    identical syncFromSolver snapshot table.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/eval/EvalGraph.h>
#include <nexus/sim/SimulationCore.h>
#include <nexus/scene/NodeScene.h>

#include <optional>
#include <unordered_map>

namespace nexus::sim {

/// Snapshot of a single body's state at the last syncFromSolver call.
struct SimCoupledState {
    SimVec3 position = {0.0f, 0.0f, 0.0f};
    SimVec3 velocity = {0.0f, 0.0f, 0.0f};
};

/// Bridges RigidBodySolver bodies to NodeScene nodes.
///
/// The harness maintains a one-to-one BodyId → SceneNodeId registry and a
/// cached snapshot table populated by syncFromSolver().  Neither the solver
/// nor the scene are mutated by the harness; it is the sole cross-domain
/// object.
///
/// Thread-safety: none.
class SimCouplingHarness {
public:
    SimCouplingHarness();
    ~SimCouplingHarness();

    // ── Registration ─────────────────────────────────────────────────────────

    /// Register a body → scene-node pair.
    ///
    /// Returns false when:
    ///   - `bodyId`     is kInvalidBodyId.
    ///   - `sceneNodeId` is kInvalidSceneNodeId.
    ///   - `bodyId`     is already registered (to any node).
    ///   - `sceneNodeId` is already registered (to any body).
    [[nodiscard]] bool registerBody(BodyId bodyId, SceneNodeId sceneNodeId);

    /// Remove the registration for `bodyId` and any cached snapshot.
    /// Returns false if `bodyId` is not registered.
    bool unregisterBody(BodyId bodyId) noexcept;

    [[nodiscard]] bool        isRegistered(BodyId bodyId)        const noexcept;
    [[nodiscard]] SceneNodeId sceneNodeFor(BodyId bodyId)        const noexcept;
    [[nodiscard]] BodyId      bodyForSceneNode(SceneNodeId id)   const noexcept;
    [[nodiscard]] std::size_t registrationCount()                const noexcept;

    // ── Sync ─────────────────────────────────────────────────────────────────

    /// Read position and velocity for every registered body from `solver` and
    /// cache them in the internal snapshot table.
    ///
    /// Bodies registered but absent from the solver (e.g., removed externally)
    /// are skipped silently.  Returns the number of bodies successfully sampled.
    ///
    /// The snapshot table is rebuilt from scratch on every call (no accumulation).
    std::size_t syncFromSolver(const RigidBodySolver& solver);

    /// Snapshot solver state and write a SimTransform payload to each
    /// registered scene node via NodeScene::setAsset().
    ///
    /// Equivalent to `syncFromSolver(solver)` followed by iterating the
    /// snapshot table and calling scene.setAsset(sceneNodeId, payload) for
    /// each body. Returns the number of scene nodes updated.
    ///
    /// Scene nodes that do not exist in `scene` (e.g., removed externally)
    /// are skipped silently.
    std::size_t syncToScene(const RigidBodySolver& solver, nexus::NodeScene& scene);

    // ── Snapshot queries ──────────────────────────────────────────────────────

    /// Return the last-synced state for a body.
    /// Returns std::nullopt if `bodyId` is not registered or has not been synced.
    [[nodiscard]] std::optional<SimCoupledState> lastState(BodyId bodyId) const noexcept;

    /// Convenience: return the last-synced position only.
    [[nodiscard]] std::optional<SimVec3> lastPosition(BodyId bodyId) const noexcept;

    /// Convenience: return the last-synced velocity only.
    [[nodiscard]] std::optional<SimVec3> lastVelocity(BodyId bodyId) const noexcept;

    /// Number of bodies with a valid snapshot from the last syncFromSolver call.
    [[nodiscard]] std::size_t syncedCount() const noexcept;

    // ── Lifetime ──────────────────────────────────────────────────────────────

    /// Clear all registrations and cached snapshots.
    void clear() noexcept;

private:
    std::unordered_map<BodyId, SceneNodeId>  m_bodyToNode;
    std::unordered_map<SceneNodeId, BodyId>  m_nodeToBody;
    std::unordered_map<BodyId, SimCoupledState> m_snapshots;
};

} // namespace nexus::sim
