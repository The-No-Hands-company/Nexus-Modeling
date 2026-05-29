// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — SculptSession implementation (CPU-only, deterministic)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/sculpt/SculptSession.h>

#include <algorithm>
#include <cmath>

namespace nexus::sculpt {

using nexus::render::Vec3;

namespace {

[[nodiscard]] inline float lengthSq(const Vec3& v) noexcept
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] inline Vec3 safeNormalize(const Vec3& v, const Vec3& fallback) noexcept
{
    const float l2 = lengthSq(v);
    if (l2 <= 1e-12f) {
        return fallback;
    }
    const float inv = 1.f / std::sqrt(l2);
    return {v.x * inv, v.y * inv, v.z * inv};
}

/// Returns the displacement weight in [0, 1] for `worldPos` relative to a brush
/// stamp centered at `sample.position` with the given radius and falloff.
[[nodiscard]] inline float radialWeight(const Vec3& worldPos,
                                        const BrushSample& sample,
                                        float radius,
                                        FalloffShape falloff) noexcept
{
    const Vec3 d = worldPos - sample.position;
    const float dist2 = lengthSq(d);
    const float r2 = radius * radius;
    if (dist2 >= r2) {
        return 0.f;
    }
    const float t = std::sqrt(dist2) / radius;
    return evaluateFalloff(falloff, t) * std::clamp(sample.pressure, 0.f, 1.f);
}

[[nodiscard]] inline bool insertBaselineSorted(
    std::vector<std::pair<uint32_t, Vec3>>& baseline,
    uint32_t vertexIndex,
    const Vec3& position)
{
    const auto it = std::lower_bound(baseline.begin(), baseline.end(), vertexIndex,
        [](const std::pair<uint32_t, Vec3>& a, uint32_t b) { return a.first < b; });
    if (it != baseline.end() && it->first == vertexIndex) {
        return false;  // already captured
    }
    baseline.emplace(it, vertexIndex, position);
    return true;
}

} // namespace

SculptSession::SculptSession(nexus::geometry::Mesh& mesh)
    : m_mesh(mesh)
{
    resync();
}

void SculptSession::resync()
{
    m_openStroke.reset();
    m_workingPositions = m_mesh.attributes().positions();
    m_workingNormals.clear();
    if (m_mesh.attributes().hasNormals()) {
        m_workingNormals = m_mesh.attributes().normals();
    }
    rebuildAdjacency();
}

void SculptSession::rebuildAdjacency()
{
    const size_t vCount = m_workingPositions.size();
    m_adjacency.assign(vCount, {});
    const auto& topo = m_mesh.topology();
    const size_t faceCount = topo.faceCount();
    for (size_t f = 0; f < faceCount; ++f) {
        const auto& face = topo.face(f);
        const size_t n = face.indices.size();
        if (n < 2) {
            continue;
        }
        for (size_t i = 0; i < n; ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % n];
            if (a < vCount && b < vCount && a != b) {
                m_adjacency[a].push_back(b);
                m_adjacency[b].push_back(a);
            }
        }
    }
    for (auto& nbrs : m_adjacency) {
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }
}

void SculptSession::captureBaselineIfNew(OpenStroke& stroke, uint32_t vertexIndex)
{
    if (vertexIndex >= m_workingPositions.size()) {
        return;
    }
    (void)insertBaselineSorted(stroke.baseline, vertexIndex, m_workingPositions[vertexIndex]);
}

void SculptSession::writeBackPositions()
{
    std::vector<Vec3> copy = m_workingPositions;
    m_mesh.attributes().setPositions(std::move(copy));
}

