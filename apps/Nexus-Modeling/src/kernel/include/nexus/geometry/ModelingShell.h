#pragma once
// ── Nexus Geometry — ModelingShell

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/RemeshOperation.h>
#include <nexus/geometry/MeshDiagnosticOverlay.h>
#include <nexus/geometry/HardSurfaceWorkflow.h>

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
    bool                                     success = false;
    std::vector<HardSurfaceWorkflowStepReport> workflowSteps;
    MeshDiagnosticOverlayData                overlay;
    std::string                              message;
};

class ModelingShell {
public:
    static std::vector<std::string> guidedIntroSteps();

    ModelingShell& startStarterModel(StarterPrimitive primitive, float size);
    ModelingShell& quickCleanup(const BevelChamferDesc& bevel,
                                const RemeshDesc& remesh);
    bool refreshDiagnostics(const MeshDiagnosticOverlayOptions& options = {}) noexcept;
    bool isReady() const noexcept;
    ModelingShellSessionReport sessionReport() const;

    const HardSurfaceWorkflow&    workflow()    const noexcept { return m_workflow; }
    const MeshDiagnosticOverlayData& diagnostics() const noexcept { return m_overlay; }

private:
    HardSurfaceWorkflow        m_workflow;
    MeshDiagnosticOverlayData  m_overlay;
    bool                       m_overlayValid = false;
};

} // namespace nexus::geometry
