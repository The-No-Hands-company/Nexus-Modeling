#include <nexus/cad/CadOperations.h>
#include <nexus/geometry/BevelChamfer.h>
#include <cmath>

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
    m_savedMesh = *node->mesh;
    geometry::BevelChamferDesc desc;
    desc.mode = geometry::BevelChamferMode::Bevel;
    desc.distance = std::min(m_radius, 1.f);
    geometry::Mesh output;
    auto report = geometry::BevelChamferOperation::apply(*node->mesh, desc, output);
    if (report.isSuccess()) { node->mesh.emplace(std::move(output)); m_executed = true; return true; }
    return false;
}

bool FilletCommand::undo(CadDocument& doc)
{
    if (!m_executed) return false;
    auto* node = doc.history().node(m_featureId);
    if (!node || !m_savedMesh) return false;
    node->mesh.emplace(std::move(*m_savedMesh));
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
    auto* node = doc.history().node(m_featureId);
    if (!node || !node->mesh) return false;
    m_savedMesh = *node->mesh;
    geometry::BevelChamferDesc desc;
    desc.mode = geometry::BevelChamferMode::Chamfer;
    desc.distance = std::min(m_distance, 1.f);
    geometry::Mesh output;
    auto report = geometry::BevelChamferOperation::apply(*node->mesh, desc, output);
    if (report.isSuccess()) { node->mesh.emplace(std::move(output)); m_executed = true; return true; }
    return false;
}

bool ChamferCommand::undo(CadDocument& doc)
{
    if (!m_executed) return false;
    auto* node = doc.history().node(m_featureId);
    if (!node || !m_savedMesh) return false;
    node->mesh.emplace(std::move(*m_savedMesh));
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
    auto* node = doc.history().node(m_featureId);
    if (!node || !node->mesh) return false;
    // Act on the original mesh in-place, but we save for undo via normal document undo.
    auto pos = node->mesh->attributes().positions();
    Vec3 n = m_planeNormal.normalize();
    Vec3 o = m_planePoint;
    for (auto& v : pos) {
        float d = (v - o).dot(n);
        v = v - n * (2.f * d);
    }
    node->mesh->attributes().setPositions(std::move(pos));
    m_executed = true;
    return true;
}

bool MirrorCommand::undo(CadDocument& doc)
{
    if (!m_executed) return false;
    // Mirror is its own inverse — re-apply.
    auto* node = doc.history().node(m_featureId);
    if (!node || !node->mesh) return false;
    auto pos = node->mesh->attributes().positions();
    Vec3 n = m_planeNormal.normalize();
    Vec3 o = m_planePoint;
    for (auto& v : pos) {
        float d = (v - o).dot(n);
        v = v - n * (2.f * d);
    }
    node->mesh->attributes().setPositions(std::move(pos));
    m_executed = false;
    return true;
}

} // namespace nexus::cad