StrokeId SculptSession::beginStroke(BrushKind kind, const BrushParams& params)
{
    if (m_openStroke.has_value()) {
        return kInvalidStrokeId;
    }
    if (m_workingPositions.empty()) {
        return kInvalidStrokeId;
    }
    if (params.radius <= 0.f) {
        return kInvalidStrokeId;
    }
    if (!params.useVertexNormal && lengthSq(params.direction) <= 1e-12f) {
        return kInvalidStrokeId;
    }
    if (params.useVertexNormal && m_workingNormals.size() != m_workingPositions.size()) {
        // Need normals for normal-driven brushes (Draw with useVertexNormal=true,
        // Inflate). For Smooth/Pinch we technically don't need them, but requiring
        // normals here keeps the contract simple and matches typical sculpt setup.
        if (kind == BrushKind::Inflate || (kind == BrushKind::Draw && params.useVertexNormal)) {
            return kInvalidStrokeId;
        }
    }

    OpenStroke stroke;
    stroke.id     = m_nextStrokeId++;
    stroke.kind   = kind;
    stroke.params = params;
    if (!params.useVertexNormal) {
        // Pre-normalize for kernels that consume direction directly.
        stroke.params.direction = safeNormalize(params.direction, {0.f, 1.f, 0.f});
    }
    m_openStroke = std::move(stroke);
    ++m_stats.strokesStarted;
    return m_openStroke->id;
}

bool SculptSession::applySample(StrokeId id, const BrushSample& sample)
{
    if (!m_openStroke.has_value() || m_openStroke->id != id) {
        return false;
    }
    OpenStroke& stroke = *m_openStroke;
    if (!stroke.firstSample && sample.sequence <= stroke.lastSequence) {
        return false;
    }
    // Grab brush: capture the origin on the very first sample of the stroke.
    if (stroke.firstSample && stroke.kind == BrushKind::Grab) {
        stroke.grabOrigin = sample.position;
    }
    stroke.firstSample  = false;
    stroke.lastSequence = sample.sequence;
    ++m_stats.samplesProcessed;

    const float radius   = stroke.params.radius;
    const FalloffShape f = stroke.params.falloff;
    const uint32_t vCount = static_cast<uint32_t>(m_workingPositions.size());

    for (uint32_t v = 0; v < vCount; ++v) {
        const Vec3& p = m_workingPositions[v];
        // Grab uses the grab origin (first sample) as the weight center so the
        // affected vertex set stays fixed as the cursor moves.
        float w;
        if (stroke.kind == BrushKind::Grab) {
            BrushSample originSample = sample;
            originSample.position = stroke.grabOrigin;
            w = radialWeight(p, originSample, radius, f);
        } else {
            w = radialWeight(p, sample, radius, f);
        }
        if (w <= 0.f) {
            continue;
        }
        captureBaselineIfNew(stroke, v);

        Vec3 newPos = p;
        switch (stroke.kind) {
            case BrushKind::Draw:    newPos = applyDraw(stroke, v, sample, w);    break;
            case BrushKind::Smooth:  newPos = applySmooth(stroke, v, w);          break;
            case BrushKind::Inflate: newPos = applyInflate(stroke, v, sample, w); break;
            case BrushKind::Flatten: newPos = applyFlatten(stroke, v, sample, w); break;
            case BrushKind::Pinch:   newPos = applyPinch(stroke, v, sample, w);   break;
            case BrushKind::Crease:  newPos = applyCrease(stroke, v, w);          break;
            case BrushKind::Layer:   newPos = applyLayer(stroke, v, w);           break;
            case BrushKind::Grab:    newPos = applyGrab(stroke, v, sample, w);    break;
        }

        // Optional per-vertex displacement cap (Layer-style behaviour).
        if (stroke.params.maxPerVertexDisplacement > 0.f) {
            // Look up baseline (we just captured it if missing).
            const auto it = std::lower_bound(stroke.baseline.begin(), stroke.baseline.end(), v,
                [](const std::pair<uint32_t, Vec3>& a, uint32_t b) { return a.first < b; });
            if (it != stroke.baseline.end() && it->first == v) {
                Vec3 acc = newPos - it->second;
                const float al = std::sqrt(lengthSq(acc));
                if (al > stroke.params.maxPerVertexDisplacement) {
                    const float k = stroke.params.maxPerVertexDisplacement / al;
                    acc = acc * k;
                    newPos = it->second + acc;
                }
            }
        }

        m_workingPositions[v] = newPos;
    }
    return true;
}

