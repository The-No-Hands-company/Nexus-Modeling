#include <nexus/sculpt/SculptSession.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace nexus::sculpt {

SculptSession::SculptSession(geometry::Mesh& mesh)
    : m_mesh(mesh)
{
    resync();
}

void SculptSession::resync() {
    auto& attr = m_mesh.attributes();
    m_baselinePositions = attr.positions();
    m_strokePositions = m_baselinePositions;

    if (attr.hasNormals()) {
        m_normals = attr.normals();
    } else {
        m_normals.assign(m_baselinePositions.size(), render::Vec3{0.f, 1.f, 0.f});
    }

    m_mask.assign(m_baselinePositions.size(), 1.f);

    buildAdjacency();
}

void SculptSession::buildAdjacency() {
    size_t vertexCount = m_baselinePositions.size();
    m_adjacency.assign(vertexCount, {});

    const auto& topo = m_mesh.topology();
    std::unordered_set<uint64_t> addedEdges;

    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& face = topo.face(f);
        const auto& idx = face.indices;
        for (size_t a = 0; a < idx.size(); ++a) {
            uint32_t va = idx[a];
            if (va >= vertexCount) continue;
            for (size_t b = a + 1; b < idx.size(); ++b) {
                uint32_t vb = idx[b];
                if (vb >= vertexCount) continue;
                uint64_t key = (static_cast<uint64_t>(std::min(va, vb)) << 32)
                             | static_cast<uint64_t>(std::max(va, vb));
                if (addedEdges.insert(key).second) {
                    m_adjacency[va].push_back(vb);
                    m_adjacency[vb].push_back(va);
                }
            }
        }
    }
}

StrokeId SculptSession::beginStroke(BrushKind kind, const BrushParams& params) {
    if (strokeInProgress()) return kInvalidStrokeId;
    if (params.radius <= 0.f) return kInvalidStrokeId;

    m_activeStrokeId = m_nextStrokeId++;
    m_activeKind = kind;
    m_activeParams = params;
    m_lastSampleSequence = 0;
    m_baselinePositions = m_mesh.attributes().positions();
    m_strokePositions = m_baselinePositions;

    m_grabPositionValid = false;

    ++m_stats.strokesStarted;
    return m_activeStrokeId;
}

