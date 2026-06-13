#pragma once
// ─── Nexus Geometry ── CurveFairing ────────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct CurveFairingOptions {
    float   strength     = 0.5f;
    int32_t iterations   = 1;
    bool    fixEndpoints = true;
};

class CurveFairing {
public:
    static NurbsCurve fair(const NurbsCurve& curve,
                            const CurveFairingOptions& opts = {});
};

} // namespace nexus::geometry
