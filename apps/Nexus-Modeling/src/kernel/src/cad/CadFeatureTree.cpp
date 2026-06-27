#include <nexus/cad/CadFeatureTree.h>

#include <algorithm>
#include <cmath>

namespace nexus::cad {

// ── Feature Tree ──────────────────────────────────────────────────────────

std::vector<FeatureTreeNode> CadFeatureTree::build(const CadDocument& doc) noexcept
{
    std::vector<FeatureTreeNode> tree;
    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node) continue;
        FeatureTreeNode tn;
        tn.id   = node->id;
        tn.name = node->name.empty()
            ? "Feature_" + std::to_string(i)
            : node->name;
        tn.kind = node->kind;
        tree.push_back(tn);
    }
    return tree;
}

const FeatureTreeNode* CadFeatureTree::find(
    const std::vector<FeatureTreeNode>& tree,
    parametric::FeatureId id) noexcept
{
    for (const auto& n : tree) {
        if (n.id == id) return &n;
        auto* child = find(n.children, id);
        if (child) return child;
    }
    return nullptr;
}

// ── Workplane ─────────────────────────────────────────────────────────────

Workplane Workplane::fromOriginNormal(
    const Vec3& origin, const Vec3& normal, const Vec3& refDir) noexcept
{
    Workplane wp;
    wp.origin = origin;
    wp.normal = normal.normalize();

    // Build orthonormal basis.
    wp.uAxis = refDir - wp.normal * refDir.dot(wp.normal);
    float uLen = wp.uAxis.length();
    if (uLen > 1e-10f) wp.uAxis = wp.uAxis * (1.f / uLen);
    else wp.uAxis = {1, 0, 0};

    wp.vAxis = wp.normal.cross(wp.uAxis).normalize();
    wp.uAxis = wp.vAxis.cross(wp.normal).normalize();
    return wp;
}

Workplane Workplane::XY() noexcept
{
    return {{0,0,0}, {0,0,1}, {1,0,0}, {0,1,0}, "XY Plane"};
}

Workplane Workplane::XZ() noexcept
{
    return {{0,0,0}, {0,1,0}, {1,0,0}, {0,0,1}, "XZ Plane"};
}

Workplane Workplane::YZ() noexcept
{
    return {{0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}, "YZ Plane"};
}

void CadWorkplaneManager::setActive(const Workplane& wp)
{ m_active = wp; }

void CadWorkplaneManager::addCustom(const Workplane& wp)
{ m_custom.push_back(wp); }

// ── Grid/Snap ────────────────────────────────────────────────────────────

Vec3 CadGridSnap::snap(const Vec3& point,
                         const Workplane& workplane,
                         const CadDocument& doc) const noexcept
{
    Vec3 result = point;

    if (m_config.snapToGrid) {
        float gx = std::round(point.x / m_config.gridSpacing) * m_config.gridSpacing;
        float gy = std::round(point.y / m_config.gridSpacing) * m_config.gridSpacing;
        float gz = std::round(point.z / m_config.gridSpacing) * m_config.gridSpacing;
        result = {gx, gy, gz};
    }

    (void)workplane; (void)doc;
    return result;
}

// ── Feature Editor ───────────────────────────────────────────────────────

bool CadFeatureEditor::setHeight(
    CadDocument& doc,
    parametric::FeatureId id,
    float newHeight) noexcept
{
    if (!doc.history().setHeight(id, newHeight)) return false;
    doc.history().rebuild();
    doc.markModified();
    return true;
}

bool CadFeatureEditor::setSketchPoints(
    CadDocument& doc,
    parametric::FeatureId id,
    const std::vector<std::pair<double, double>>& xyPoints) noexcept
{
    auto* node = doc.history().node(id);
    if (!node) return false;

    // Clear existing points and re-add.
    node->sketch.points.clear();
    parametric::SketchProfile newProfile = node->sketch;

    for (const auto& [x, y] : xyPoints) {
        (void)parametric::ParametricSketchFactory::addSketchPoint(newProfile, x, y);
    }

    node->sketch = std::move(newProfile);
    node->dirty = true;
    doc.history().rebuild();
    doc.markModified();
    return true;
}

bool CadFeatureEditor::setCapEnds(
    CadDocument& doc,
    parametric::FeatureId id,
    bool enabled) noexcept
{
    if (!doc.history().setCapEnds(id, enabled)) return false;
    doc.history().rebuild();
    doc.markModified();
    return true;
}

bool CadFeatureEditor::setSuppressed(
    CadDocument& doc,
    parametric::FeatureId id,
    bool suppressed) noexcept
{
    auto* node = doc.history().node(id);
    if (!node) return false;

    if (suppressed) {
        node->hidden = true;
    } else {
        node->hidden = false;
    }

    return true;
}

} // namespace nexus::cad
