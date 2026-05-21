#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Simple-by-default Modeling Shell (Month 5)
//
//  Thin ergonomic wrapper over HardSurfaceWorkflow that provides:
//  - starter primitive selection
//  - guided intro text for first-use flows
//  - one-call quick cleanup toolset (bevel + remesh + normals)
//  - diagnostics overlay extraction for UI consumption
//
//  This is intentionally small and deterministic; advanced authoring remains in
//  operation-level APIs.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HardSurfaceWorkflow.h>
#include <nexus/geometry/MeshDiagnosticOverlay.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

enum class StarterPrimitive : uint8_t {
    Box,
    Plane,
    Sphere,
    Cylinder,
    Cone,
    Capsule,
};

struct ModelingShellSessionReport {
    bool success = false;
    std::string message;
    // Caller-supplied human-readable label for this session (e.g. scenario
    // id, starter primitive name, source asset path). Empty by default.
    // See ModelingShell::setSessionLabel().
    std::string sessionLabel;
    std::vector<HardSurfaceStepReport> workflowSteps;
    MeshDiagnosticOverlayData overlay;
};

class ModelingShell {
public:
    ModelingShell() = default;

    [[nodiscard]] static std::vector<std::string> guidedIntroSteps();

    // Initializes the shell with a starter primitive and rebuilds normals.
    ModelingShell& startStarterModel(StarterPrimitive primitive,
                                     float size = 1.0f);

    // Runs a lean default post-blockout pass.
    ModelingShell& quickCleanup(const BevelChamferDesc& bevel = {},
                                const RemeshDesc& remesh = {});

    // Regenerates diagnostic overlay for the current workflow mesh.
    [[nodiscard]] bool refreshDiagnostics(const MeshDiagnosticOverlayOptions& options = {}) noexcept;

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] const MeshDiagnosticOverlayData& diagnostics() const noexcept { return m_overlay; }

    [[nodiscard]] const HardSurfaceWorkflow& workflow() const noexcept { return m_workflow; }
    [[nodiscard]] HardSurfaceWorkflow& workflow() noexcept { return m_workflow; }

    // Caller-supplied label for this session (scenario id, starter primitive
    // name, source asset path). Stored verbatim and surfaced through
    // sessionReport(). Empty by default.
    void setSessionLabel(std::string label) { m_sessionLabel = std::move(label); }
    [[nodiscard]] const std::string& sessionLabel() const noexcept { return m_sessionLabel; }

    [[nodiscard]] ModelingShellSessionReport sessionReport() const;

private:
    HardSurfaceWorkflow m_workflow;
    MeshDiagnosticOverlayData m_overlay;
    std::string m_sessionLabel;
    bool m_overlayValid = false;
};

} // namespace nexus::geometry
