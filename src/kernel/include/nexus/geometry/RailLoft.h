#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — rail loft (2-rail sweep)
//
//  A rail loft sweeps a cross-section profile along two guide rails, scaling
//  and orienting the section so that its endpoints lie on the two rails at
//  each step parameter.
//
//  Algorithm:
//    1. Sample both rails at the same parameter fractions t ∈ [0, 1].
//    2. At each t the span vector (rail1(t) → rail0(t)) defines the local
//       "width" axis W.  The local "up" axis U is derived from the mean
//       tangent of both rails cross W, then orthonormalised.
//    3. The profile (a polyline in [0,1] normalized width × height) is placed
//       so that profile[0] maps to rail0(t) and profile[n-1] maps to rail1(t).
//       Intermediate points are bilinearly interpolated.
//    4. Optional start/end caps are added as fan triangles.
//
//  Profiles are specified as 2-D points where:
//    - x ∈ [0, 1] is the normalized span fraction (0 = rail0, 1 = rail1)
//    - y is the height offset perpendicular to the span plane.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct RailLoftOptions {
    uint32_t railSteps      = 32;   // number of parameter samples along the rails
    bool     closedProfile  = false;// profile is a closed loop
    bool     capStart       = true; // fan-cap the start cross-section
    bool     capEnd         = true; // fan-cap the end cross-section
};

struct RailLoftResult {
    Mesh        mesh;
    bool        ok    = false;
    std::string error;
};

class RailLofter {
public:
    // 2-rail sweep: sweep `profile2d` along the two guide rails.
    // profile2d: points in normalized (x=span, y=height) space; x should
    // range from 0 (at rail0) to 1 (at rail1).
    // Both rails must be valid NurbsCurve instances.
    [[nodiscard]] static RailLoftResult loft(
        const NurbsCurve& rail0,
        const NurbsCurve& rail1,
        const std::vector<nexus::render::Vec3>& profile2d,
        const RailLoftOptions& opts = {});

    // Convenience: sweep a flat cross-section (straight line between rails)
    // with `profileSteps` evenly-spaced points.
    [[nodiscard]] static RailLoftResult loftFlat(
        const NurbsCurve& rail0,
        const NurbsCurve& rail1,
        uint32_t profileSteps = 8,
        const RailLoftOptions& opts = {});
};

} // namespace nexus::geometry
