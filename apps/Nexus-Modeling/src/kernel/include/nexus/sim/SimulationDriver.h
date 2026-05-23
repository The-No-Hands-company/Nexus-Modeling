#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sim — Fixed-timestep simulation driver with render interpolation
//
//  Decouples the deterministic solver step rate from the (variable) render frame
//  rate using the canonical fixed-timestep accumulator pattern
//  (cf. Gaffer-on-Games "Fix Your Timestep"):
//
//    - The solver always advances in fixed dt increments, so trajectories are
//      reproducible regardless of frame pacing.
//    - Leftover time is exposed as an interpolation factor `alpha` in [0,1].
//    - `interpolatedState()` blends the previous and current solver snapshots
//      (lerp position/velocity, nlerp orientation) to produce a smooth,
//      jitter-free transform for the renderer to consume via the coupling.
//    - A substep cap guards against the "spiral of death" when a frame delta is
//      pathologically large.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/sim/SimulationCore.h>

#include <cstdint>

namespace nexus::sim {

/// Blends two solver snapshots: lerp(position, velocity, angularVelocity) and
/// nlerp(orientation, shortest-path). `alpha` is clamped to [0,1]. Bodies are
/// matched by id; a body present in only one snapshot is taken as-is from `to`
/// (or `from` when `to` lacks it). The result is ordered by ascending id.
[[nodiscard]] SimState interpolateSimState(const SimState& from,
                                           const SimState& to,
                                           float alpha);

/// Owns the fixed-timestep accumulator and the previous/current snapshots used
/// for render interpolation. Not thread-safe.
class SimulationDriver {
public:
    /// `fixedTimestep` must be finite and > 0 for advance() to do work.
    /// `maxSubstepsPerAdvance` caps the fixed steps a single advance() may take.
    explicit SimulationDriver(double fixedTimestep,
                              std::uint32_t maxSubstepsPerAdvance = 8) noexcept;

    /// Seed the previous/current interpolation snapshots from the solver's
    /// current state and zero the accumulator. Called implicitly on first
    /// advance(); call explicitly after mutating the solver out of band.
    void reset(const RigidBodySolver& solver);

    /// Accumulate `frameDt` and advance `solver` at the fixed timestep. Returns
    /// the number of fixed steps taken (0 when frameDt is non-finite, <= 0, or
    /// below the fixed timestep). Captures previous/current snapshots so
    /// interpolatedState() stays valid.
    std::uint32_t advance(RigidBodySolver& solver, double frameDt);

    /// Interpolation factor between previousState() and currentState(), [0,1].
    [[nodiscard]] float alpha() const noexcept;

    [[nodiscard]] const SimState& previousState() const noexcept { return m_previous; }
    [[nodiscard]] const SimState& currentState()  const noexcept { return m_current; }

    /// Render-ready snapshot: interpolateSimState(previous, current, alpha()).
    [[nodiscard]] SimState interpolatedState() const;

    [[nodiscard]] double        fixedTimestep() const noexcept { return m_fixedDt; }
    [[nodiscard]] double        accumulator()   const noexcept { return m_accumulator; }
    [[nodiscard]] std::uint32_t maxSubsteps()   const noexcept { return m_maxSubsteps; }
    [[nodiscard]] bool          initialized()   const noexcept { return m_initialized; }

private:
    double        m_fixedDt;
    std::uint32_t m_maxSubsteps;
    double        m_accumulator = 0.0;
    SimState      m_previous;
    SimState      m_current;
    bool          m_initialized = false;
};

} // namespace nexus::sim
