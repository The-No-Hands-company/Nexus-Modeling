#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Parametric — Feature wrappers for sketch-to-solid workflow
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/parametric/ConstraintGraph.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/parametric/ParametricSolver.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace nexus::parametric {

// ──────────── ExtrudeFeature ───────────────────────────────────────────────

class ExtrudeFeature {
public:
    ExtrudeFeature() = default;
    explicit ExtrudeFeature(SketchProfile sketch);

    // ── Sketch access ──
    [[nodiscard]] SketchProfile&       sketch()       noexcept { return m_sketch; }
    [[nodiscard]] const SketchProfile& sketch() const noexcept { return m_sketch; }

    // ── Parameter setters ──
    void setDirection(nexus::render::Vec3 direction);
    void setHeight(float h);
    void setDraftAngleDeg(float deg);
    void setHeightSamples(uint32_t n);
    void setCapEnds(bool enabled);
    void setOutputNurbsSurface(bool enabled);

    // ── Descriptor access ──
    [[nodiscard]] const geometry::CurveExtrudeDesc& desc() const noexcept { return m_extrudeDesc; }

    // ── Output access ──
    [[nodiscard]] const geometry::Mesh*        mesh()         const noexcept;
    [[nodiscard]] const geometry::NurbsSurface* nurbsSurface() const noexcept;
    [[nodiscard]] const geometry::NurbsCurve*   profileCurve() const noexcept;

    [[nodiscard]] bool             valid()   const noexcept { return m_valid; }
    [[nodiscard]] geometry::CurveExtrudeDiagnostic diagnostic() const noexcept { return m_diagnostic; }

    // ── Regeneration ──
    // Solve constraints, extract profile, rebuild geometry.  Returns true on success.
    bool rebuild();

private:
    SketchProfile             m_sketch;
    geometry::CurveExtrudeDesc m_extrudeDesc;
    geometry::CurveExtrudeDiagnostic m_diagnostic = geometry::CurveExtrudeDiagnostic::Success;
    geometry::Mesh             m_mesh;
    geometry::NurbsSurface     m_surface;
    geometry::NurbsCurve       m_profileCurve;
    bool                       m_dirty = true;
    bool                       m_valid = false;
};

// ──────────── RevolveFeature ───────────────────────────────────────────────

class RevolveFeature {
public:
    RevolveFeature() = default;
    explicit RevolveFeature(SketchProfile sketch);

    // ── Sketch access ──
    [[nodiscard]] SketchProfile&       sketch()       noexcept { return m_sketch; }
    [[nodiscard]] const SketchProfile& sketch() const noexcept { return m_sketch; }

    // ── Parameter setters ──
    void setAxis(nexus::render::Vec3 origin, nexus::render::Vec3 direction);
    void setStartAngleDeg(float deg);
    void setEndAngleDeg(float deg);
    void setAngularSamples(uint32_t n);
    void setCapEnds(bool enabled);
    void setOutputNurbsSurface(bool enabled);

    // ── Descriptor access ──
    [[nodiscard]] const geometry::RevolveDesc& desc() const noexcept { return m_revolveDesc; }

    // ── Output access ──
    [[nodiscard]] const geometry::Mesh*        mesh()         const noexcept;
    [[nodiscard]] const geometry::NurbsSurface* nurbsSurface() const noexcept;
    [[nodiscard]] const geometry::NurbsCurve*   profileCurve() const noexcept;

    [[nodiscard]] bool             valid()   const noexcept { return m_valid; }
    [[nodiscard]] geometry::RevolveDiagnostic diagnostic() const noexcept { return m_diagnostic; }

    // ── Regeneration ──
    bool rebuild();

private:
    SketchProfile           m_sketch;
    geometry::RevolveDesc   m_revolveDesc;
    geometry::RevolveDiagnostic m_diagnostic = geometry::RevolveDiagnostic::Success;
    geometry::Mesh           m_mesh;
    geometry::NurbsSurface   m_surface;
    geometry::NurbsCurve     m_profileCurve;
    bool                     m_dirty = true;
    bool                     m_valid = false;
};

} // namespace nexus::parametric
