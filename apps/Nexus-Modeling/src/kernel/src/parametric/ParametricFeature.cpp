#include <nexus/parametric/ParametricFeature.h>

#include <algorithm>

namespace nexus::parametric {

namespace {

geometry::NurbsCurve buildProfileCurve(const std::vector<ParametricEntityId>& ids,
                                        const ConstraintGraph& graph) noexcept
{
    std::vector<geometry::Vec3> cps;
    for (auto id : ids) {
        const ParametricPoint3* p = graph.point(id);
        if (!p) continue;
        cps.emplace_back(static_cast<float>(p->x), static_cast<float>(p->y),
                          static_cast<float>(p->z));
    }
    if (cps.size() < 2) return {};

    int32_t nCPs = static_cast<int32_t>(cps.size());
    int32_t deg = std::min(3, nCPs - 1);
    deg = std::max(deg, 1);

    std::vector<float> knots(static_cast<size_t>(nCPs + deg + 1));
    for (int32_t j = 0; j <= deg; ++j) knots[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nCPs - deg; ++j)
        knots[static_cast<size_t>(deg + j)] = static_cast<float>(j) / static_cast<float>(nCPs - deg);
    for (int32_t j = 0; j <= deg; ++j) knots[knots.size() - 1 - static_cast<size_t>(j)] = 1.f;

    return geometry::NurbsCurve(deg, std::move(knots), std::move(cps));
}

} // namespace

// ── ExtrudeFeature ──────────────────────────────────────────────────────────

ExtrudeFeature::ExtrudeFeature(SketchProfile sketch)
    : m_sketch(std::move(sketch)), m_dirty(true)
{}

void ExtrudeFeature::setDirection(nexus::render::Vec3 direction)
{
    m_extrudeDesc.direction = direction;
    m_dirty = true;
}

void ExtrudeFeature::setHeight(float h)
{
    m_extrudeDesc.height = h;
    m_dirty = true;
}

void ExtrudeFeature::setDraftAngleDeg(float deg)
{
    m_extrudeDesc.draftAngleDeg = deg;
    m_dirty = true;
}

void ExtrudeFeature::setHeightSamples(uint32_t n)
{
    m_extrudeDesc.heightSamples = n;
    m_dirty = true;
}

void ExtrudeFeature::setCapEnds(bool enabled)
{
    m_extrudeDesc.capEnds = enabled;
    m_dirty = true;
}

void ExtrudeFeature::setOutputNurbsSurface(bool enabled)
{
    m_extrudeDesc.outputAsNurbsSurface = enabled;
    m_dirty = true;
}

const geometry::Mesh* ExtrudeFeature::mesh() const noexcept
{
    if (m_dirty || !m_valid) return nullptr;
    return &m_mesh;
}

const geometry::NurbsSurface* ExtrudeFeature::nurbsSurface() const noexcept
{
    if (m_dirty || !m_valid) return nullptr;
    if (!m_extrudeDesc.outputAsNurbsSurface) return nullptr;
    return &m_surface;
}

const geometry::NurbsCurve* ExtrudeFeature::profileCurve() const noexcept
{
    if (m_dirty || !m_valid) return nullptr;
    return &m_profileCurve;
}

bool ExtrudeFeature::rebuild()
{
    m_valid = false;
    m_mesh = {};
    m_surface = {};
    m_profileCurve = {};

    // Solve constraints if any exist.
    if (m_sketch.graph.totalConstraintCount() > 0) {
        ParametricSolverReport solveReport = ParametricSolver::solve(m_sketch.graph);
        if (!solveReport.converged || !solveReport.errors.empty()) {
            return false;
        }
    }

    // Build profile from sketch points.
    m_profileCurve = buildProfileCurve(m_sketch.points, m_sketch.graph);
    if (m_profileCurve.controlPointCount() < 2) {
        return false;
    }

    // Extrude the profile.
    geometry::CurveExtrudeReport report =
        geometry::CurveExtrudeOperation::extrude(m_profileCurve, m_extrudeDesc,
                                                  m_surface, &m_mesh);
    m_diagnostic = report.diagnostic;
    m_valid = (report.diagnostic == geometry::CurveExtrudeDiagnostic::Success);
    m_dirty = false;
    return m_valid;
}

// ── RevolveFeature ──────────────────────────────────────────────────────────

RevolveFeature::RevolveFeature(SketchProfile sketch)
    : m_sketch(std::move(sketch)), m_dirty(true)
{}

void RevolveFeature::setAxis(nexus::render::Vec3 origin, nexus::render::Vec3 direction)
{
    m_revolveDesc.axisOrigin    = origin;
    m_revolveDesc.axisDirection = direction;
    m_dirty = true;
}

void RevolveFeature::setStartAngleDeg(float deg)
{
    m_revolveDesc.startAngleDeg = deg;
    m_dirty = true;
}

void RevolveFeature::setEndAngleDeg(float deg)
{
    m_revolveDesc.endAngleDeg = deg;
    m_dirty = true;
}

void RevolveFeature::setAngularSamples(uint32_t n)
{
    m_revolveDesc.angularSamples = n;
    m_dirty = true;
}

void RevolveFeature::setCapEnds(bool enabled)
{
    m_revolveDesc.capEnds = enabled;
    m_dirty = true;
}

void RevolveFeature::setOutputNurbsSurface(bool enabled)
{
    m_revolveDesc.outputAsNurbsSurface = enabled;
    m_dirty = true;
}

const geometry::Mesh* RevolveFeature::mesh() const noexcept
{
    if (m_dirty || !m_valid) return nullptr;
    return &m_mesh;
}

const geometry::NurbsSurface* RevolveFeature::nurbsSurface() const noexcept
{
    if (m_dirty || !m_valid) return nullptr;
    if (!m_revolveDesc.outputAsNurbsSurface) return nullptr;
    return &m_surface;
}

const geometry::NurbsCurve* RevolveFeature::profileCurve() const noexcept
{
    if (m_dirty || !m_valid) return nullptr;
    return &m_profileCurve;
}

bool RevolveFeature::rebuild()
{
    m_valid = false;
    m_mesh = {};
    m_surface = {};
    m_profileCurve = {};

    // Solve constraints if any exist.
    if (m_sketch.graph.totalConstraintCount() > 0) {
        ParametricSolverReport solveReport = ParametricSolver::solve(m_sketch.graph);
        if (!solveReport.converged || !solveReport.errors.empty()) {
            return false;
        }
    }

    // Build profile from sketch points.
    m_profileCurve = buildProfileCurve(m_sketch.points, m_sketch.graph);
    if (m_profileCurve.controlPointCount() < 2) {
        return false;
    }

    // Revolve the profile.
    geometry::RevolveReport report =
        geometry::RevolveOperation::revolve(m_profileCurve, m_revolveDesc,
                                             m_surface, &m_mesh);
    m_diagnostic = report.diagnostic;
    m_valid = (report.diagnostic == geometry::RevolveDiagnostic::Success);
    m_dirty = false;
    return m_valid;
}

} // namespace nexus::parametric
