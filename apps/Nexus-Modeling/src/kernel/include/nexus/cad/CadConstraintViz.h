#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <vector>

namespace nexus::cad {
using Vec3 = nexus::render::Vec3;

struct ConstraintIcon {
    enum Type { Coincident=0, Horizontal, Vertical, Parallel, Perpendicular,
                Distance, Angle, Midpoint, Tangent, Concentric, Symmetric };
    Type type = Coincident;
    Vec3 position{}, direction{0,1,0};
    double value = 0.0;
    std::vector<parametric::ParametricEntityId> entityIds;
};

class CadConstraintViz {
public:
    [[nodiscard]] static std::vector<ConstraintIcon> generate(
        const parametric::ConstraintGraph& graph) noexcept;
    [[nodiscard]] static geometry::Mesh generateIconMesh(
        const ConstraintIcon& icon) noexcept;
};
}
