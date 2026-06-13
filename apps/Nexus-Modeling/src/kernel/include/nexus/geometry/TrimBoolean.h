#pragma once
// ── Nexus Geometry — TrimBoolean

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct BooleanLoop {
    std::vector<Vec3> points;
    bool closed = true;

    [[nodiscard]] bool empty() const noexcept { return points.empty(); }
};

struct BooleanRegion {
    std::vector<BooleanLoop> outerLoops;
    std::vector<BooleanLoop> innerLoops;

    [[nodiscard]] bool empty() const noexcept {
        return outerLoops.empty();
    }
};

enum class BooleanOp {
    Union,
    Intersection,
    Difference,
};

struct TrimBooleanOptions {
    int32_t gridRes = 256;
};

class TrimBoolean {
public:
    [[nodiscard]] static BooleanRegion compute(
        const BooleanRegion& a,
        const BooleanRegion& b,
        BooleanOp op,
        const TrimBooleanOptions& opts = {});
};

} // namespace nexus::geometry
