#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — deterministic solver v0
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/parametric/ConstraintGraph.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::parametric {

struct ParametricSolverConfig {
    uint32_t maxIterations = 16;
    double convergenceEpsilon = 1e-8;
    // When true (default), use topological propagation order (single pass for DAGs).
    // When false, fall back to the original iterative relaxation order.
    bool useTopologicalOrder = true;
};

struct ParametricSolverReport {
    bool converged = true;
    uint32_t iterationsRan = 0;
    double maxConstraintError = 0.0;
    // errors is sorted lexicographically on every return path; callers may rely on this.
    std::vector<std::string> errors;
    // DOF analysis fields populated by solve() before relaxation.
    int remainingDOF = 0;
    ConstraintStatus constraintStatus = ConstraintStatus::Unconstrained;
    // Propagation order used (constraint IDs in processing order).
    std::vector<ParametricConstraintId> propagationOrder;
    // Constraint IDs that are part of detected dependency cycles.
    std::vector<ParametricConstraintId> cycleConstraintIds;
};

class ParametricSolver {
public:
    [[nodiscard]] static ParametricSolverReport solve(ConstraintGraph& graph,
                                                        const ParametricSolverConfig& config = {}) noexcept;
};

} // namespace nexus::parametric
