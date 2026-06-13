#include <nexus/geometry/MeshMorphTarget.h>

#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh MeshMorphTarget::blend(const Mesh& base,
                              const std::vector<MorphTarget>& targets,
                              const std::vector<float>& weights,
                              const BlendOptions& opts) {
    Mesh result = base;
    if (!result.isValid()) return result;

    const auto& pos = base.attributes().positions();
    size_t V = pos.size();
    if (V == 0 || targets.empty() || weights.empty()) return result;

    if (targets.size() != weights.size()) return result;

    for (size_t t = 0; t < targets.size(); ++t) {
        const auto& target = targets[t];
        if (target.deltas.size() != V) return result;
    }

    std::vector<Vec3> blended = pos;
    for (size_t v = 0; v < V; ++v) {
        Vec3 delta = {0, 0, 0};
        for (size_t t = 0; t < targets.size(); ++t) {
            float w = weights[t];
            if (std::fabs(w) < 1e-10f) continue;
            const auto& target = targets[t];
            delta = delta + target.deltas[v] * w;
        }
        blended[v] = blended[v] + delta;
    }

    result.attributes().setPositions(std::move(blended));

    if (opts.recomputeNormals) {
        if (!result.computeVertexNormals()) return base;
    }

    return result;
}

} // namespace nexus::geometry
