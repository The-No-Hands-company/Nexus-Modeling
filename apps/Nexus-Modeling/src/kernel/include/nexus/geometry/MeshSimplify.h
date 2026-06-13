#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct SimplifyOptions {
    uint32_t targetFaceCount = 500;
};

class MeshSimplify {
public:
    [[nodiscard]] static Mesh decimate(const Mesh& mesh, const SimplifyOptions& opts = {});
};

} // namespace nexus::geometry
