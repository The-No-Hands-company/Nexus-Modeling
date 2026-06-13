#pragma once
// ─── Nexus Geometry ── NurbsSurfaceArea ────────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsSurfaceAreaOptions {
    int32_t quadraturePoints = 5;
};

struct NurbsSurfaceAreaResult {
    float totalArea = 0.f;
    std::vector<float> subPatchAreas;
};

class NurbsSurfaceArea {
public:
    static NurbsSurfaceAreaResult compute(const NurbsSurface& surface,
                                          const NurbsSurfaceAreaOptions& opts = {});

    static NurbsSurfaceAreaResult subPatch(const NurbsSurface& surface,
                                            float u0, float u1,
                                            float v0, float v1,
                                            const NurbsSurfaceAreaOptions& opts = {});
};

} // namespace nexus::geometry
