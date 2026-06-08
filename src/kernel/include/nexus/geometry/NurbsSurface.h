#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — NURBS / B-spline surfaces (tensor-product)
//
//  A bi-degree (p, q) tensor-product NURBS surface:
//
//    S(u,v) = Σᵢ Σⱼ Nᵢₚ(u) · Nⱼq(v) · wᵢⱼ · Pᵢⱼ
//             ─────────────────────────────────────
//             Σᵢ Σⱼ Nᵢₚ(u) · Nⱼq(v) · wᵢⱼ
//
//  Control points are stored in row-major order: P[i * nV + j] for
//  row i (u-direction) and column j (v-direction).
//
//  Supports:
//    - Arbitrary degree in each parametric direction (independent)
//    - Clamped (interpolates corner control points) knot vectors
//    - First partial derivatives Su(u,v) and Sv(u,v)
//    - Surface normal N = Su × Sv
//    - Uniform (nU × nV grid) and adaptive chord-error tessellation to Mesh
//    - Iso-parameter curve extraction: S(u, v=const) or S(u=const, v)
//    - Knot insertion in u or v direction (Boehm's algorithm)
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace nexus::geometry {

struct NurbsSurfaceData {
    uint32_t degreeU = 3;
    uint32_t degreeV = 3;
    uint32_t nU      = 0; // control-point count in u-direction
    uint32_t nV      = 0; // control-point count in v-direction
    // Row-major: controlPoints[i * nV + j], i ∈ [0,nU), j ∈ [0,nV)
    std::vector<nexus::render::Vec3> controlPoints;
    std::vector<double> weights;   // size nU*nV; default 1.0
    std::vector<double> knotsU;    // size nU + degreeU + 1
    std::vector<double> knotsV;    // size nV + degreeV + 1
};

class NurbsSurface {
public:
    // Build a NURBS surface.  Returns std::nullopt if data is inconsistent:
    //   controlPoints.size() != nU * nV, or knot vector sizes are wrong.
    [[nodiscard]] static std::optional<NurbsSurface> create(NurbsSurfaceData data);

    // Build a clamped uniform B-spline surface from a nU × nV grid of control
    // points (row-major).  All weights = 1.
    [[nodiscard]] static std::optional<NurbsSurface>
    fromGrid(const std::vector<nexus::render::Vec3>& pts,
             uint32_t nU, uint32_t nV,
             uint32_t degreeU = 3, uint32_t degreeV = 3);

    // ── Evaluation ────────────────────────────────────────────────────────────

    // S(u, v) — rational NURBS point.
    [[nodiscard]] nexus::render::Vec3 evaluate(double u, double v) const noexcept;

    // ∂S/∂u and ∂S/∂v via central finite differences.
    [[nodiscard]] nexus::render::Vec3 derivU(double u, double v) const noexcept;
    [[nodiscard]] nexus::render::Vec3 derivV(double u, double v) const noexcept;

    // Unit surface normal N = normalize(Su × Sv).
    [[nodiscard]] nexus::render::Vec3 normal(double u, double v) const noexcept;

    // ── Tessellation ──────────────────────────────────────────────────────────

    // Sample on a regular nU × nV grid; triangulate into a Mesh.
    [[nodiscard]] Mesh tessellate(uint32_t samplesU, uint32_t samplesV) const;

    // Adaptive tessellation: recursively subdivide quads until the chord error
    // in both directions is below tolerance.
    [[nodiscard]] Mesh tessellateAdaptive(float tolerance) const;

    // ── Iso-parameter curves ──────────────────────────────────────────────────

    // Extract an iso-u curve S(u=uConst, v) as a NurbsCurve.
    [[nodiscard]] std::optional<NurbsCurve> isoU(double uConst) const;

    // Extract an iso-v curve S(u, v=vConst) as a NurbsCurve.
    [[nodiscard]] std::optional<NurbsCurve> isoV(double vConst) const;

    // ── Knot insertion ────────────────────────────────────────────────────────

    // Insert knot u into the u-direction knot vector (Boehm's algorithm).
    [[nodiscard]] std::optional<NurbsSurface> insertKnotU(double u) const;

    // Insert knot v into the v-direction knot vector.
    [[nodiscard]] std::optional<NurbsSurface> insertKnotV(double v) const;

    // ── Properties ────────────────────────────────────────────────────────────

    [[nodiscard]] double paramMinU() const noexcept { return m_data.knotsU.front(); }
    [[nodiscard]] double paramMaxU() const noexcept { return m_data.knotsU.back(); }
    [[nodiscard]] double paramMinV() const noexcept { return m_data.knotsV.front(); }
    [[nodiscard]] double paramMaxV() const noexcept { return m_data.knotsV.back(); }
    [[nodiscard]] const NurbsSurfaceData& data() const noexcept { return m_data; }

private:
    explicit NurbsSurface(NurbsSurfaceData d) : m_data(std::move(d)) {}

    double basisU(uint32_t i, uint32_t p, double u) const noexcept;
    double basisV(uint32_t j, uint32_t q, double v) const noexcept;
    uint32_t spanU(double u) const noexcept;
    uint32_t spanV(double v) const noexcept;

    NurbsSurfaceData m_data;
};

} // namespace nexus::geometry
