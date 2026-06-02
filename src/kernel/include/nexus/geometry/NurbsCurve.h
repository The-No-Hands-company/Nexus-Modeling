#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — NURBS / B-spline curves
//
//  Implements the standard Cox-de Boor recursion for B-spline basis functions
//  and rational NURBS evaluation:
//
//    C(u) = Σ Nᵢₚ(u) * wᵢ * Pᵢ  /  Σ Nᵢₚ(u) * wᵢ
//
//  Supports:
//    - Uniform and non-uniform knot vectors
//    - Clamped (open), periodic, and unclamped end conditions
//    - Homogeneous control point weights (NURBS) or unit weights (B-spline)
//    - First and second derivative evaluation
//    - Tessellation to a polyline at a given chord-length tolerance or
//      fixed sample count
//    - Degree elevation (raise degree by 1)
//    - Knot insertion (Boehm's algorithm)
//
//  Control points are 3-D (Vec3); weights are stored separately.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace nexus::geometry {

enum class NurbsKnotType : uint8_t {
    Clamped,   // knot vector clamped at both ends (interpolates endpoints)
    Unclamped, // uniform, no clamping
    Periodic,  // wrapped domain
};

struct NurbsCurveData {
    uint32_t degree = 3;
    std::vector<nexus::render::Vec3> controlPoints;
    std::vector<double> weights;   // size == controlPoints.size(); default 1.0
    std::vector<double> knots;     // size == n + degree + 1
    NurbsKnotType knotType = NurbsKnotType::Clamped;
};

class NurbsCurve {
public:
    // Build a NURBS curve.  Returns std::nullopt if the data is inconsistent:
    //   knots.size() != controlPoints.size() + degree + 1, or
    //   degree == 0, or controlPoints.size() < degree + 1.
    [[nodiscard]] static std::optional<NurbsCurve> create(NurbsCurveData data);

    // Build a uniform clamped B-spline (all weights = 1).
    [[nodiscard]] static std::optional<NurbsCurve>
    fromControlPoints(const std::vector<nexus::render::Vec3>& pts, uint32_t degree = 3);

    // ── Evaluation ────────────────────────────────────────────────────────────

    // Evaluate the curve at parameter u ∈ [u_min, u_max].
    [[nodiscard]] nexus::render::Vec3 evaluate(double u) const noexcept;

    // First derivative C'(u).
    [[nodiscard]] nexus::render::Vec3 derivative1(double u) const noexcept;

    // Second derivative C''(u).
    [[nodiscard]] nexus::render::Vec3 derivative2(double u) const noexcept;

    // ── Tessellation ──────────────────────────────────────────────────────────

    // Sample at `count` uniformly-spaced parameter values.
    [[nodiscard]] std::vector<nexus::render::Vec3> tessellate(uint32_t count) const;

    // Adaptive tessellation: subdivide until chord error < tolerance.
    [[nodiscard]] std::vector<nexus::render::Vec3> tessellateAdaptive(float tolerance) const;

    // ── Refinement ────────────────────────────────────────────────────────────

    // Raise curve degree by 1 (degree elevation).
    [[nodiscard]] NurbsCurve elevate() const;

    // Insert a knot value u into the knot vector (Boehm's algorithm).
    // Multiplicity increases by 1; curve shape is preserved.
    [[nodiscard]] std::optional<NurbsCurve> insertKnot(double u) const;

    // ── Properties ────────────────────────────────────────────────────────────

    [[nodiscard]] uint32_t degree() const noexcept { return m_data.degree; }
    [[nodiscard]] uint32_t controlPointCount() const noexcept
    {
        return static_cast<uint32_t>(m_data.controlPoints.size());
    }
    [[nodiscard]] double paramMin() const noexcept;
    [[nodiscard]] double paramMax() const noexcept;
    [[nodiscard]] const NurbsCurveData& data() const noexcept { return m_data; }

private:
    explicit NurbsCurve(NurbsCurveData d) : m_data(std::move(d)) {}

    // Cox-de Boor basis function evaluation.
    double basis(uint32_t i, uint32_t p, double u) const noexcept;

    // Find knot span index for parameter u.
    uint32_t knotSpan(double u) const noexcept;

    NurbsCurveData m_data;
};

} // namespace nexus::geometry
