#pragma once
// ── Nexus Geometry — QuadRemesh

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct QuadRemeshOptions {
    int32_t resolutionU = 32;
    int32_t resolutionV = 32;
};

class QuadRemesh {
public:
    [[nodiscard]] static Mesh remesh(const Mesh& surface,
                                      const QuadRemeshOptions& opts = {});
};

} // namespace nexus::geometry
