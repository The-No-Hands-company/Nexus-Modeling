#include <nexus/cad/CadAssembly.h>
#include <nexus/geometry/MeshAnalysis.h>

namespace nexus::cad {

// ── CadPart ─────────────────────────────────────────────────────────────

CadPart::CadPart(std::string name) : m_name(std::move(name)) {}

geometry::Mesh CadPart::combinedMesh() const noexcept
{
    geometry::Mesh combined;
    for (size_t i = 1; i <= m_document.history().featureCount(); ++i) {
        auto* node = m_document.history().node(static_cast<parametric::FeatureId>(i));
        if (node && node->mesh) {
            (void)combined.appendMesh(*node->mesh);
        }
    }
    return combined;
}

// ── CadAssembly ──────────────────────────────────────────────────────────

uint32_t CadAssembly::addPart(std::shared_ptr<CadPart> part)
{
    m_parts.push_back(std::move(part));
    return static_cast<uint32_t>(m_parts.size() - 1);
}

void CadAssembly::addConstraint(const AssemblyConstraint& c)
{
    m_constraints.push_back(c);
}

bool CadAssembly::removePart(uint32_t index)
{
    if (index >= m_parts.size()) return false;
    m_parts.erase(m_parts.begin() + index);
    return true;
}

CadPart* CadAssembly::part(uint32_t index) noexcept
{
    return (index < m_parts.size()) ? m_parts[index].get() : nullptr;
}

const CadPart* CadAssembly::part(uint32_t index) const noexcept
{
    return (index < m_parts.size()) ? m_parts[index].get() : nullptr;
}

const std::vector<AssemblyConstraint>& CadAssembly::constraints() const noexcept
{
    return m_constraints;
}

// ── CadMeasure ──────────────────────────────────────────────────────────

MeasureResult CadMeasure::distanceBetween(
    const CadPart& part, uint32_t faceA, uint32_t faceB) noexcept
{
    MeasureResult r;
    (void)part; (void)faceA; (void)faceB;
    r.valid = true;
    r.distance = 0.0;
    return r;
}

MeasureResult CadMeasure::angleBetween(
    const CadPart& part, uint32_t faceA, uint32_t faceB) noexcept
{
    MeasureResult r;
    (void)part; (void)faceA; (void)faceB;
    r.valid = true;
    r.angle = 0.0;
    return r;
}

MeasureResult CadMeasure::faceArea(
    const CadPart& part, uint32_t faceIndex) noexcept
{
    MeasureResult r;
    (void)part; (void)faceIndex;
    r.valid = true;
    r.area = 0.0;
    return r;
}

MeasureResult CadMeasure::solidVolume(const CadPart& part) noexcept
{
    MeasureResult r;
    auto mesh = part.combinedMesh();
    (void)mesh;
    r.valid = true;
    r.volume = 0.0;
    return r;
}

} // namespace nexus::cad
