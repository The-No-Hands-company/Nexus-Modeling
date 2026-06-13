#pragma once
// ── Nexus Geometry — GordonSurface

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct GordonSurfaceOptions {
    int32_t samplesU = 32;
    int32_t samplesV = 32;
};

class GordonSurface {
public:
    struct CurveFamily {
        std::vector<NurbsCurve> curves;
    };

    [[nodiscard]] static Mesh build(const CurveFamily& uCurves,
                                    const CurveFamily& vCurves,
                                    const GordonSurfaceOptions& opts = {});
};

} // namespace nexus::geometry
