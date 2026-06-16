#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Surface Primitive Factories
//
//  Ruled surface, pipe/tube surface, and isoparametric curve extraction.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ──────────── Ruled Surface ───────────────────────────────────────────────

// Build a ruled surface between two curves C0(u) and C1(u).
// S(u,v) = (1-v) * C0(u) + v * C1(u).
// The two curves should have the same degree and compatible knot vectors;
// if they differ, the output surface may be approximate.
[[nodiscard]] NurbsSurface makeRuledSurface(const NurbsCurve& c0,
                                             const NurbsCurve& c1);

// ──────────── Pipe/Tube Surface ──────────────────────────────────────────

// Sweep a circle of given radius along a curve to produce a tube surface.
// 'samples' controls circumferential resolution.
[[nodiscard]] NurbsSurface makeTubeSurface(const NurbsCurve& centerline,
                                            float radius,
                                            uint32_t samples = 16);

// ──────────── Isoparametric Curve ────────────────────────────────────────

// Extract a curve at constant U from a NURBS surface.
[[nodiscard]] NurbsCurve isoCurveU(const NurbsSurface& surf, float u);

// Extract a curve at constant V from a NURBS surface.
[[nodiscard]] NurbsCurve isoCurveV(const NurbsSurface& surf, float v);

// ──────────── Primitive Surfaces ─────────────────────────────────────────

[[nodiscard]] NurbsSurface makePlaneSurface(const Vec3& point,
                                              const Vec3& normal,
                                              float extent = 10.f);

[[nodiscard]] NurbsSurface makeSphereSurface(const Vec3& center,
                                               float radius);

[[nodiscard]] NurbsSurface makeTorusSurface(const Vec3& center,
                                              const Vec3& axis,
                                              float majorRadius,
                                              float minorRadius);

[[nodiscard]] NurbsSurface makeOffsetSurface(const NurbsSurface& surf,
                                               float distance);

[[nodiscard]] NurbsSurface makeCoonsPatch(const NurbsCurve& c0,
                                            const NurbsCurve& c1,
                                            const NurbsCurve& d0,
                                            const NurbsCurve& d1);

} // namespace nexus::geometry
