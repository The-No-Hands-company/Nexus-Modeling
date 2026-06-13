#pragma once
// ─── Nexus Geometry ── NurbsCurveSplit ──────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsCurveSplitResult {
    NurbsCurve left;
    NurbsCurve right;
    bool       valid = false;
};

class NurbsCurveSplit {
public:
    static NurbsCurveSplitResult split(const NurbsCurve& curve, float param);
};

} // namespace nexus::geometry
