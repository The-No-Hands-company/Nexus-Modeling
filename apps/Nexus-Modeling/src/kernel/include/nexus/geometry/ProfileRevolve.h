#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Profile Revolve Operation
//
//  Creates a surface of revolution by rotating a 2D/3D profile curve around
//  an axis.  Produces both a NURBS surface and/or a triangle mesh.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

enum class RevolveDiagnostic : uint32_t {
    Success              = 0,
    InvalidProfileCurve  = 1,
    DegenerateAxis       = 2,
    InvalidAngles        = 4,
};

struct RevolveDesc {
    // Axis of revolution — the profile is swept around the line through 'axisOrigin'
    // in the direction of 'axisDirection'.
    nexus::render::Vec3 axisOrigin    = {0.f, 0.f, 0.f};
    nexus::render::Vec3 axisDirection = {0.f, 0.f, 1.f};

    float       startAngleDeg  = 0.f;    // start angle in degrees
    float       endAngleDeg    = 360.f;  // end angle in degrees
    uint32_t    angularSamples = 64;     // number of samples around the axis
    bool        outputAsNurbsSurface = false;
    bool        capEnds        = false;  // add top and bottom caps (only for full 360°)
};

struct RevolveReport {
    RevolveDiagnostic diagnostic = RevolveDiagnostic::Success;
    bool              converged  = true;
    uint32_t          vertexCount = 0;
    uint32_t          faceCount   = 0;
    uint32_t          capFaceCount = 0;
};

class RevolveOperation {
public:
    // Revolve a profile curve around an axis.
    // 'profile' is sampled along its domain to produce profile samples.
    // Output is written to 'outSurface' (if requested) and/or 'outMesh'.
    [[nodiscard]] static RevolveReport revolve(const NurbsCurve&  profile,
                                                const RevolveDesc& desc,
                                                NurbsSurface&       outSurface,
                                                Mesh*               outMesh = nullptr) noexcept;
};

} // namespace nexus::geometry
