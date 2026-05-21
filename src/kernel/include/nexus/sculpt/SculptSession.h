#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — Session, stroke lifecycle, and undo deltas (Slice 1)
//
//  SculptSession holds a non-owning reference to a Mesh and applies brush
//  strokes deterministically on the CPU. Behavior is identical on the Null
//  backend and on Vulkan (no GPU dependency).
//
//  Stroke lifecycle:
//      auto id = session.beginStroke(BrushKind::Draw, params);
//      session.applySample(id, sample0);
//      session.applySample(id, sample1);
//      StrokeDelta delta = session.endStroke(id);
//      // ... later
//      session.undoStroke(delta);
//      session.redoStroke(delta);
//
//  Determinism:
//      - Vertex iteration is in ascending vertex-index order.
//      - applySample requires strictly increasing BrushSample::sequence per stroke.
//      - StrokeDelta's vertexDeltas are sorted by vertex index.
//
//  Ownership / invariants:
//      - The Mesh referenced by the session must outlive the session.
//      - External writers to the mesh while a stroke is open invalidate that stroke;
//        endStroke returns an invalid delta in that case.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>     // Vec3
#include <nexus/sculpt/Brush.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace nexus::sculpt {

using StrokeId = uint32_t;
inline constexpr StrokeId kInvalidStrokeId = 0u;

/// Cumulative per-stroke displacement record. Stored after endStroke for undo/redo.
struct StrokeDelta {
    StrokeId  id   = kInvalidStrokeId;
    BrushKind kind = BrushKind::Draw;

    /// (vertexIndex, finalPos - baselinePos), sorted ascending by vertexIndex.
    std::vector<std::pair<uint32_t, nexus::render::Vec3>> vertexDeltas;

    [[nodiscard]] bool isValid() const noexcept { return id != kInvalidStrokeId; }
    [[nodiscard]] uint32_t touchedVertexCount() const noexcept
    {
        return static_cast<uint32_t>(vertexDeltas.size());
    }
};

/// Aggregate counters across the session's lifetime. Reset via resetStats().
struct SculptStats {
    uint32_t strokesStarted    = 0;
    uint32_t strokesCommitted  = 0;
    uint32_t samplesProcessed  = 0;
    uint32_t verticesTouched   = 0;  ///< Sum of touchedVertexCount over committed strokes.
    uint32_t undoApplied       = 0;
    uint32_t redoApplied       = 0;
};

class SculptSession {
public:
    /// Constructs a session referencing `mesh`. Caches a working copy of vertex
    /// positions and the 1-ring adjacency. The mesh must outlive this session.
    explicit SculptSession(nexus::geometry::Mesh& mesh);

    /// Recomputes the working position cache and adjacency from the current
    /// mesh state. Use after external code mutates the mesh between strokes.
    void resync();

    /// Begins a new stroke. Returns kInvalidStrokeId if a stroke is already open,
    /// if params are invalid (radius <= 0, direction zero when useVertexNormal=false),
    /// or if the mesh has no positions.
    [[nodiscard]] StrokeId beginStroke(BrushKind kind, const BrushParams& params);

    /// Applies a single brush sample to the open stroke. Returns false if `id`
    /// does not match the open stroke or if sample.sequence is not strictly greater
    /// than the previous sample in this stroke. Mesh positions are updated immediately.
    bool applySample(StrokeId id, const BrushSample& sample);

    /// Closes the stroke and returns the cumulative displacement delta. The returned
    /// StrokeDelta is invalid (kInvalidStrokeId) if `id` does not match the open stroke.
    [[nodiscard]] StrokeDelta endStroke(StrokeId id);

    /// Re-applies the inverse of `delta` to the mesh. Returns false if delta is invalid
    /// or references vertex indices outside the current mesh.
    bool undoStroke(const StrokeDelta& delta);

    /// Re-applies `delta` to the mesh. Returns false if delta is invalid or references
    /// vertex indices outside the current mesh.
    bool redoStroke(const StrokeDelta& delta);

    /// True between beginStroke() and endStroke().
    [[nodiscard]] bool strokeInProgress() const noexcept { return m_openStroke.has_value(); }

    [[nodiscard]] const SculptStats& stats() const noexcept { return m_stats; }
    void resetStats() noexcept { m_stats = {}; }

    /// Read-only view of the working position cache.
    [[nodiscard]] const std::vector<nexus::render::Vec3>& workingPositions() const noexcept
    {
        return m_workingPositions;
    }

private:
    struct OpenStroke {
        StrokeId    id        = kInvalidStrokeId;
        BrushKind   kind      = BrushKind::Draw;
        BrushParams params    = {};
        uint64_t    lastSequence = 0;
        bool        firstSample  = true;
        /// vertexIndex -> baseline position captured on first touch this stroke.
        std::vector<std::pair<uint32_t, nexus::render::Vec3>> baseline;
        /// Parallel index set for O(log n) "was-this-vertex-touched" lookup in
        /// the baseline vector. baseline is kept sorted ascending by index.
    };

    void rebuildAdjacency();
    void captureBaselineIfNew(OpenStroke& stroke, uint32_t vertexIndex);
    void writeBackPositions();

    /// Brush kernels — each returns the new vertex position. None mutate state.
    [[nodiscard]] nexus::render::Vec3 applyDraw(const OpenStroke&, uint32_t vIdx,
                                                const BrushSample&, float weight) const;
    [[nodiscard]] nexus::render::Vec3 applySmooth(const OpenStroke&, uint32_t vIdx,
                                                  float weight) const;
    [[nodiscard]] nexus::render::Vec3 applyInflate(const OpenStroke&, uint32_t vIdx,
                                                   const BrushSample&, float weight) const;
    [[nodiscard]] nexus::render::Vec3 applyFlatten(const OpenStroke&, uint32_t vIdx,
                                                   const BrushSample&, float weight) const;
    [[nodiscard]] nexus::render::Vec3 applyPinch(const OpenStroke&, uint32_t vIdx,
                                                 const BrushSample&, float weight) const;

    nexus::geometry::Mesh& m_mesh;
    std::vector<nexus::render::Vec3>   m_workingPositions;
    std::vector<nexus::render::Vec3>   m_workingNormals;
    std::vector<std::vector<uint32_t>> m_adjacency;  ///< vertexIndex -> sorted neighbor indices
    std::optional<OpenStroke>          m_openStroke;
    StrokeId                           m_nextStrokeId = 1u;
    SculptStats                        m_stats        = {};
};

} // namespace nexus::sculpt
