#pragma once
// ── Nexus Geometry — CurveCurveIntersect

#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

class CurveCurveIntersect {
public:
    using Vec3 = nexus::render::Vec3;

    struct Intersection {
        float s = 0.f;
        float t = 0.f;
        Vec3 point;
        float distance = 0.f;
    };

    [[nodiscard]] static std::vector<Intersection> intersect(
        const NurbsCurve& a,
        const NurbsCurve& b,
        int32_t seedSamples = 32,
        float tolerance = 1e-4f);
};

} // namespace nexus::geometry
