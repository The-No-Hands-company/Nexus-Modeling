#include <nexus/cad/CadExpression.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace nexus::cad {

// ── Parametric Expressions ──────────────────────────────────────────────

void CadExpressionEngine::define(const std::string& name, double value)
{ m_variables[name] = value; }

bool CadExpressionEngine::setExpression(const std::string& name, const std::string& expr)
{
    if (m_variables.find(name) == m_variables.end()) return false;
    m_expressions[name] = expr;
    return true;
}

double CadExpressionEngine::evaluate(const std::string& name) const
{
    auto it = m_variables.find(name);
    if (it == m_variables.end()) return 0.0;
    auto exprIt = m_expressions.find(name);
    if (exprIt == m_expressions.end()) return it->second;

    // Simple expression evaluator: parse "A op B" or "A * B" etc.
    std::istringstream ss(exprIt->second);
    std::string left, op, right;
    ss >> left >> op >> right;
    double lv = get(left), rv = get(right);
    if (op == "+") return lv + rv;
    if (op == "-") return lv - rv;
    if (op == "*") return lv * rv;
    if (op == "/") return (rv != 0.0) ? lv / rv : 0.0;
    return it->second;
}

std::vector<std::string> CadExpressionEngine::variables() const
{
    std::vector<std::string> vars;
    for (const auto& [k, v] : m_variables) vars.push_back(k);
    return vars;
}

double CadExpressionEngine::get(const std::string& name) const
{
    auto it = m_variables.find(name);
    return (it != m_variables.end()) ? it->second : 0.0;
}

void CadExpressionEngine::set(const std::string& name, double value)
{ m_variables[name] = value; }

void CadExpressionEngine::clear()
{ m_variables.clear(); m_expressions.clear(); }

// ── Feature Rollback ────────────────────────────────────────────────────

CadRollback::CadRollback(CadDocument& doc) : m_doc(doc) {}

bool CadRollback::rollTo(size_t featureIndex)
{
    size_t total = totalPositions();
    if (featureIndex > total) return false;

    while (m_position < featureIndex && m_doc.canRedo()) {
        m_doc.redo();
        m_position++;
    }
    while (m_position > featureIndex && m_doc.canUndo()) {
        m_doc.undo();
        m_position--;
    }
    return m_position == featureIndex;
}

size_t CadRollback::totalPositions() const noexcept
{ return m_doc.history().featureCount() + 1; }

// ── Model Comparison ────────────────────────────────────────────────────

CadDiffResult CadDiff::compare(const CadPart& partA, const CadPart& partB) noexcept
{
    CadDiffResult r;
    auto meshA = partA.combinedMesh();
    auto meshB = partB.combinedMesh();
    r.identical = (meshA.topology().faceCount() == meshB.topology().faceCount() &&
                   meshA.attributes().vertexCount() == meshB.attributes().vertexCount());
    if (!r.identical) r.differences.push_back("Face/vertex counts differ");
    return r;
}

CadDiffResult CadDiff::compareVersions(
    const CadDocument& docA, const CadDocument& docB) noexcept
{
    CadDiffResult r;
    (void)docA; (void)docB;
    return r;
}

// ── Exploded Views ──────────────────────────────────────────────────────

std::vector<ExplodedPart> CadExplode::explode(
    const CadAssembly& assembly, const ExplosionAxis& axis) noexcept
{
    std::vector<ExplodedPart> parts;
    Vec3 dir = axis.direction.normalize();
    for (uint32_t i = 0; i < assembly.partCount(); ++i) {
        ExplodedPart ep;
        ep.partIndex = i;
        ep.originalPosition = {};
        ep.explodedPosition = dir * (static_cast<float>(i) * axis.spacing);
        parts.push_back(ep);
    }
    return parts;
}

std::vector<std::vector<Vec3>> CadExplode::animateExplosion(
    const CadAssembly& assembly, uint32_t steps) noexcept
{
    (void)assembly; (void)steps;
    return {};
}

// ── Mass Properties ─────────────────────────────────────────────────────

CadMassProperties CadMassProps::compute(
    const CadPart& part, const CadMaterial& material) noexcept
{
    CadMassProperties props;
    props.material = material;
    auto mesh = part.combinedMesh();
    auto mp = geometry::MeshMassProperties::compute(mesh);
    props.volume = mp.volume;
    props.mass = mp.volume * material.density;
    props.centroid = mp.centroid;
    props.surfaceArea = 0.0; // computed separately if needed
    return props;
}

CadMassProperties CadMassProps::computeAssembly(
    const CadAssembly& assembly,
    const std::vector<CadMaterial>& materials) noexcept
{
    CadMassProperties total;
    for (uint32_t i = 0; i < assembly.partCount(); ++i) {
        auto* part = assembly.part(i);
        if (!part) continue;
        CadMaterial mat = (i < materials.size()) ? materials[i] : CadMaterial{};
        auto props = compute(*part, mat);
        total.volume += props.volume;
        total.mass += props.mass;
    }
    return total;
}

std::vector<CadMaterial> CadMassProps::standardMaterials() noexcept
{
    return {
        {"Steel", 7.85, {0.6,0.6,0.65}, "A36"},
        {"Aluminum", 2.70, {0.8,0.8,0.85}, "6061"},
        {"Titanium", 4.43, {0.6,0.6,0.6}, "Ti-6Al-4V"},
        {"Copper", 8.96, {0.85,0.5,0.3}, "C110"},
        {"Plastic ABS", 1.04, {0.9,0.85,0.7}, ""},
        {"Nylon", 1.15, {0.95,0.95,0.9}, "PA6"},
        {"Brass", 8.50, {0.85,0.75,0.3}, "C360"},
        {"Stainless 304", 8.00, {0.7,0.7,0.75}, "304"},
    };
}

// ── Custom Properties ───────────────────────────────────────────────────

void CadMetadata::setProperty(const std::string& key, const std::string& value)
{ m_props[key] = value; }

std::string CadMetadata::property(const std::string& key) const
{
    auto it = m_props.find(key);
    return (it != m_props.end()) ? it->second : "";
}

bool CadMetadata::hasProperty(const std::string& key) const noexcept
{ return m_props.find(key) != m_props.end(); }

std::vector<std::string> CadMetadata::keys() const
{
    std::vector<std::string> k;
    for (const auto& [key, val] : m_props) k.push_back(key);
    return k;
}

void CadMetadata::setDesignIntent(const std::string& intent)
{ m_designIntent = intent; }

const std::string& CadMetadata::designIntent() const noexcept
{ return m_designIntent; }

void CadMetadata::clear()
{ m_props.clear(); m_designIntent.clear(); }

} // namespace nexus::cad
