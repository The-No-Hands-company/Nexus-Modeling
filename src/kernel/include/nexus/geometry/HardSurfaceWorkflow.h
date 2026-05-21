#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Hard-Surface Modeling Workflow  (Month 5)
//
//  HardSurfaceWorkflow chains geometry operations (boolean, bevel/chamfer,
//  remesh, normals rebuild) on a single working mesh.  Each step appends a
//  HardSurfaceStepReport so callers can inspect the full operation history.
//
//  Design rules:
//  - Steps always execute regardless of prior failures; the caller chooses
//    how to interpret per-step results.
//  - isValid() returns false when any step produced an invalid result.
//  - All paths are noexcept at the workflow level; exceptions from STL
//    containers are propagated as-is.
//  - Deterministic for identical inputs.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/geometry/RemeshOperation.h>

#include <string>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Step kind enumeration
// ─────────────────────────────────────────────────────────────────────────────
enum class HardSurfaceStepKind : uint8_t {
    Init,
    ApplyTransform,
    BooleanOperation,
    Triangulate,
    BevelChamfer,
    Remesh,
    RebuildNormals,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Per-step report
// ─────────────────────────────────────────────────────────────────────────────
struct HardSurfaceStepReport {
    HardSurfaceStepKind kind    = HardSurfaceStepKind::Init;
    bool                success = true;
    std::string         message;

    [[nodiscard]] bool isSuccess() const noexcept { return success; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  HardSurfaceWorkflow
// ─────────────────────────────────────────────────────────────────────────────
class HardSurfaceWorkflow {
public:
    HardSurfaceWorkflow() = default;
    explicit HardSurfaceWorkflow(Mesh initialMesh);

    // ── Mutation steps ────────────────────────────────────────────────────────

    // Replace or set the working mesh (resets prior step history).
    HardSurfaceWorkflow& init(Mesh mesh);

    // Transform all vertex positions (and direction channels) of the working mesh.
    HardSurfaceWorkflow& applyTransform(const nexus::render::Mat4& transform) noexcept;

    // Boolean union A ∪ B: merge tool geometry into the working mesh.
    HardSurfaceWorkflow& booleanUnion(const Mesh& tool,
                                      const BooleanOperationOptions& opts = {});

    // Boolean difference A – B: subtract tool geometry from the working mesh.
    HardSurfaceWorkflow& booleanDifference(const Mesh& tool,
                                           const BooleanOperationOptions& opts = {});

    // Boolean intersection A ∩ B: keep only the overlapping region.
    HardSurfaceWorkflow& booleanIntersection(const Mesh& tool,
                                             const BooleanOperationOptions& opts = {});

    // Apply bevel or chamfer to sharp edges.
    HardSurfaceWorkflow& bevelEdges(const BevelChamferDesc& desc = {});

    // Triangulate the working mesh in place. Quads split into two triangles,
    // n-gons fan-triangulate (vertex 0 as pivot). Safe to call multiple times;
    // already-triangulated meshes pass through unchanged. Records a step report
    // with the post-triangulation face count.
    HardSurfaceWorkflow& triangulate();

    // Remesh the working mesh to uniform edge length.
    HardSurfaceWorkflow& remesh(const RemeshDesc& desc = {});

    // Recompute per-vertex normals from face geometry.
    HardSurfaceWorkflow& rebuildNormals() noexcept;

    // ── Query ─────────────────────────────────────────────────────────────────

    [[nodiscard]] const Mesh& mesh()   const noexcept { return m_mesh; }
    [[nodiscard]] Mesh       releaseMesh() noexcept   { return std::move(m_mesh); }

    // Returns true when the mesh is currently valid and every step succeeded.
    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] const std::vector<HardSurfaceStepReport>& stepReports() const noexcept
    {
        return m_reports;
    }

    // Returns the most recent step report, or a default-constructed report if
    // no steps have been taken.
    [[nodiscard]] HardSurfaceStepReport lastStepReport() const noexcept;

    // ── Export shortcut ───────────────────────────────────────────────────────

    [[nodiscard]] MeshExportReport exportMesh(const std::string& path,
                                              const MeshExportOptions& opts = {}) const;

private:
    void pushBoolean(const Mesh& tool,
                     BooleanOperationType type,
                     const BooleanOperationOptions& opts);

    Mesh                                m_mesh;
    std::vector<HardSurfaceStepReport>  m_reports;
};

} // namespace nexus::geometry
