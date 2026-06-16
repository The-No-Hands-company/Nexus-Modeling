#include <nexus/parametric/FeatureHistory.h>

#include <nexus/parametric/ParametricSolver.h>

namespace nexus::parametric {

// ── add ─────────────────────────────────────────────────────────────────────

FeatureId FeatureHistory::addSketch(SketchProfile sketch)
{
    const FeatureId id = m_nextId++;
    FeatureNode node;
    node.id     = id;
    node.kind   = FeatureKind::Sketch;
    node.sketch = std::move(sketch);
    node.dirty  = true;
    m_nodes.push_back(std::move(node));
    return id;
}

FeatureId FeatureHistory::addExtrude(SketchProfile sketch,
                                      geometry::CurveExtrudeDesc desc)
{
    const FeatureId id = m_nextId++;
    FeatureNode node;
    node.id         = id;
    node.kind       = FeatureKind::Extrude;
    node.sketch     = std::move(sketch);
    node.extrudeDesc = std::move(desc);
    node.dirty      = true;
    m_nodes.push_back(std::move(node));
    return id;
}

FeatureId FeatureHistory::addRevolve(SketchProfile sketch,
                                      geometry::RevolveDesc desc)
{
    const FeatureId id = m_nextId++;
    FeatureNode node;
    node.id         = id;
    node.kind       = FeatureKind::Revolve;
    node.sketch     = std::move(sketch);
    node.revolveDesc = std::move(desc);
    node.dirty      = true;
    m_nodes.push_back(std::move(node));
    return id;
}

// ── Parameter setters ──────────────────────────────────────────────────────

bool FeatureHistory::setDirection(FeatureId id, nexus::render::Vec3 direction)
{
    FeatureNode* n = findNode(id);
    if (!n || !n->extrudeDesc) return false;
    n->extrudeDesc->direction = direction;
    n->dirty = true;
    return true;
}

bool FeatureHistory::setHeight(FeatureId id, float h)
{
    FeatureNode* n = findNode(id);
    if (!n) return false;
    if (n->extrudeDesc) {
        n->extrudeDesc->height = h;
    } else if (n->revolveDesc) {
        // height for revolve is implicit in the profile, not a descriptor field.
        return false;
    } else {
        return false;
    }
    n->dirty = true;
    return true;
}

bool FeatureHistory::setDraftAngle(FeatureId id, float deg)
{
    FeatureNode* n = findNode(id);
    if (!n || !n->extrudeDesc) return false;
    n->extrudeDesc->draftAngleDeg = deg;
    n->dirty = true;
    return true;
}

bool FeatureHistory::setHeightSamples(FeatureId id, uint32_t n)
{
    FeatureNode* node = findNode(id);
    if (!node || !node->extrudeDesc) return false;
    node->extrudeDesc->heightSamples = n;
    node->dirty = true;
    return true;
}

bool FeatureHistory::setCapEnds(FeatureId id, bool enabled)
{
    FeatureNode* n = findNode(id);
    if (!n) return false;
    if (n->extrudeDesc) {
        n->extrudeDesc->capEnds = enabled;
    } else if (n->revolveDesc) {
        n->revolveDesc->capEnds = enabled;
    } else {
        return false;
    }
    n->dirty = true;
    return true;
}

bool FeatureHistory::setAxis(FeatureId id, nexus::render::Vec3 origin,
                              nexus::render::Vec3 direction)
{
    FeatureNode* n = findNode(id);
    if (!n || !n->revolveDesc) return false;
    n->revolveDesc->axisOrigin    = origin;
    n->revolveDesc->axisDirection = direction;
    n->dirty = true;
    return true;
}

bool FeatureHistory::setStartAngle(FeatureId id, float deg)
{
    FeatureNode* n = findNode(id);
    if (!n || !n->revolveDesc) return false;
    n->revolveDesc->startAngleDeg = deg;
    n->dirty = true;
    return true;
}

bool FeatureHistory::setEndAngle(FeatureId id, float deg)
{
    FeatureNode* n = findNode(id);
    if (!n || !n->revolveDesc) return false;
    n->revolveDesc->endAngleDeg = deg;
    n->dirty = true;
    return true;
}

bool FeatureHistory::setAngularSamples(FeatureId id, uint32_t n)
{
    FeatureNode* node = findNode(id);
    if (!node || !node->revolveDesc) return false;
    node->revolveDesc->angularSamples = n;
    node->dirty = true;
    return true;
}

bool FeatureHistory::setName(FeatureId id, const std::string& name)
{
    FeatureNode* n = findNode(id);
    if (!n) return false;
    n->name = name;
    return true;
}

bool FeatureHistory::removeFeature(FeatureId id)
{
    auto* node = findNode(id);
    if (!node) return false;
    node->deleted = true;
    node->mesh.reset();
    node->surf.reset();
    node->dirty = false;
    return true;
}

bool FeatureHistory::setHidden(FeatureId id, bool hidden)
{
    auto* node = findNode(id);
    if (!node) return false;
    node->hidden = hidden;
    return true;
}

bool FeatureHistory::unhideAll()
{
    bool changed = false;
    for (auto& n : m_nodes) {
        if (n.hidden) { n.hidden = false; changed = true; }
    }
    return changed;
}

// ── Rebuild ─────────────────────────────────────────────────────────────────

namespace {

geometry::NurbsCurve buildProfile(const SketchProfile& sketch) noexcept
{
    std::vector<geometry::Vec3> cps;
    for (auto id : sketch.points) {
        const ParametricPoint3* p = sketch.graph.point(id);
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

FeatureHistoryReport FeatureHistory::rebuild()
{
    FeatureHistoryReport report;

    for (auto& node : m_nodes) {
        if (!node.dirty) continue;

        // Solve sketch constraints if any exist.
        if (node.sketch.graph.totalConstraintCount() > 0) {
            ParametricSolverReport solveReport = ParametricSolver::solve(node.sketch.graph);
            if (!solveReport.converged || !solveReport.errors.empty()) {
                report.converged = false;
                report.errors.insert(report.errors.end(),
                                     solveReport.errors.begin(),
                                     solveReport.errors.end());
                continue;
            }
        }

        // Sketch nodes: just mark clean, no geometry to generate.
        if (node.kind == FeatureKind::Sketch) {
            node.dirty = false;
            continue;
        }

        geometry::NurbsCurve profile = buildProfile(node.sketch);
        if (profile.controlPointCount() < 2) {
            report.converged = false;
            report.errors.push_back("feature " + std::to_string(node.id) +
                                    ": profile has fewer than 2 points");
            continue;
        }

        if (node.kind == FeatureKind::Extrude) {
            geometry::NurbsSurface surf;
            geometry::Mesh mesh;
            auto exRep = geometry::CurveExtrudeOperation::extrude(
                profile, *node.extrudeDesc, surf, &mesh);
            if (exRep.diagnostic != geometry::CurveExtrudeDiagnostic::Success) {
                report.converged = false;
                report.errors.push_back("feature " + std::to_string(node.id) +
                                        ": extrude failed");
                continue;
            }
            node.mesh.emplace(std::move(mesh));
            if (node.extrudeDesc->outputAsNurbsSurface)
                node.surf.emplace(std::move(surf));
        } else if (node.kind == FeatureKind::Revolve) {
            geometry::NurbsSurface surf;
            geometry::Mesh mesh;
            auto revRep = geometry::RevolveOperation::revolve(
                profile, *node.revolveDesc, surf, &mesh);
            if (revRep.diagnostic != geometry::RevolveDiagnostic::Success) {
                report.converged = false;
                report.errors.push_back("feature " + std::to_string(node.id) +
                                        ": revolve failed");
                continue;
            }
            node.mesh.emplace(std::move(mesh));
            if (node.revolveDesc->outputAsNurbsSurface)
                node.surf.emplace(std::move(surf));
        }

        node.dirty = false;
    }

    return report;
}

// ── Convenience accessors ──────────────────────────────────────────────────

const geometry::Mesh* FeatureHistory::mesh(FeatureId id) const noexcept
{
    const FeatureNode* n = findNode(id);
    if (!n || n->dirty || !n->mesh) return nullptr;
    return &*n->mesh;
}

const geometry::NurbsSurface* FeatureHistory::surface(FeatureId id) const noexcept
{
    const FeatureNode* n = findNode(id);
    if (!n || n->dirty || !n->surf) return nullptr;
    return &*n->surf;
}

// ── Query ───────────────────────────────────────────────────────────────────

bool FeatureHistory::isDirty(FeatureId id) const noexcept
{
    const FeatureNode* n = findNode(id);
    return n ? n->dirty : false;
}

FeatureId FeatureHistory::parent(FeatureId id) const noexcept
{
    const FeatureNode* n = findNode(id);
    return n ? n->parent : kInvalidFeatureId;
}

FeatureKind FeatureHistory::kind(FeatureId id) const noexcept
{
    const FeatureNode* n = findNode(id);
    return n ? n->kind : FeatureKind::Extrude;
}

FeatureNode* FeatureHistory::node(FeatureId id) noexcept
{
    return findNode(id);
}

const FeatureNode* FeatureHistory::node(FeatureId id) const noexcept
{
    return findNode(id);
}

// ── Internals ───────────────────────────────────────────────────────────────

FeatureNode* FeatureHistory::findNode(FeatureId id) noexcept
{
    for (auto& n : m_nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const FeatureNode* FeatureHistory::findNode(FeatureId id) const noexcept
{
    for (const auto& n : m_nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

} // namespace nexus::parametric
