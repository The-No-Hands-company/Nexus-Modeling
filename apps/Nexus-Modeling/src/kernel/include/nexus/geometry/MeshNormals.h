#pragma once

#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>

namespace nexus::geometry {

enum class NormalMode : uint8_t {
    Smooth,
    Flat,
    CreaseAngle,
};

struct NormalOptions {
    NormalMode mode = NormalMode::Smooth;
    float creaseAngleDeg = 60.f;
};

class MeshNormals {
public:
    [[nodiscard]] static bool compute(Mesh& mesh, const NormalOptions& opts = {});

    [[nodiscard]] static bool computeSmooth(Mesh& mesh);
};

} // namespace nexus::geometry
