#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct SurfaceOffsetOptions {
    float distance = 0.1f;
    bool recomputeNormals = true;
};

class SurfaceOffset {
public:
    [[nodiscard]] static Mesh offset(const Mesh& mesh, const SurfaceOffsetOptions& opts = {});
};

} // namespace nexus::geometry
