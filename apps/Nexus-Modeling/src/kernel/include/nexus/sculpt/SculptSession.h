#pragma once
#include <nexus/geometry/Mesh.h>
#include <nexus/sculpt/Brush.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace nexus::sculpt {

using StrokeId = uint32_t;
constexpr StrokeId kInvalidStrokeId = 0u;

struct StrokeDelta {
    StrokeId id = kInvalidStrokeId;
    BrushKind kind = BrushKind::Draw;
    std::vector<std::pair<uint32_t, render::Vec3>> vertexDeltas;

    [[nodiscard]] bool isValid() const noexcept { return id != kInvalidStrokeId; }
    [[nodiscard]] uint32_t touchedVertexCount() const noexcept {
        return static_cast<uint32_t>(vertexDeltas.size());
    }
};

struct SculptStats {
    uint32_t strokesStarted = 0;
    uint32_t strokesCommitted = 0;
    uint32_t samplesProcessed = 0;
    uint32_t verticesTouched = 0;
    uint32_t undoApplied = 0;
    uint32_t redoApplied = 0;
};

enum class SymmetryAxes : uint8_t {
    None = 0,
    X    = 1 << 0,
    Y    = 1 << 1,
    Z    = 1 << 2,
};

inline constexpr SymmetryAxes operator|(SymmetryAxes a, SymmetryAxes b) noexcept {
    return static_cast<SymmetryAxes>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline constexpr SymmetryAxes operator&(SymmetryAxes a, SymmetryAxes b) noexcept {
    return static_cast<SymmetryAxes>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline constexpr bool hasSymmetry(SymmetryAxes axes, SymmetryAxes flag) noexcept {
    return (static_cast<uint8_t>(axes) & static_cast<uint8_t>(flag)) != 0;
}

class SculptSession {
public:
    explicit SculptSession(geometry::Mesh& mesh);

    void resync();

    StrokeId beginStroke(BrushKind kind, const BrushParams& params);
    bool applySample(StrokeId id, const BrushSample& sample);
    StrokeDelta endStroke(StrokeId id);
    bool undoStroke(const StrokeDelta& delta);
    bool redoStroke(const StrokeDelta& delta);
    bool strokeInProgress() const noexcept { return m_activeStrokeId != kInvalidStrokeId; }

    void setMask(float value);
    void setMaskVertex(uint32_t vertexIndex, float value);
    void clearMask();
    void invertMask();
    void floodFillMask(uint32_t seedVertex);
    [[nodiscard]] float maskAt(uint32_t vertexIndex) const;
    [[nodiscard]] const std::vector<float>& mask() const noexcept { return m_mask; }

    void setSymmetry(SymmetryAxes axes);
    [[nodiscard]] SymmetryAxes symmetryAxes() const noexcept { return m_symmetryAxes; }

    [[nodiscard]] const SculptStats& stats() const noexcept { return m_stats; }

private:
    void buildAdjacency();
    render::Vec3 mirrorPosition(const render::Vec3& pos, SymmetryAxes axis) const noexcept;

    void applyDrawKernel(const BrushParams& params, const BrushSample& sample,
                         std::vector<render::Vec3>& targetPositions);
    void applySmoothKernel(const BrushParams& params, const BrushSample& sample,
                           std::vector<render::Vec3>& targetPositions);
    void applyInflateKernel(const BrushParams& params, const BrushSample& sample,
                            std::vector<render::Vec3>& targetPositions);
    void applyFlattenKernel(const BrushParams& params, const BrushSample& sample,
                            std::vector<render::Vec3>& targetPositions);
    void applyPinchKernel(const BrushParams& params, const BrushSample& sample,
                          std::vector<render::Vec3>& targetPositions);
    void applyCreaseKernel(const BrushParams& params, const BrushSample& sample,
                           std::vector<render::Vec3>& targetPositions);
    void applyLayerKernel(const BrushParams& params, const BrushSample& sample,
                          std::vector<render::Vec3>& targetPositions);
    void applyGrabKernel(const BrushParams& params, const BrushSample& sample,
                         std::vector<render::Vec3>& targetPositions);

    geometry::Mesh& m_mesh;

    std::vector<render::Vec3> m_baselinePositions;
    std::vector<render::Vec3> m_strokePositions;
    std::vector<render::Vec3> m_normals;
    std::vector<std::vector<uint32_t>> m_adjacency;
    std::vector<float> m_mask;

    StrokeId m_activeStrokeId = kInvalidStrokeId;
    StrokeId m_nextStrokeId = 1;
    BrushKind m_activeKind = BrushKind::Draw;
    BrushParams m_activeParams;
    uint64_t m_lastSampleSequence = 0;

    render::Vec3 m_lastGrabPosition = {0.f, 0.f, 0.f};
    bool m_grabPositionValid = false;

    SymmetryAxes m_symmetryAxes = SymmetryAxes::None;

    SculptStats m_stats;
};

} // namespace nexus::sculpt
