#pragma once
// ─── Nexus Geometry ── NurbsSurfaceOffset ───────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsSurfaceOffsetOptions {
    float   distance     = 1.f;
    int32_t samplesU     = 20;
    int32_t samplesV     = 20;
    int32_t fitDegree    = 3;
};

class NurbsSurfaceOffset {
public:
    static NurbsSurface offset(const NurbsSurface& surface,
                                const NurbsSurfaceOffsetOptions& opts = {});
};

} // namespace nexus::geometry
