#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — Session, stroke lifecycle, and undo deltas (Slice 1-3)
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
//  Masking (Slice 3):
//      session.setMask(vertexIndex, 0.f);   // fully protect vertex
//      session.setMask(vertexIndex, 0.5f);  // half-strength displacement
//      session.clearMask();                 // restore full displacement everywhere
//      session.invertMask();                // swap protected/unprotected regions
//      session.floodFillMask(value);        // set every vertex to value
//      Effective displacement weight = radialWeight * mask[v].
//
//  Symmetry (Slice 3):
//      session.setSymmetry(SymmetryAxes::X);  // mirror strokes across X = 0
//      session.setSymmetry(SymmetryAxes::None); // disable symmetry
//      When symmetry is active, each applySample call generates one or more
//      mirrored stamps in addition to the primary stamp. Mirrored stamps are
//      processed in deterministic order (X then Y then Z then XY then XZ...).
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

/// Bitmask of symmetry axes. Combine with bitwise-OR.
enum class SymmetryAxes : uint8_t {
    None = 0,
    X    = 1 << 0,  ///< Mirror across the plane x=0.
    Y    = 1 << 1,  ///< Mirror across the plane y=0.
    Z    = 1 << 2,  ///< Mirror across the plane z=0.
};

[[nodiscard]] inline SymmetryAxes operator|(SymmetryAxes a, SymmetryAxes b) noexcept
{
    return static_cast<SymmetryAxes>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
[[nodiscard]] inline bool hasSymmetryAxis(SymmetryAxes axes, SymmetryAxes bit) noexcept
{
    return (static_cast<uint8_t>(axes) & static_cast<uint8_t>(bit)) != 0;
}

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

    // ── Mask API (Slice 3) ───────────────────────────────────────────────────

    /// Set the mask weight for a single vertex. `value` is clamped to [0, 1].
    /// 0 = fully masked (no displacement); 1 = fully unmasked (full displacement).
    /// Returns false if `vertexIndex` is out of range.
    bool setMask(uint32_t vertexIndex, float value) noexcept;

    /// Read the current mask value for a vertex. Returns 1.f for out-of-range indices.
    [[nodiscard]] float getMask(uint32_t vertexIndex) const noexcept;

    /// Set every vertex mask to `value` (clamped to [0, 1]).
    void floodFillMask(float value) noexcept;

    /// Set every vertex mask to 1.0 (equivalent to floodFillMask(1.f)).
    void clearMask() noexcept;

    /// Invert: each mask[v] becomes (1 - mask[v]).
    void invertMask() noexcept;

    // ── Symmetry API (Slice 3) ───────────────────────────────────────────────

    /// Set the active symmetry axes. Applied on every subsequent applySample call.
    void setSymmetry(SymmetryAxes axes) noexcept;

    /// Return the currently active symmetry axes.
    [[nodiscard]] SymmetryAxes symmetry() const noexcept;

private:
    struct OpenStroke {
        StrokeId    id        = kInvalidStrokeId;
        BrushKind   kind      = BrushKind::Draw;
        BrushParams params    = {};
        uint64_t    lastSequence = 0;
        bool        firstSample  = true;
        /// World-space position of the first sample; used by Grab as the rigid
        /// translation origin. Set on the first applySample call.
        nexus::render::Vec3 grabOrigin = {};
        /// vertexIndex -> baseline position captured on first touch this stroke.
        std::vector<std::pair<uint32_t, nexus::render::Vec3>> baseline;
        /// Parallel index set for O(log n) "was-this-vertex-touched" lookup in
        /// the baseline vector. baseline is kept sorted ascending by index.
    };

    void rebuildAdjacency();
    void captureBaselineIfNew(OpenStroke& stroke, uint32_t vertexIndex);
    void writeBackPositions();

    /// Apply one brush stamp to all vertices given a specific sample position.
    /// Used for both primary and mirrored stamps.
    void applyStamp(OpenStroke& stroke, const BrushSample& sample);

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
    // Slice 2 brush kernels.
    [[nodiscard]] nexus::render::Vec3 applyCrease(const OpenStroke&, uint32_t vIdx,
                                                  float weight) const;
    [[nodiscard]] nexus::render::Vec3 applyLayer(const OpenStroke&, uint32_t vIdx,
                                                 float weight) const;
    [[nodiscard]] nexus::render::Vec3 applyGrab(const OpenStroke&, uint32_t vIdx,
                                                const BrushSample&, float weight) const;

    nexus::geometry::Mesh& m_mesh;
    std::vector<nexus::render::Vec3>   m_workingPositions;
    std::vector<nexus::render::Vec3>   m_workingNormals;
    std::vector<std::vector<uint32_t>> m_adjacency;  ///< vertexIndex -> sorted neighbor indices
    std::vector<float>                 m_mask;        ///< Per-vertex mask [0,1]; 1=unmasked.
    std::optional<OpenStroke>          m_openStroke;
    StrokeId                           m_nextStrokeId = 1u;
    SculptStats                        m_stats        = {};
    SymmetryAxes                       m_symmetry     = SymmetryAxes::None;
};

} // namespace nexus::sculpt
