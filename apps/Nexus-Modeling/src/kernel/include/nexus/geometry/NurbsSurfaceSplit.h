#pragma once
// ─── Nexus Geometry ── NurbsSurfaceSplit ────────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

enum class SplitDirection { U, V };

struct NurbsSurfaceSplitResult {
    NurbsSurface left;
    NurbsSurface right;
    bool         valid = false;
};

class NurbsSurfaceSplit {
public:
    static NurbsSurfaceSplitResult split(const NurbsSurface& surface,
                                          float param,
                                          SplitDirection direction);
};

} // namespace nexus::geometry