StrokeDelta SculptSession::endStroke(StrokeId id)
{
    StrokeDelta delta;
    if (!m_openStroke.has_value() || m_openStroke->id != id) {
        return delta;
    }
    OpenStroke stroke = std::move(*m_openStroke);
    m_openStroke.reset();

    delta.id   = stroke.id;
    delta.kind = stroke.kind;
    delta.vertexDeltas.reserve(stroke.baseline.size());
    for (const auto& [vIdx, basePos] : stroke.baseline) {
        if (vIdx >= m_workingPositions.size()) {
            continue;
        }
        const Vec3 d = m_workingPositions[vIdx] - basePos;
        // Always record; zero-deltas are valid (no-op on undo/redo).
        delta.vertexDeltas.emplace_back(vIdx, d);
    }
    writeBackPositions();
    ++m_stats.strokesCommitted;
    m_stats.verticesTouched += delta.touchedVertexCount();
    return delta;
}

bool SculptSession::undoStroke(const StrokeDelta& delta)
{
    if (!delta.isValid()) {
        return false;
    }
    for (const auto& [vIdx, d] : delta.vertexDeltas) {
        if (vIdx >= m_workingPositions.size()) {
            return false;
        }
    }
    for (const auto& [vIdx, d] : delta.vertexDeltas) {
        m_workingPositions[vIdx] = m_workingPositions[vIdx] - d;
    }
    writeBackPositions();
    ++m_stats.undoApplied;
    return true;
}

bool SculptSession::redoStroke(const StrokeDelta& delta)
{
    if (!delta.isValid()) {
        return false;
    }
    for (const auto& [vIdx, d] : delta.vertexDeltas) {
        if (vIdx >= m_workingPositions.size()) {
            return false;
        }
    }
    for (const auto& [vIdx, d] : delta.vertexDeltas) {
        m_workingPositions[vIdx] = m_workingPositions[vIdx] + d;
    }
    writeBackPositions();
    ++m_stats.redoApplied;
    return true;
}

// ── Brush kernels ────────────────────────────────────────────────────────────

Vec3 SculptSession::applyDraw(const OpenStroke& stroke, uint32_t vIdx,
                              const BrushSample& sample, float weight) const
{
    Vec3 dir;
    if (stroke.params.useVertexNormal && vIdx < m_workingNormals.size()) {
        dir = safeNormalize(m_workingNormals[vIdx], stroke.params.direction);
    } else {
        dir = stroke.params.direction;  // already normalized in beginStroke
    }
    const float amount = stroke.params.strength * weight * stroke.params.radius;
    (void)sample;
    return m_workingPositions[vIdx] + dir * amount;
}

Vec3 SculptSession::applySmooth(const OpenStroke& stroke, uint32_t vIdx, float weight) const
{
    const auto& nbrs = m_adjacency[vIdx];
    if (nbrs.empty()) {
        return m_workingPositions[vIdx];
    }
    Vec3 avg{0.f, 0.f, 0.f};
    for (uint32_t n : nbrs) {
        avg = avg + m_workingPositions[n];
    }
    avg = avg * (1.f / static_cast<float>(nbrs.size()));
    const float t = std::clamp(stroke.params.strength * weight, 0.f, 1.f);
    const Vec3& p = m_workingPositions[vIdx];
    return p + (avg - p) * t;
}

Vec3 SculptSession::applyInflate(const OpenStroke& stroke, uint32_t vIdx,
                                 const BrushSample& sample, float weight) const
{
    if (vIdx >= m_workingNormals.size()) {
        return m_workingPositions[vIdx];
    }
    const Vec3 n = safeNormalize(m_workingNormals[vIdx], {0.f, 1.f, 0.f});
    const float amount = stroke.params.strength * weight * stroke.params.radius;
    (void)sample;
    return m_workingPositions[vIdx] + n * amount;
}

