#include <nexus/geometry/ModelingShell.h>

#include <algorithm>
#include <bit>
#include <cstdint>

namespace nexus::geometry {

namespace {

[[nodiscard]] bool isFiniteFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

} // namespace

std::vector<std::string> ModelingShell::guidedIntroSteps()
{
    return {
        "Pick a starter primitive for your blockout.",
        "Use quick cleanup to smooth topology and edge flow.",
        "Inspect diagnostics overlay before export.",
        "Export the resulting mesh when validation is clean.",
    };
}

ModelingShell& ModelingShell::startStarterModel(StarterPrimitive primitive,
                                                float size)
{
    const float sanitizedSize = isFiniteFloat(size) ? size : 0.01f;
    const float clampedSize = std::max(sanitizedSize, 0.01f);

    Mesh starter;
    switch (primitive) {
    case StarterPrimitive::Box:
        starter = primitives::makeBox(clampedSize, clampedSize, clampedSize);
        break;
    case StarterPrimitive::Plane:
        starter = primitives::makePlane(clampedSize, clampedSize, 1u, 1u);
        break;
    case StarterPrimitive::Sphere:
        starter = primitives::makeSphere(clampedSize * 0.5f);
        break;
    case StarterPrimitive::Cylinder:
        starter = primitives::makeCylinder(clampedSize * 0.5f, clampedSize);
        break;
    case StarterPrimitive::Cone:
        starter = primitives::makeCone(clampedSize * 0.5f, clampedSize);
        break;
    case StarterPrimitive::Capsule:
        starter = primitives::makeCapsule(clampedSize * 0.25f, clampedSize * 0.5f);
        break;
    }

    m_workflow.init(std::move(starter));
    m_workflow.rebuildNormals();
    m_overlayValid = refreshDiagnostics();
    return *this;
}

ModelingShell& ModelingShell::quickCleanup(const BevelChamferDesc& bevel,
                                           const RemeshDesc& remesh)
{
    BevelChamferDesc effectiveBevel = bevel;
    if (!isFiniteFloat(effectiveBevel.distance) || effectiveBevel.distance <= 0.0f) {
        effectiveBevel.distance = 0.025f;
    }

    RemeshDesc effectiveRemesh = remesh;
    if (!isFiniteFloat(effectiveRemesh.targetEdgeLength) || effectiveRemesh.targetEdgeLength <= 0.0f) {
        effectiveRemesh.targetEdgeLength = 0.25f;
    }
    if (effectiveRemesh.maxIterations == 0u) {
        effectiveRemesh.maxIterations = 1u;
    }

    m_workflow.triangulate()
              .bevelEdges(effectiveBevel)
              .triangulate()
              .remesh(effectiveRemesh)
              .rebuildNormals();

    m_overlayValid = refreshDiagnostics();
    return *this;
}

bool ModelingShell::refreshDiagnostics(const MeshDiagnosticOverlayOptions& options) noexcept
{
    const bool ok = MeshDiagnosticExporter::extract(m_workflow.mesh(), options, m_overlay);
    m_overlayValid = ok;
    return ok;
}

bool ModelingShell::isReady() const noexcept
{
    return m_workflow.mesh().isValid() && m_workflow.isValid() && m_overlayValid;
}

ModelingShellSessionReport ModelingShell::sessionReport() const
{
    ModelingShellSessionReport report{};
    report.success = isReady();
    report.workflowSteps = m_workflow.stepReports();
    report.overlay = m_overlay;
    report.sessionLabel = m_sessionLabel;

    if (report.workflowSteps.empty()) {
        report.message = "No workflow steps executed.";
    } else if (report.success) {
        report.message = report.workflowSteps.back().message;
    } else {
        // Surface the first failing step so callers see the actual blocker
        // instead of the cosmetically-last "Normals rebuilt." success message.
        // Falls back to the last step when no explicit failure was recorded
        // (e.g. mesh validity fails after every step succeeded individually).
        report.message = report.workflowSteps.back().message;
        for (const auto& step : report.workflowSteps) {
            if (!step.success) {
                report.message = step.message;
                break;
            }
        }
    }

    return report;
}

} // namespace nexus::geometry
