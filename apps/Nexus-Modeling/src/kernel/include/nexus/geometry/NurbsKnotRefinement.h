#pragma once
// ─── Nexus Geometry ── NurbsKnotRefinement ─────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

class NurbsKnotRefinement {
public:
    static NurbsCurve insertKnot(const NurbsCurve& curve, float u);

    static NurbsCurve refineCurve(const NurbsCurve& curve,
                                   const std::vector<float>& knots);

    static NurbsSurface insertKnotU(const NurbsSurface& surface, float u);

    static NurbsSurface insertKnotV(const NurbsSurface& surface, float v);
};

} // namespace nexus::geometry
