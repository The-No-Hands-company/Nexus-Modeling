#pragma once

#include <nexus/parametric/ConstraintGraph.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::parametric {

struct ParametricSolverConfig {
    uint32_t maxIterations = 32;
    double convergenceEpsilon = 1e-8;
    bool symmetricRelaxation = true;
};

struct ParametricSolverReport {
    bool converged = true;
    uint32_t iterationsRan = 0;
    double maxConstraintError = 0.0;
    std::vector<std::string> errors;
};

class ParametricSolver {
public:
    [[nodiscard]] static ParametricSolverReport solve(ConstraintGraph& graph,
                                                      const ParametricSolverConfig& config = {}) noexcept;
};

} // namespace nexus::parametric
