#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Surface Blending (G1 tangent-continuous)
//
//  Creates a blend surface between two NURBS surfaces along their
//  intersection curve or a given rail curve.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

namespace nexus::geometry {

struct BlendOptions {
    float blendRadius     = 1.f;    // radius of the blend
    int32_t blendSegments = 16;     // samples along the blend direction
    int32_t railSegments  = 64;     // samples along the rail curve
};

// Create a G1 tangent-continuous blend surface between two surfaces
// along a given rail curve (typically the intersection or trim boundary).
// The blend connects the two surfaces with circular-arc cross-sections.
[[nodiscard]] NurbsSurface createBlendSurface(
    const NurbsSurface& surfA,
    const NurbsSurface& surfB,
    const NurbsCurve&   railCurve,
    const BlendOptions& opts = {});

// Create a constant-radius rolling-ball fillet surface between two surfaces.
// The fillet curve is computed as the intersection of the two offset surfaces.
[[nodiscard]] NurbsSurface createFilletSurface(
    const NurbsSurface& surfA,
    const NurbsSurface& surfB,
    float radius,
    const BlendOptions& opts = {});

// Check G1 (tangent) continuity between two surfaces at a given parameter.
// Returns the angle in degrees between the two surface normals.
[[nodiscard]] float surfaceContinuityAngle(
    const NurbsSurface& surfA, float uA, float vA,
    const NurbsSurface& surfB, float uB, float vB);

// Check G2 (curvature) continuity between two surfaces at a given parameter.
// Returns the maximum relative curvature difference.
[[nodiscard]] float surfaceCurvatureError(
    const NurbsSurface& surfA, float uA, float vA,
    const NurbsSurface& surfB, float uB, float vB);

} // namespace nexus::geometry
