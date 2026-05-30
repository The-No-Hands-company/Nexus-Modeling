#include <nexus/automation/ParametricExtension.h>

#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSolver.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
getArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static double floatArg(const ScriptCommand& cmd,
                                     std::string_view key, double def)
{
    if (const auto v = getArg(cmd, key)) {
        double out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = getArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

static std::string f2s(double v)
{
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v,
                                   std::chars_format::fixed, 6);
    return std::string(buf, ptr);
}

} // namespace

void registerParametricCommands(ScriptBatchHarness& harness)
{
    harness.registry().registerCommand("param.add_point",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            nexus::parametric::ParametricPoint3 pt;
            pt.x = floatArg(cmd, "x", 0.0);
            pt.y = floatArg(cmd, "y", 0.0);
            pt.z = floatArg(cmd, "z", 0.0);

            const auto id = ctx.parametricGraph.addPoint(pt);
            if (id == nexus::parametric::kInvalidEntityId) {
                messages.push_back("param.add_point error: addPoint returned kInvalidEntityId");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.hasParametricGraph = true;

            std::string msg = "param.add_point ok"
                " id=" + std::to_string(id) +
                " x="  + f2s(pt.x) +
                " y="  + f2s(pt.y) +
                " z="  + f2s(pt.z);

            if (const auto v = getArg(cmd, "name"))
                msg += " name=" + std::string(*v);

            messages.push_back(std::move(msg));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    harness.registry().registerCommand("param.add_constraint",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            nexus::parametric::ParametricEntityId idA = nexus::parametric::kInvalidEntityId;
            nexus::parametric::ParametricEntityId idB = nexus::parametric::kInvalidEntityId;

            if (const auto v = getArg(cmd, "a"))
                std::from_chars(v->data(), v->data() + v->size(), idA);
            if (const auto v = getArg(cmd, "b"))
                std::from_chars(v->data(), v->data() + v->size(), idB);

            if (!ctx.parametricGraph.hasEntity(idA) || !ctx.parametricGraph.hasEntity(idB)) {
                messages.push_back("param.add_constraint error: entity id not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const double value = floatArg(cmd, "value", 1.0);

            std::string typeStr = "distance";
            if (const auto v = getArg(cmd, "type"))
                typeStr = std::string(*v);

            nexus::parametric::ParametricConstraintId cid =
                nexus::parametric::kInvalidConstraintId;

            if (typeStr == "coincident") {
                cid = ctx.parametricGraph.addCoincidentConstraint(idA, idB);
            } else if (typeStr == "axis_distance") {
                nexus::parametric::Axis axis = nexus::parametric::Axis::X;
                if (const auto v = getArg(cmd, "axis")) {
                    if      (*v == "y") axis = nexus::parametric::Axis::Y;
                    else if (*v == "z") axis = nexus::parametric::Axis::Z;
                }
                cid = ctx.parametricGraph.addAxisAlignedDistanceConstraint(idA, idB, axis, value);
            } else {
                cid = ctx.parametricGraph.addDistanceConstraint(idA, idB, value);
            }

            if (cid == nexus::parametric::kInvalidConstraintId) {
                messages.push_back("param.add_constraint error: returned kInvalidConstraintId"
                    " type=" + typeStr);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("param.add_constraint ok"
                " type=" + typeStr +
                " id="   + std::to_string(cid));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    harness.registry().registerCommand("param.solve",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            nexus::parametric::ParametricSolverConfig config;
            config.maxIterations      = uintArg(cmd, "max_iterations", 16u);
            config.convergenceEpsilon = floatArg(cmd, "epsilon", 1e-8);

            const auto report = nexus::parametric::ParametricSolver::solve(
                ctx.parametricGraph, config);

            if (report.converged) {
                messages.push_back("param.solve ok"
                    " iterations=" + std::to_string(report.iterationsRan) +
                    " error="      + f2s(report.maxConstraintError));
            } else {
                messages.push_back("param.solve warn not_converged"
                    " iterations=" + std::to_string(report.iterationsRan) +
                    " error="      + f2s(report.maxConstraintError));
                for (const auto& e : report.errors)
                    messages.push_back(e);
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });

    harness.registry().registerCommand("param.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const auto& graph = ctx.parametricGraph;

            messages.push_back("param.describe"
                " points="               + std::to_string(graph.entityCount()) +
                " distance_constraints=" + std::to_string(graph.distanceConstraintCount()) +
                " coincident_constraints=" + std::to_string(graph.coincidentConstraintCount()) +
                " axis_constraints="     + std::to_string(graph.axisAlignedDistanceConstraintCount()));

            const auto& entities = graph.entities();
            const std::size_t limit = std::min(entities.size(), std::size_t{8});
            for (std::size_t i = 0; i < limit; ++i) {
                const auto& e = entities[i];
                messages.push_back("param.describe point"
                    " id=" + std::to_string(e.id) +
                    " x="  + f2s(e.point.x) +
                    " y="  + f2s(e.point.y) +
                    " z="  + f2s(e.point.z));
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
