#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct MorphTarget {
    std::vector<nexus::render::Vec3> deltas;
};

struct BlendOptions {
    bool recomputeNormals = true;
};

class MeshMorphTarget {
public:
    [[nodiscard]] static Mesh blend(const Mesh& base,
                                     const std::vector<MorphTarget>& targets,
                                     const std::vector<float>& weights,
                                     const BlendOptions& opts = {});
};

} // namespace nexus::geometry