Vec3 SculptSession::applyFlatten(const OpenStroke& stroke, uint32_t vIdx,
                                 const BrushSample& sample, float weight) const
{
    // Project the vertex onto the plane defined by sample.position and stroke direction.
    const Vec3& p = m_workingPositions[vIdx];
    const Vec3& planeN = stroke.params.direction;  // unit if !useVertexNormal
    Vec3 n = planeN;
    if (stroke.params.useVertexNormal && vIdx < m_workingNormals.size()) {
        n = safeNormalize(m_workingNormals[vIdx], planeN);
    }
    const Vec3 toPlane = sample.position - p;
    const float signedDist = n.x * toPlane.x + n.y * toPlane.y + n.z * toPlane.z;
    const Vec3 onto = n * signedDist;
    const float t = std::clamp(stroke.params.strength * weight, 0.f, 1.f);
    return p + onto * t;
}

Vec3 SculptSession::applyPinch(const OpenStroke& stroke, uint32_t vIdx,
                               const BrushSample& sample, float weight) const
{
    const Vec3& p = m_workingPositions[vIdx];
    const Vec3 toCenter = sample.position - p;
    const float t = std::clamp(stroke.params.strength * weight, 0.f, 1.f);
    return p + toCenter * t;
}

// ── Slice 2 brush kernels ─────────────────────────────────────────────────────

Vec3 SculptSession::applyCrease(const OpenStroke& stroke, uint32_t vIdx,
                                float weight) const
{
    // Pull the vertex toward the average of its 1-ring edge midpoints.
    // This tightens the surface around edges, sharpening creases.
    const auto& nbrs = m_adjacency[vIdx];
    if (nbrs.empty()) {
        return m_workingPositions[vIdx];
    }
    const Vec3& p = m_workingPositions[vIdx];
    Vec3 midpointSum{0.f, 0.f, 0.f};
    for (uint32_t n : nbrs) {
        const Vec3& q = m_workingPositions[n];
        midpointSum = midpointSum + ((p + q) * 0.5f);
    }
    const Vec3 avg = midpointSum * (1.f / static_cast<float>(nbrs.size()));
    const float t = std::clamp(stroke.params.strength * weight, 0.f, 1.f);
    return p + (avg - p) * t;
}

Vec3 SculptSession::applyLayer(const OpenStroke& stroke, uint32_t vIdx,
                               float weight) const
{
    // Displace along the vertex normal by strength * weight * radius, but cap
    // the accumulated per-vertex displacement within this stroke.
    if (vIdx >= m_workingNormals.size()) {
        return m_workingPositions[vIdx];
    }
    const Vec3& p = m_workingPositions[vIdx];
    const Vec3 n = safeNormalize(m_workingNormals[vIdx], {0.f, 1.f, 0.f});
    const float stepAmount = stroke.params.strength * weight * stroke.params.radius;

    const float cap = stroke.params.maxPerVertexDisplacement > 0.f
                      ? stroke.params.maxPerVertexDisplacement
                      : std::abs(stepAmount); // uncapped: let the step through fully

    // Compute accumulated displacement from baseline.
    Vec3 accumulated{0.f, 0.f, 0.f};
    const auto it = std::lower_bound(stroke.baseline.begin(), stroke.baseline.end(), vIdx,
        [](const std::pair<uint32_t, Vec3>& a, uint32_t b) { return a.first < b; });
    if (it != stroke.baseline.end() && it->first == vIdx) {
        accumulated = p - it->second;
    }

    const float accLen = std::sqrt(lengthSq(accumulated));
    if (accLen >= cap) {
        return p; // already at or past cap — no further displacement
    }

    const float remaining = cap - accLen;
    const float apply = std::min(std::abs(stepAmount), remaining)
                        * (stepAmount >= 0.f ? 1.f : -1.f);
    return p + n * apply;
}

Vec3 SculptSession::applyGrab(const OpenStroke& stroke, uint32_t vIdx,
                              const BrushSample& sample, float weight) const
{
    // Move all touched vertices by the same rigid delta: (currentSamplePos - grabOrigin),
    // attenuated by the falloff weight. The grab origin is set on the first sample.
    const Vec3 delta = sample.position - stroke.grabOrigin;
    return m_workingPositions[vIdx] + delta * weight;
}

} // namespace nexus::sculpt
