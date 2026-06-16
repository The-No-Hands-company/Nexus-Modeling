#include <nexus/cad/CadOperations.h>

namespace nexus::cad {

// ── FilletCommand ────────────────────────────────────────────────────────

FilletCommand::FilletCommand(parametric::FeatureId featureId,
                               const std::vector<uint32_t>& edgeIndices,
                               float radius)
    : m_featureId(featureId), m_edgeIndices(edgeIndices), m_radius(radius)
{}

std::string FilletCommand::description() const
{ return "Fillet (R=" + std::to_string(m_radius) + ")"; }

bool FilletCommand::execute(CadDocument& doc)
{
    auto* node = doc.history().node(m_featureId);
    if (!node || !node->mesh) return false;
    (void)doc;
    m_executed = true;
    return true;
}

bool FilletCommand::undo(CadDocument& doc)
{
    (void)doc;
    m_executed = false;
    return true;
}

// ── ChamferCommand ───────────────────────────────────────────────────────

ChamferCommand::ChamferCommand(parametric::FeatureId featureId,
                                 const std::vector<uint32_t>& edgeIndices,
                                 float distance)
    : m_featureId(featureId), m_edgeIndices(edgeIndices), m_distance(distance)
{}

std::string ChamferCommand::description() const
{ return "Chamfer (D=" + std::to_string(m_distance) + ")"; }

bool ChamferCommand::execute(CadDocument& doc)
{
    (void)doc;
    m_executed = true;
    return true;
}

bool ChamferCommand::undo(CadDocument& doc)
{
    (void)doc;
    m_executed = false;
    return true;
}

// ── MirrorCommand ────────────────────────────────────────────────────────

MirrorCommand::MirrorCommand(parametric::FeatureId featureId,
                               const Vec3& planePoint,
                               const Vec3& planeNormal)
    : m_featureId(featureId), m_planePoint(planePoint), m_planeNormal(planeNormal)
{}

std::string MirrorCommand::description() const
{ return "Mirror"; }

bool MirrorCommand::execute(CadDocument& doc)
{
    (void)doc;
    m_createdId = parametric::kInvalidFeatureId;
    m_executed = true;
    return true;
}

bool MirrorCommand::undo(CadDocument& doc)
{
    (void)doc;
    m_executed = false;
    return true;
}

} // namespace nexus::cad
