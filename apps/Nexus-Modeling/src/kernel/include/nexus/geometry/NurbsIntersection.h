#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct IntersectionPoint {
    float u = 0;
    float v = 0;
    double t = 0;
    Vec3 point;
};

struct NurbsCurveCurveIntersect {
    double ta = 0;
    double tb = 0;
    Vec3 point;
};

class NurbsIntersection {
public:
    [[nodiscard]] static std::vector<IntersectionPoint> intersectSurfaceCurve(
        const NurbsSurface& surface,
        const NurbsCurve& curve,
        float tolerance = 1e-4f);

    [[nodiscard]] static std::vector<NurbsCurveCurveIntersect> intersectCurves(
        const NurbsCurve& a,
        const NurbsCurve& b,
        float tolerance = 1e-4f);
};

} // namespace nexus::geometry
