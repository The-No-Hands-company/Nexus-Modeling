// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Hard-Surface Modeling Workflow implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/HardSurfaceWorkflow.h>
#include <nexus/geometry/MeshIO.h>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Constructors
// ─────────────────────────────────────────────────────────────────────────────

HardSurfaceWorkflow::HardSurfaceWorkflow(Mesh initialMesh)
{
    init(std::move(initialMesh));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mutation steps
// ─────────────────────────────────────────────────────────────────────────────

HardSurfaceWorkflow& HardSurfaceWorkflow::init(Mesh mesh)
{
    m_mesh = std::move(mesh);
    m_reports.clear();

    HardSurfaceStepReport rep{};
    rep.kind    = HardSurfaceStepKind::Init;
    rep.success = m_mesh.isValid();
    rep.message = rep.success ? "Mesh initialized." : "Init mesh is invalid.";
    m_reports.push_back(std::move(rep));
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::applyTransform(
    const nexus::render::Mat4& transform) noexcept
{
    HardSurfaceStepReport rep{};
    rep.kind = HardSurfaceStepKind::ApplyTransform;

    const bool ok = m_mesh.applyTransform(transform);
    rep.success = ok;
    rep.message = ok ? "Transform applied." : "Transform failed (invalid mesh or degenerate transform).";
    m_reports.push_back(std::move(rep));
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::booleanUnion(
    const Mesh& tool, const BooleanOperationOptions& opts)
{
    pushBoolean(tool, BooleanOperationType::Union, opts);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::booleanDifference(
    const Mesh& tool, const BooleanOperationOptions& opts)
{
    pushBoolean(tool, BooleanOperationType::Difference, opts);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::booleanIntersection(
    const Mesh& tool, const BooleanOperationOptions& opts)
{
    pushBoolean(tool, BooleanOperationType::Intersection, opts);
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::bevelEdges(const BevelChamferDesc& desc)
{
    HardSurfaceStepReport rep{};
    rep.kind = HardSurfaceStepKind::BevelChamfer;

    Mesh output;
    const BevelChamferReport bvRep = BevelChamferOperation::apply(m_mesh, desc, output);
    rep.success = bvRep.isSuccess();

    if (rep.success) {
        m_mesh = std::move(output);
        rep.message = "Bevel/chamfer applied ("
                    + std::to_string(bvRep.splitEdgeCount) + " edges split).";
    } else {
        rep.message = "Bevel/chamfer failed.";
        if (!bvRep.messages.empty()) {
            rep.message += " " + bvRep.messages.front();
        }
    }

    m_reports.push_back(std::move(rep));
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::triangulate()
{
    HardSurfaceStepReport rep{};
    rep.kind = HardSurfaceStepKind::Triangulate;

    const size_t beforeFaces = m_mesh.topology().faceCount();
    const size_t afterFaces  = m_mesh.topology().triangulate();
    // Triangulation can invalidate stable element IDs (face topology changes);
    // rebuild them so downstream operations that rely on stable IDs stay sound.
    if (m_mesh.hasStableElementIds()) {
        m_mesh.rebuildStableElementIds();
    }

    rep.success = m_mesh.isValid();
    if (rep.success) {
        rep.message = "Triangulated ("
                    + std::to_string(beforeFaces) + " -> "
                    + std::to_string(afterFaces) + " faces).";
    } else {
        rep.message = "Triangulate failed (invalid mesh state).";
    }

    m_reports.push_back(std::move(rep));
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::remesh(const RemeshDesc& desc)
{
    HardSurfaceStepReport rep{};
    rep.kind = HardSurfaceStepKind::Remesh;

    Mesh output;
    const RemeshReport rmRep = RemeshOperation::apply(m_mesh, desc, output);
    rep.success = rmRep.isSuccess();

    if (rep.success) {
        m_mesh = std::move(output);
        rep.message = "Remesh applied ("
                    + std::to_string(rmRep.splitCount) + " splits, "
                    + std::to_string(rmRep.collapseCount) + " collapses, "
                    + std::to_string(rmRep.outputFaceCount) + " output faces).";
    } else {
        rep.message = "Remesh failed.";
        if (!rmRep.messages.empty()) {
            rep.message += " " + rmRep.messages.front();
        }
    }

    m_reports.push_back(std::move(rep));
    return *this;
}

HardSurfaceWorkflow& HardSurfaceWorkflow::rebuildNormals() noexcept
{
    HardSurfaceStepReport rep{};
    rep.kind = HardSurfaceStepKind::RebuildNormals;

    const bool ok = m_mesh.computeVertexNormals();
    rep.success = ok;
    rep.message = ok ? "Normals rebuilt." : "Normal rebuild failed (invalid mesh state).";
    m_reports.push_back(std::move(rep));
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Query
// ─────────────────────────────────────────────────────────────────────────────

bool HardSurfaceWorkflow::isValid() const noexcept
{
    if (!m_mesh.isValid()) {
        return false;
    }
    for (const auto& rep : m_reports) {
        if (!rep.success) {
            return false;
        }
    }
    return true;
}

HardSurfaceStepReport HardSurfaceWorkflow::lastStepReport() const noexcept
{
    if (m_reports.empty()) {
        return {};
    }
    return m_reports.back();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Export shortcut
// ─────────────────────────────────────────────────────────────────────────────

MeshExportReport HardSurfaceWorkflow::exportMesh(const std::string& path,
                                                  const MeshExportOptions& opts) const
{
    return MeshIO::exportMesh(m_mesh, path, opts);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void HardSurfaceWorkflow::pushBoolean(const Mesh& tool,
                                      BooleanOperationType type,
                                      const BooleanOperationOptions& opts)
{
    HardSurfaceStepReport rep{};
    rep.kind = HardSurfaceStepKind::BooleanOperation;

    Mesh output;
    const BooleanOperationReport bRep =
        BooleanOperation::compute(m_mesh, tool, type, opts, output);
    rep.success = bRep.isSuccess();

    if (rep.success) {
        m_mesh = std::move(output);
        const char* opName = BooleanOperation::operationName(type);
        rep.message = std::string("Boolean ") + opName + " applied.";
    } else {
        const char* opName = BooleanOperation::operationName(type);
        rep.message = std::string("Boolean ") + opName + " failed.";
        if (!bRep.messages.empty()) {
            rep.message += " " + bRep.messages.front();
        }
    }

    m_reports.push_back(std::move(rep));
}

} // namespace nexus::geometry
