#pragma once
#include <nexus/parametric/ConstraintGraph.h>
#include <cstdint>
#include <vector>

namespace nexus::cad {
struct SolverCache { std::vector<double> lastPositions; std::vector<parametric::ParametricConstraintId> changedConstraints; bool valid=false; };
class CadIncrementalSolver {
public:
    [[nodiscard]] static bool solve(parametric::ConstraintGraph&,const SolverCache&,const std::vector<parametric::ParametricConstraintId>&) noexcept;
    [[nodiscard]] static SolverCache buildCache(const parametric::ConstraintGraph&) noexcept;
    static void invalidate(SolverCache&,parametric::ParametricEntityId) noexcept;
};
}
