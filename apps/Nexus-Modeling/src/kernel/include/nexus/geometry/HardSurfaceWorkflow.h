#pragma once
// ── Nexus Geometry — HardSurfaceWorkflow
//
// Non-destructive hard-surface modeling workflow with selection management,
// modifier-style operation recording, and topology queries via HalfEdgeMesh.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/RemeshOperation.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace nexus::geometry {

enum class HardSurfaceStepKind : uint8_t {
    Init,
    RebuildNormals,
    EdgeSelection,
    FaceSelection,
    BevelSelected,
    ExtrudeSelected,
    InsetSelected,
    BevelChamfer,
    Remesh,
};

enum class HardSurfaceOpKind : uint8_t {
    Init,
    BevelSelected,
    ExtrudeSelected,
    InsetSelected,
    Remesh,
    RebuildNormals,
    BevelChamferLegacy,
};

struct HardSurfaceOperation {
    HardSurfaceOpKind kind = HardSurfaceOpKind::Init;
    float             distance = 0.f;
    int               segments = 1;
    RemeshDesc        remeshDesc;
    BevelChamferDesc  bevelDesc;
    bool              success = false;
};

struct HardSurfaceWorkflowStepReport {
    HardSurfaceStepKind kind    = HardSurfaceStepKind::Init;
    std::string         message;
    bool                success = true;
};

class HardSurfaceWorkflow {
public:
    void init(Mesh&& mesh);

    // ── Selection ──────────────────────────────────────────────────────────
    void selectEdge(uint32_t edgeIndex);
    void selectEdges(const std::vector<uint32_t>& edgeIndices);
    void selectEdgeLoop(uint32_t seedEdge);
    void selectEdgeRing(uint32_t seedEdge);
    void clearEdgeSelection() noexcept;

    void selectFace(uint32_t faceIndex);
    void selectFaces(const std::vector<uint32_t>& faceIndices);
    void clearFaceSelection() noexcept;

    [[nodiscard]] bool hasSelectedEdges() const noexcept { return !m_selectedEdges.empty(); }
    [[nodiscard]] bool hasSelectedFaces() const noexcept { return !m_selectedFaces.empty(); }
    [[nodiscard]] const std::unordered_set<uint32_t>& selectedEdges() const noexcept { return m_selectedEdges; }
    [[nodiscard]] const std::unordered_set<uint32_t>& selectedFaces() const noexcept { return m_selectedFaces; }

    // ── Operations (act on current selection, recorded for rebuild) ────────
    HardSurfaceWorkflow& bevelSelectedEdges(float width, int segments);
    HardSurfaceWorkflow& extrudeSelectedFaces(float distance);
    HardSurfaceWorkflow& insetSelectedFaces(float distance);
    HardSurfaceWorkflow& remesh(const RemeshDesc& desc);

    // Legacy API (backward compatibility)
    HardSurfaceWorkflow& bevelEdges(const BevelChamferDesc& desc);
    void rebuildNormals();

    // ── Modifier-style rebuild ─────────────────────────────────────────────
    const Mesh& rebuild();

    // ── Accessors ──────────────────────────────────────────────────────────
    [[nodiscard]] const Mesh& mesh() const { return m_mesh; }
    [[nodiscard]] bool isValid() const { return m_mesh.isValid(); }
    [[nodiscard]] uint32_t operationCount() const noexcept { return m_operationCount; }
    [[nodiscard]] std::vector<HardSurfaceWorkflowStepReport> stepReports() const { return m_stepReports; }
    [[nodiscard]] const std::vector<HardSurfaceOperation>& operations() const noexcept { return m_operations; }

private:
    Mesh                                       m_mesh;
    Mesh                                       m_originalMesh;
    std::vector<HardSurfaceWorkflowStepReport> m_stepReports;

    std::unordered_set<uint32_t> m_selectedEdges;
    std::unordered_set<uint32_t> m_selectedFaces;

    std::vector<HardSurfaceOperation> m_operations;
    uint32_t                          m_operationCount = 0;

    void rebuildHEMIfStale();

    [[nodiscard]] std::vector<uint32_t> computeEdgeLoop(uint32_t seedEdge) const;
    [[nodiscard]] std::vector<uint32_t> computeEdgeRing(uint32_t seedEdge) const;

    void addReport(HardSurfaceStepKind kind, std::string msg, bool success);

    // Internal apply helpers used by rebuild()
    bool applyBevel(const HardSurfaceOperation& op);
    bool applyExtrude(const HardSurfaceOperation& op);
    bool applyInset(const HardSurfaceOperation& op);
    bool applyRemesh(const HardSurfaceOperation& op);
    bool applyBevelLegacy(const HardSurfaceOperation& op);
};

} // namespace nexus::geometry