bool SculptSession::applySample(StrokeId id, const BrushSample& sample) {
    if (!strokeInProgress()) return false;
    if (id != m_activeStrokeId) return false;
    if (sample.sequence < m_lastSampleSequence) return false;

    m_lastSampleSequence = sample.sequence;

    auto applyWithSymmetry = [&](auto&& kernel) {
        kernel(m_activeParams, sample, m_strokePositions);

        if (hasSymmetry(m_symmetryAxes, SymmetryAxes::X)) {
            BrushSample mirroredX = sample;
            mirroredX.position = mirrorPosition(sample.position, SymmetryAxes::X);
            kernel(m_activeParams, mirroredX, m_strokePositions);
        }
        if (hasSymmetry(m_symmetryAxes, SymmetryAxes::Y)) {
            BrushSample mirroredY = sample;
            mirroredY.position = mirrorPosition(sample.position, SymmetryAxes::Y);
            kernel(m_activeParams, mirroredY, m_strokePositions);
        }
        if (hasSymmetry(m_symmetryAxes, SymmetryAxes::Z)) {
            BrushSample mirroredZ = sample;
            mirroredZ.position = mirrorPosition(sample.position, SymmetryAxes::Z);
            kernel(m_activeParams, mirroredZ, m_strokePositions);
        }
    };

    switch (m_activeKind) {
    case BrushKind::Draw:
        applyWithSymmetry([this](auto&&... args) { applyDrawKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Smooth:
        applyWithSymmetry([this](auto&&... args) { applySmoothKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Inflate:
        applyWithSymmetry([this](auto&&... args) { applyInflateKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Flatten:
        applyWithSymmetry([this](auto&&... args) { applyFlattenKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Pinch:
        applyWithSymmetry([this](auto&&... args) { applyPinchKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Crease:
        applyWithSymmetry([this](auto&&... args) { applyCreaseKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Layer:
        applyWithSymmetry([this](auto&&... args) { applyLayerKernel(std::forward<decltype(args)>(args)...); });
        break;
    case BrushKind::Grab:
        applyWithSymmetry([this](auto&&... args) { applyGrabKernel(std::forward<decltype(args)>(args)...); });
        break;
    }

    ++m_stats.samplesProcessed;
    return true;
}

StrokeDelta SculptSession::endStroke(StrokeId id) {
    if (!strokeInProgress()) return {};
    if (id != m_activeStrokeId) return {};

    StrokeDelta delta;
    delta.id = m_activeStrokeId;
    delta.kind = m_activeKind;

    const auto& baseline = m_baselinePositions;
    for (size_t i = 0; i < m_strokePositions.size(); ++i) {
        render::Vec3 d = m_strokePositions[i] - baseline[i];
        if (d.lengthSq() > 1e-12f) {
            delta.vertexDeltas.emplace_back(static_cast<uint32_t>(i), d);
        }
    }

    m_stats.verticesTouched += static_cast<uint32_t>(delta.vertexDeltas.size());

    auto& pos = m_mesh.attributes();
    pos.setPositions(m_strokePositions);

    m_baselinePositions = pos.positions();

    ++m_stats.strokesCommitted;
    m_activeStrokeId = kInvalidStrokeId;
    m_grabPositionValid = false;
    return delta;
}

bool SculptSession::undoStroke(const StrokeDelta& delta) {
    if (!delta.isValid()) return false;

    auto& attr = m_mesh.attributes();
    std::vector<render::Vec3> positions = attr.positions();
    if (positions.empty()) return false;

    for (const auto& [vi, d] : delta.vertexDeltas) {
        if (vi < positions.size()) {
            positions[vi] = positions[vi] - d;
        }
    }
    attr.setPositions(std::move(positions));
    m_baselinePositions = attr.positions();
    m_strokePositions = m_baselinePositions;

    ++m_stats.undoApplied;
    return true;
}

bool SculptSession::redoStroke(const StrokeDelta& delta) {
    if (!delta.isValid()) return false;

    auto& attr = m_mesh.attributes();
    std::vector<render::Vec3> positions = attr.positions();
    if (positions.empty()) return false;

    for (const auto& [vi, d] : delta.vertexDeltas) {
        if (vi < positions.size()) {
            positions[vi] = positions[vi] + d;
        }
    }
    attr.setPositions(std::move(positions));
    m_baselinePositions = attr.positions();
    m_strokePositions = m_baselinePositions;

    ++m_stats.redoApplied;
    return true;
}

void SculptSession::setMask(float value) {
    std::fill(m_mask.begin(), m_mask.end(), std::clamp(value, 0.f, 1.f));
}

void SculptSession::setMaskVertex(uint32_t vertexIndex, float value) {
    if (vertexIndex < m_mask.size()) {
        m_mask[vertexIndex] = std::clamp(value, 0.f, 1.f);
    }
}

void SculptSession::clearMask() {
    setMask(1.f);
}

void SculptSession::invertMask() {
    for (auto& m : m_mask) m = 1.f - m;
}

void SculptSession::floodFillMask(uint32_t seedVertex) {
    if (seedVertex >= m_mask.size()) return;

    std::vector<bool> visited(m_mask.size(), false);
    std::vector<uint32_t> stack;
    stack.push_back(seedVertex);
    visited[seedVertex] = true;

    while (!stack.empty()) {
        uint32_t v = stack.back();
        stack.pop_back();
        m_mask[v] = 1.f;
        for (uint32_t nbr : m_adjacency[v]) {
            if (!visited[nbr]) {
                visited[nbr] = true;
                stack.push_back(nbr);
            }
        }
    }
}

float SculptSession::maskAt(uint32_t vertexIndex) const {
    if (vertexIndex < m_mask.size()) return m_mask[vertexIndex];
    return 1.f;
}

void SculptSession::setSymmetry(SymmetryAxes axes) {
    m_symmetryAxes = axes;
}

render::Vec3 SculptSession::mirrorPosition(const render::Vec3& pos, SymmetryAxes axis) const noexcept {
    render::Vec3 result = pos;
    if (hasSymmetry(axis, SymmetryAxes::X)) result.x = -result.x;
    if (hasSymmetry(axis, SymmetryAxes::Y)) result.y = -result.y;
    if (hasSymmetry(axis, SymmetryAxes::Z)) result.z = -result.z;
    return result;
}

void SculptSession::applyDrawKernel(const BrushParams& params, const BrushSample& sample,
                                     std::vector<render::Vec3>& targetPositions) {
    render::Vec3 dir = params.direction;
    float len = dir.length();
    if (len < 1e-8f) dir = {0.f, 1.f, 0.f};
    else dir = dir * (1.f / len);

    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        render::Vec3 normal = dir;
        if (params.useVertexNormal && i < m_normals.size()) {
            normal = m_normals[i];
            float nl = normal.length();
            if (nl > 1e-8f) normal = normal * (1.f / nl);
            else normal = dir;
        }
        targetPositions[i] = targetPositions[i] + normal * weight;
    }
}

void SculptSession::applySmoothKernel(const BrushParams& params, const BrushSample& sample,
                                       std::vector<render::Vec3>& targetPositions) {
    const size_t n = targetPositions.size();

    std::vector<render::Vec3> averages(n);
    for (size_t i = 0; i < n; ++i) {
        const auto& nbrs = m_adjacency[i];
        if (nbrs.empty()) {
            averages[i] = targetPositions[i];
            continue;
        }
        render::Vec3 sum = {0.f, 0.f, 0.f};
        for (uint32_t nbr : nbrs) {
            sum += targetPositions[nbr];
        }
        averages[i] = sum * (1.f / static_cast<float>(nbrs.size()));
    }

    for (size_t i = 0; i < n; ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        render::Vec3 offset = averages[i] - targetPositions[i];
        targetPositions[i] = targetPositions[i] + offset * weight;
    }
}

void SculptSession::applyInflateKernel(const BrushParams& params, const BrushSample& sample,
                                        std::vector<render::Vec3>& targetPositions) {
    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        render::Vec3 normal = (i < m_normals.size()) ? m_normals[i] : render::Vec3{0.f, 1.f, 0.f};
        float nl = normal.length();
        if (nl > 1e-8f) normal = normal * (1.f / nl);
        else normal = {0.f, 1.f, 0.f};

        targetPositions[i] = targetPositions[i] + normal * weight;
    }
}

void SculptSession::applyFlattenKernel(const BrushParams& params, const BrushSample& sample,
                                        std::vector<render::Vec3>& targetPositions) {
    render::Vec3 dir = params.direction;
    float len = dir.length();
    if (len < 1e-8f) dir = {0.f, 1.f, 0.f};
    else dir = dir * (1.f / len);

    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        float height = (targetPositions[i] - sample.position).dot(dir);
        targetPositions[i] = targetPositions[i] - dir * (height * weight);
    }
}

void SculptSession::applyPinchKernel(const BrushParams& params, const BrushSample& sample,
                                      std::vector<render::Vec3>& targetPositions) {
    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        render::Vec3 toCenter = sample.position - targetPositions[i];
        float toCenterLen = toCenter.length();
        if (toCenterLen > 1e-8f) {
            targetPositions[i] = targetPositions[i] + toCenter * (weight / toCenterLen);
        }
    }
}

void SculptSession::applyCreaseKernel(const BrushParams& params, const BrushSample& sample,
                                       std::vector<render::Vec3>& targetPositions) {
    render::Vec3 dir = params.direction;
    float len = dir.length();
    if (len < 1e-8f) dir = {0.f, 1.f, 0.f};
    else dir = dir * (1.f / len);

    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        render::Vec3 normal = (i < m_normals.size()) ? m_normals[i] : dir;
        float nl = normal.length();
        if (nl > 1e-8f) normal = normal * (1.f / nl);
        else normal = dir;

        float ndot = normal.dot(dir);
        float sign = (ndot >= 0.f) ? 1.f : -1.f;
        targetPositions[i] = targetPositions[i] + normal * (weight * sign);
    }
}

void SculptSession::applyLayerKernel(const BrushParams& params, const BrushSample& sample,
                                      std::vector<render::Vec3>& targetPositions) {
    render::Vec3 dir = params.direction;
    float len = dir.length();
    if (len < 1e-8f) dir = {0.f, 1.f, 0.f};
    else dir = dir * (1.f / len);

    float maxDisp = (params.maxPerVertexDisplacement > 0.f) ? params.maxPerVertexDisplacement
                                                            : std::numeric_limits<float>::max();

    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        float currentDisp = (targetPositions[i] - m_baselinePositions[i]).length();
        if (currentDisp >= maxDisp) continue;

        render::Vec3 normal = dir;
        if (params.useVertexNormal && i < m_normals.size()) {
            normal = m_normals[i];
            float nl = normal.length();
            if (nl > 1e-8f) normal = normal * (1.f / nl);
            else normal = dir;
        }

        float remaining = maxDisp - currentDisp;
        float cappedWeight = std::min(weight, remaining);
        targetPositions[i] = targetPositions[i] + normal * cappedWeight;
    }
}

void SculptSession::applyGrabKernel(const BrushParams& params, const BrushSample& sample,
                                     std::vector<render::Vec3>& targetPositions) {
    if (!m_grabPositionValid) {
        m_lastGrabPosition = sample.position;
        m_grabPositionValid = true;
        return;
    }

    render::Vec3 delta = sample.position - m_lastGrabPosition;
    m_lastGrabPosition = sample.position;

    float deltaLenSq = delta.lengthSq();
    if (deltaLenSq < 1e-12f) return;

    for (size_t i = 0; i < targetPositions.size(); ++i) {
        float maskVal = (i < m_mask.size()) ? m_mask[i] : 1.f;
        if (maskVal <= 0.f) continue;

        float dist = (targetPositions[i] - sample.position).length();
        if (dist > params.radius) continue;

        float t = dist / params.radius;
        float falloff = evaluateFalloff(params.falloff, t);
        float weight = falloff * params.strength * sample.pressure * maskVal;
        if (weight <= 0.f) continue;

        targetPositions[i] = targetPositions[i] + delta * weight;
    }
}

} // namespace nexus::sculpt
