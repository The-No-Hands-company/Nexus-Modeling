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
};

class ParametricSolver {
public:
    [[nodiscard]] static ParametricSolverReport solve(ConstraintGraph& graph,
                                                        const ParametricSolverConfig& config = {}) noexcept;
};

} // namespace nexus::parametric
