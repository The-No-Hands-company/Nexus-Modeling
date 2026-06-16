#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Bezier and Hermite Curve Convenience Types
//
//  Thin wrappers that construct NurbsCurve instances from standard
//  polynomial representations.  Bezier and Hermite curves are special
//  cases of NURBS, so these are factory-only types — evaluation,
//  knot insertion, etc. all go through the NurbsCurve API.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <vector>

namespace nexus::geometry {

// ──────────── BezierCurve ──────────────────────────────────────────────────

class BezierCurve {
public:
    // Build a rational Bezier curve of degree n = controlPoints.size() - 1.
    [[nodiscard]] static NurbsCurve fromControlPoints(
        const std::vector<Vec3>& controlPoints);

    // Build a rational Bezier curve with per-control-point weights.
    [[nodiscard]] static NurbsCurve fromControlPoints(
        const std::vector<Vec3>& controlPoints,
        const std::vector<float>& weights);
};

// ──────────── HermiteCurve ─────────────────────────────────────────────────

class HermiteCurve {
public:
    // Build a cubic Hermite curve from two points and two tangents.
    // P(0) = p0, P(1) = p1, P'(0) = t0, P'(1) = t1.
    [[nodiscard]] static NurbsCurve fromEndpoints(
        const Vec3& p0, const Vec3& p1,
        const Vec3& t0, const Vec3& t1);
};

// ──────────── CatmullRomCurve ──────────────────────────────────────────────

class CatmullRomCurve {
public:
    // Build a Catmull-Rom spline through a set of points.
    // The curve passes through each point; tangents are computed from neighbors.
    // For the first/last segments, tangents are mirror-reflected.
    [[nodiscard]] static NurbsCurve fromPoints(
        const std::vector<Vec3>& points);
};

// ──────────── Conic Sections & Helix ────────────────────────────────────────

class ConicCurve {
public:
    [[nodiscard]] static NurbsCurve circle(const Vec3& center,
                                            const Vec3& normal,
                                            float radius);

    [[nodiscard]] static NurbsCurve arc(const Vec3& center,
                                         const Vec3& normal,
                                         const Vec3& startDir,
                                         float radius,
                                         float angleDeg);

    [[nodiscard]] static NurbsCurve ellipse(const Vec3& center,
                                             const Vec3& normal,
                                             float radiusX, float radiusY);

    [[nodiscard]] static NurbsCurve helix(const Vec3& center,
                                           const Vec3& axis,
                                           float radius,
                                           float pitch,
                                           float height,
                                           uint32_t samplesPerTurn = 16);
};

// ──────────── Primitive Curve Factories ─────────────────────────────────────

class CurveFactory {
public:
    // Polyline from a set of 3D points.
    [[nodiscard]] static NurbsCurve polyline(const std::vector<Vec3>& points);

    // Offset a curve by a distance along its normal direction.
    [[nodiscard]] static NurbsCurve offset(const NurbsCurve& curve,
                                            float distance,
                                            const Vec3& planeNormal = {0, 0, 1});

    // Compose multiple curves into a single composite curve.
    [[nodiscard]] static NurbsCurve composite(
        const std::vector<NurbsCurve>& curves);

    // Project a 3D curve onto a plane, returning a 2D curve in the plane.
    [[nodiscard]] static NurbsCurve projectToPlane(
        const NurbsCurve& curve,
        const Vec3& planePoint,
        const Vec3& planeNormal);

    // Intersection curve of two surfaces (approximate by sampling).
    [[nodiscard]] static NurbsCurve intersection(
        const NurbsSurface& surfA,
        const NurbsSurface& surfB,
        uint32_t samples = 64);
};

} // namespace nexus::geometry
