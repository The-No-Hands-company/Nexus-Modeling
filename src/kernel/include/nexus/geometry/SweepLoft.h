#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — parametric sweep and loft operations
//
//  Sweep:
//    Extrudes a 2-D profile curve along a 3-D spine (rail) curve.  At each
//    spine sample, a local Frenet-Serret frame is computed (tangent T,
//    normal N, binormal B) and the profile is placed in the (N, B) plane.
//    The result is a closed or open tube-like mesh.
//
//    Twist angle and non-uniform scaling along the spine are supported.
//
//  Loft:
//    Skins a surface through an ordered list of cross-section profiles.
//    Each profile is a polyline (sampled from a NurbsCurve or supplied
//    directly).  Profiles must all have the same vertex count.
//    The resulting Mesh connects corresponding vertices across sections
//    with quad faces (triangulated into pairs of triangles).
//
//    Rail loft (2-rail sweep) is a planned v0.32 feature.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

// ─── Sweep ───────────────────────────────────────────────────────────────────

struct SweepOptions {
    uint32_t spineSteps     = 32;   // number of spine parameter samples
    uint32_t profileSteps   = 0;    // 0 = use profile sample count as-is
    float    twistRadians   = 0.f;  // total twist from spine start to end
    float    scaleStart     = 1.f;  // profile scale at spine start
    float    scaleEnd       = 1.f;  // profile scale at spine end
    bool     closedSpine    = false;
    bool     closedProfile  = true;
    bool     capStart       = true;  // fan-cap the start opening
    bool     capEnd         = true;  // fan-cap the end opening
};

struct SweepResult {
    Mesh mesh;
    bool ok = false;
    std::string error;
};

class Sweeper {
public:
    // Sweep a 2-D profile (sampled polyline in XY plane) along a 3-D spine.
    // The profile is placed in the plane perpendicular to the spine tangent
    // at each step using a rotation-minimizing frame.
    [[nodiscard]] static SweepResult sweep(
        const NurbsCurve& spine,
        const std::vector<nexus::render::Vec3>& profile2d,
        const SweepOptions& opts = {});

    // Convenience: sweep a circle profile with radius `r` along the spine.
    [[nodiscard]] static SweepResult sweepCircle(
        const NurbsCurve& spine,
        float radius,
        const SweepOptions& opts = {});
};

// ─── Loft ────────────────────────────────────────────────────────────────────

struct LoftOptions {
    bool closedProfiles  = false; // profiles are closed loops
    bool closedLoft      = false; // connect last profile back to first
    bool capStart        = true;
    bool capEnd          = true;
};

struct LoftResult {
    Mesh mesh;
    bool ok = false;
    std::string error;
};

class Lofter {
public:
    // Loft through a sequence of cross-section polylines.
    // All profiles must have the same number of vertices.
    [[nodiscard]] static LoftResult loft(
        const std::vector<std::vector<nexus::render::Vec3>>& profiles,
        const LoftOptions& opts = {});

    // Convenience: loft through NurbsCurve sections, tessellating each to
    // `samplesPerProfile` points first.
    [[nodiscard]] static LoftResult loftCurves(
        const std::vector<NurbsCurve>& profiles,
        uint32_t samplesPerProfile,
        const LoftOptions& opts = {});
};

} // namespace nexus::geometry
