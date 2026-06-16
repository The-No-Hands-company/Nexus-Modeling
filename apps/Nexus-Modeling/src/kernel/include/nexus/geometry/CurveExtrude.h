#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Curve Extrude Operation
//
//  Creates a 3D swept volume by extruding a 2D/3D profile curve along a
//  direction vector.  Supports draft angle (taper).  Produces both a NURBS
//  surface and/or a triangle mesh.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

enum class CurveExtrudeDiagnostic : uint32_t {
    Success              = 0,
    InvalidProfileCurve  = 1,
    InvalidDirection     = 2,
    InvalidHeight        = 4,
};

struct CurveExtrudeDesc {
    // Direction and magnitude of extrusion.
    nexus::render::Vec3 direction = {0.f, 0.f, 1.f};
    float       height           = 1.f;

    float       draftAngleDeg    = 0.f;    // per-side taper angle in degrees (0 = no taper)
    uint32_t    heightSamples    = 16;     // number of samples along the extrusion
    bool        outputAsNurbsSurface = false;
    bool        capEnds          = false;  // add bottom and top caps
};

struct CurveExtrudeReport {
    CurveExtrudeDiagnostic diagnostic = CurveExtrudeDiagnostic::Success;
    bool                   converged  = true;
    uint32_t               vertexCount = 0;
    uint32_t               faceCount   = 0;
    uint32_t               capFaceCount = 0;
};

class CurveExtrudeOperation {
public:
    // Extrude a profile curve along a direction.
    // 'profile' is sampled along its domain.
    // Output is written to 'outSurface' (if requested) and/or 'outMesh'.
    [[nodiscard]] static CurveExtrudeReport extrude(const NurbsCurve&        profile,
                                                     const CurveExtrudeDesc& desc,
                                                     NurbsSurface&            outSurface,
                                                     Mesh*                    outMesh = nullptr) noexcept;
};

} // namespace nexus::geometry
