#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace nexus {

using ClothNodeId = uint32_t;
inline constexpr ClothNodeId kInvalidClothNodeId = 0u;

/// Minimal 3-component vector for cloth simulation.
/// Kept independent from render math types.
struct ClothVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] ClothVec3 operator+(const ClothVec3& o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    [[nodiscard]] ClothVec3 operator-(const ClothVec3& o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
    [[nodiscard]] ClothVec3 operator*(float s)            const noexcept { return {x*s,   y*s,   z*s};   }
    ClothVec3& operator+=(const ClothVec3& o) noexcept { x+=o.x; y+=o.y; z+=o.z; return *this; }
    [[nodiscard]] bool operator==(const ClothVec3& o) const noexcept { return x==o.x && y==o.y && z==o.z; }
};

/// Initial description for a cloth particle node.
struct ClothNodeDesc {
    float      mass     = 1.0f;               ///< kg. Zero means pinned (kinematic).
    ClothVec3  position = {0.0f, 0.0f, 0.0f};
    ClothVec3  velocity = {0.0f, 0.0f, 0.0f};
};

/// Per-node state snapshot (position and velocity).
struct ClothNodeSnapshot {
    ClothNodeId id;
    ClothVec3   position;
    ClothVec3   velocity;
};

/// Full cloth state snapshot for replay and rollback.
struct ClothState {
    std::vector<ClothNodeSnapshot> nodes;
    double simulationTime = 0.0;
};

[[nodiscard]] bool operator==(const ClothNodeSnapshot& a, const ClothNodeSnapshot& b) noexcept;
[[nodiscard]] bool operator==(const ClothState& a, const ClothState& b) noexcept;

/// Deterministic cacheable byte format for cloth state snapshots.
[[nodiscard]] std::vector<std::uint8_t> serializeClothState(const ClothState& state);

/// Restores a ClothState from serializeClothState(). Returns false on malformed input.
[[nodiscard]] bool deserializeClothState(const std::vector<std::uint8_t>& bytes, ClothState& outState);

/// Step report returned by ClothSolver::step().
struct ClothStepReport {
    bool        ok               = true;
    double      simulationTime   = 0.0;
    std::size_t nodesIntegrated  = 0;  ///< Dynamic (non-pinned) nodes advanced.
};

/// Cloth solver baseline: mass-spring model with explicit Euler integration.
///
/// Nodes represent cloth particles connected by constraint edges.
/// Pinned nodes (mass == 0) are not integrated.
/// Gravity is applied to dynamic nodes each step.
///
/// Determinism contract: identical initial state + identical step sequence →
/// identical trajectory.
///
/// Thread-safety: none.
class ClothSolver {
public:
    ClothSolver();
    ~ClothSolver();

    // ── Node management ──────────────────────────────────────────────────────

    /// Add a cloth node. Returns a stable ClothNodeId never reused within a session.
    [[nodiscard]] ClothNodeId addNode(const ClothNodeDesc& desc);

    /// Remove a node and any constraint edges touching it. Returns false if unknown.
    [[nodiscard]] bool removeNode(ClothNodeId id) noexcept;

    [[nodiscard]] bool        hasNode(ClothNodeId id)   const noexcept;
    [[nodiscard]] std::size_t nodeCount()               const noexcept;

    /// Returns false if id unknown or node is pinned.
    [[nodiscard]] bool getNodeState(ClothNodeId id,
                                    ClothVec3& outPosition,
                                    ClothVec3& outVelocity) const noexcept;

    // ── Constraint edges ─────────────────────────────────────────────────────

    /// Add a spring edge between two existing nodes.
    /// restLength: natural length (m), must be >= 0.
    /// stiffness: spring constant (N/m), must be > 0.
    /// Returns false if either node is unknown, the edge is self-referential,
    /// or an equivalent edge already exists (a,b) == (b,a).
    [[nodiscard]] bool addEdge(ClothNodeId a, ClothNodeId b,
                                float restLength, float stiffness) noexcept;

    // ── Gravity ──────────────────────────────────────────────────────────────

    void setGravity(ClothVec3 gravity) noexcept;
    [[nodiscard]] ClothVec3 gravity() const noexcept;

    // ── Simulation step ──────────────────────────────────────────────────────

    /// Advance the solver by dt seconds (must be > 0).
    [[nodiscard]] ClothStepReport step(double dt);

    // ── Snapshot / rollback ──────────────────────────────────────────────────

    /// Capture current state. All node positions/velocities and simulationTime.
    [[nodiscard]] ClothState captureState() const;

    /// Restore previously captured state. Returns false if node sets are incompatible.
    [[nodiscard]] bool restoreState(const ClothState& state);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nexus
