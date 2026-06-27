#include <nexus/geometry/MeshSkinning.h>

#include <cmath>

namespace {

[[nodiscard]] inline nexus::render::Vec4 quatMul(const nexus::render::Vec4& a,
                                                  const nexus::render::Vec4& b) noexcept
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

[[nodiscard]] inline nexus::render::Vec4 quatConj(const nexus::render::Vec4& q) noexcept
{
    return {-q.x, -q.y, -q.z, q.w};
}

[[nodiscard]] inline float quatDot(const nexus::render::Vec4& a, const nexus::render::Vec4& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

[[nodiscard]] inline nexus::render::Vec3 quatRotate(const nexus::render::Vec4& q,
                                                     const nexus::render::Vec3& v) noexcept
{
    const nexus::render::Vec4 vq = {v.x, v.y, v.z, 0.f};
    const nexus::render::Vec4 r = quatMul(quatMul(q, vq), quatConj(q));
    return {r.x, r.y, r.z};
}

} // namespace

namespace nexus::geometry {

DualQuat DualQuat::fromTransform(const nexus::render::Mat4& mat) noexcept
{
    const auto& m = mat.m;
    const float trace = m[0][0] + m[1][1] + m[2][2];

    nexus::render::Vec4 q{};
    if (trace > 0.f) {
        float s = std::sqrt(trace + 1.f) * 2.f;
        q.w = s * 0.25f;
        q.x = (m[2][1] - m[1][2]) / s;
        q.y = (m[0][2] - m[2][0]) / s;
        q.z = (m[1][0] - m[0][1]) / s;
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        float s = std::sqrt(1.f + m[0][0] - m[1][1] - m[2][2]) * 2.f;
        q.w = (m[2][1] - m[1][2]) / s;
        q.x = s * 0.25f;
        q.y = (m[0][1] + m[1][0]) / s;
        q.z = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        float s = std::sqrt(1.f + m[1][1] - m[0][0] - m[2][2]) * 2.f;
        q.w = (m[0][2] - m[2][0]) / s;
        q.x = (m[0][1] + m[1][0]) / s;
        q.y = s * 0.25f;
        q.z = (m[1][2] + m[2][1]) / s;
    } else {
        float s = std::sqrt(1.f + m[2][2] - m[0][0] - m[1][1]) * 2.f;
        q.w = (m[1][0] - m[0][1]) / s;
        q.x = (m[0][2] + m[2][0]) / s;
        q.y = (m[1][2] + m[2][1]) / s;
        q.z = s * 0.25f;
    }

    const nexus::render::Vec3 t{m[0][3], m[1][3], m[2][3]};
    const float dotTQ = -(t.x * q.x + t.y * q.y + t.z * q.z) * 0.5f;
    const nexus::render::Vec4 d{
        (t.x * q.w + t.y * q.z - t.z * q.y) * 0.5f,
        (-t.x * q.z + t.y * q.w + t.z * q.x) * 0.5f,
        (t.x * q.y - t.y * q.x + t.z * q.w) * 0.5f,
        dotTQ,
    };

    return {q, d};
}

Mesh MeshSkinning::deform(const Mesh& mesh,
                           const std::vector<nexus::render::Mat4>& boneTransforms,
                           const SkinningOptions& options)
{
    if (options.method == SkinningMethod::DQS) {
        return deformDQS(mesh, boneTransforms);
    }

    if (!mesh.isValid()) {
        return Mesh{};
    }

    if (!mesh.attributes().hasSkinning()) {
        return Mesh{};
    }

    const auto& positions = mesh.attributes().positions();
    const auto& jointIndices = mesh.attributes().jointIndices();
    const auto& jointWeights = mesh.attributes().jointWeights();

    Mesh result;
    std::vector<nexus::render::Vec3> newPositions;
    newPositions.reserve(positions.size());

    for (size_t v = 0; v < positions.size(); ++v) {
        nexus::render::Vec3 pos = positions[v];
        nexus::render::Vec3 blended = {0.f, 0.f, 0.f};

        for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
            float weight = jointWeights[v][b];
            if (weight <= 0.f) {
                continue;
            }

            uint32_t boneIdx = jointIndices[v][b];
            if (boneIdx >= boneTransforms.size()) {
                return Mesh{};
            }

            const auto& mat = boneTransforms[boneIdx];
            nexus::render::Vec4 transformed = mat * nexus::render::Vec4{pos.x, pos.y, pos.z, 1.f};
            blended.x += transformed.x * weight;
            blended.y += transformed.y * weight;
            blended.z += transformed.z * weight;
        }

        newPositions.push_back(blended);
    }

    result.attributes().setPositions(std::move(newPositions));

    if (mesh.attributes().hasNormals()) {
        const auto& oldNormals = mesh.attributes().normals();
        std::vector<nexus::render::Vec3> newNormals;
        newNormals.reserve(oldNormals.size());

        for (size_t v = 0; v < oldNormals.size(); ++v) {
            nexus::render::Vec3 normal = oldNormals[v];
            nexus::render::Vec3 blended = {0.f, 0.f, 0.f};

            for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
                float weight = jointWeights[v][b];
                if (weight <= 0.f) continue;

                uint32_t boneIdx = jointIndices[v][b];
                if (boneIdx >= boneTransforms.size()) return Mesh{};

                const auto& mat = boneTransforms[boneIdx];
                // Transform normal via inverse transpose (upper-left 3x3).
                float nx = normal.x * mat.m[0][0] + normal.y * mat.m[1][0] + normal.z * mat.m[2][0];
                float ny = normal.x * mat.m[0][1] + normal.y * mat.m[1][1] + normal.z * mat.m[2][1];
                float nz = normal.x * mat.m[0][2] + normal.y * mat.m[1][2] + normal.z * mat.m[2][2];
                blended.x += nx * weight;
                blended.y += ny * weight;
                blended.z += nz * weight;
            }

            float lenSq = blended.x * blended.x + blended.y * blended.y + blended.z * blended.z;
            if (lenSq > 1e-12f) {
                float inv = 1.f / std::sqrt(lenSq);
                blended.x *= inv;
                blended.y *= inv;
                blended.z *= inv;
            }
            newNormals.push_back(blended);
        }
        result.attributes().setNormals(std::move(newNormals));
    }
    if (mesh.attributes().hasUVs()) {
        result.attributes().setUVs(mesh.attributes().uvs());
    }
    if (mesh.attributes().hasTangents()) {
        result.attributes().setTangents(mesh.attributes().tangents());
    }
    result.attributes().setSkinning(
        std::vector<JointIndex4>(jointIndices),
        std::vector<JointWeight4>(jointWeights));

    for (size_t f = 0; f < mesh.topology().faceCount(); ++f) {
        result.topology().addFace(Face{mesh.topology().face(f).indices});
    }

    return result;
}

Mesh MeshSkinning::deformDQS(const Mesh& mesh,
                              const std::vector<nexus::render::Mat4>& boneTransforms)
{
    if (!mesh.isValid()) {
        return Mesh{};
    }

    if (!mesh.attributes().hasSkinning()) {
        return Mesh{};
    }

    const auto& positions = mesh.attributes().positions();
    const auto& jointIndices = mesh.attributes().jointIndices();
    const auto& jointWeights = mesh.attributes().jointWeights();

    std::vector<DualQuat> boneDQs;
    boneDQs.reserve(boneTransforms.size());
    for (const auto& mat : boneTransforms) {
        boneDQs.push_back(DualQuat::fromTransform(mat));
    }

    Mesh result;
    std::vector<nexus::render::Vec3> newPositions;
    newPositions.reserve(positions.size());

    for (size_t v = 0; v < positions.size(); ++v) {
        const nexus::render::Vec3& pos = positions[v];

        const nexus::render::Vec4* refReal = nullptr;
        for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
            float weight = jointWeights[v][b];
            if (weight <= 0.f) continue;
            uint32_t boneIdx = jointIndices[v][b];
            if (boneIdx >= boneTransforms.size()) return Mesh{};
            refReal = &boneDQs[boneIdx].real;
            break;
        }

        if (!refReal) {
            newPositions.push_back(pos);
            continue;
        }

        nexus::render::Vec4 blendedReal{0.f, 0.f, 0.f, 0.f};
        nexus::render::Vec4 blendedDual{0.f, 0.f, 0.f, 0.f};

        for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
            float weight = jointWeights[v][b];
            if (weight <= 0.f) continue;
            uint32_t boneIdx = jointIndices[v][b];
            if (boneIdx >= boneDQs.size()) continue;

            const auto& dq = boneDQs[boneIdx];
            if (quatDot(*refReal, dq.real) < 0.f) weight = -weight;

            blendedReal.x += dq.real.x * weight;
            blendedReal.y += dq.real.y * weight;
            blendedReal.z += dq.real.z * weight;
            blendedReal.w += dq.real.w * weight;
            blendedDual.x += dq.dual.x * weight;
            blendedDual.y += dq.dual.y * weight;
            blendedDual.z += dq.dual.z * weight;
            blendedDual.w += dq.dual.w * weight;
        }

        float lenSq = blendedReal.x * blendedReal.x +
                      blendedReal.y * blendedReal.y +
                      blendedReal.z * blendedReal.z +
                      blendedReal.w * blendedReal.w;
        if (lenSq < 1e-12f) continue;
        float invLen = 1.f / std::sqrt(lenSq);
        blendedReal.x *= invLen;
        blendedReal.y *= invLen;
        blendedReal.z *= invLen;
        blendedReal.w *= invLen;
        blendedDual.x *= invLen;
        blendedDual.y *= invLen;
        blendedDual.z *= invLen;
        blendedDual.w *= invLen;

        const nexus::render::Vec3 rotPos = quatRotate(blendedReal, pos);

        const nexus::render::Vec4 qConj = quatConj(blendedReal);
        const nexus::render::Vec4 tQuat = quatMul(blendedDual, qConj);

        newPositions.push_back({
            rotPos.x + tQuat.x * 2.f,
            rotPos.y + tQuat.y * 2.f,
            rotPos.z + tQuat.z * 2.f,
        });
    }

    result.attributes().setPositions(std::move(newPositions));

    if (mesh.attributes().hasNormals()) {
        const auto& oldNormals = mesh.attributes().normals();
        std::vector<nexus::render::Vec3> newNormals;
        newNormals.reserve(oldNormals.size());

        for (size_t v = 0; v < oldNormals.size(); ++v) {
            const nexus::render::Vec3& normal = oldNormals[v];

            const nexus::render::Vec4* refReal = nullptr;
            for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
                float weight = jointWeights[v][b];
                if (weight <= 0.f) continue;
                uint32_t boneIdx = jointIndices[v][b];
                if (boneIdx >= boneTransforms.size()) return Mesh{};
                refReal = &boneDQs[boneIdx].real;
                break;
            }

            if (!refReal) {
                newNormals.push_back(normal);
                continue;
            }

            nexus::render::Vec4 blendedQ{0.f, 0.f, 0.f, 0.f};
            for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
                float weight = jointWeights[v][b];
                if (weight <= 0.f) continue;
                uint32_t boneIdx = jointIndices[v][b];

                const auto& dq = boneDQs[boneIdx];
                if (quatDot(*refReal, dq.real) < 0.f) weight = -weight;

                blendedQ.x += dq.real.x * weight;
                blendedQ.y += dq.real.y * weight;
                blendedQ.z += dq.real.z * weight;
                blendedQ.w += dq.real.w * weight;
            }

            float lenSq = blendedQ.x * blendedQ.x + blendedQ.y * blendedQ.y +
                          blendedQ.z * blendedQ.z + blendedQ.w * blendedQ.w;
            if (lenSq > 1e-12f) {
                float inv = 1.f / std::sqrt(lenSq);
                blendedQ.x *= inv;
                blendedQ.y *= inv;
                blendedQ.z *= inv;
                blendedQ.w *= inv;
            }

            newNormals.push_back(quatRotate(blendedQ, normal));
        }
        result.attributes().setNormals(std::move(newNormals));
    }

    if (mesh.attributes().hasUVs()) {
        result.attributes().setUVs(mesh.attributes().uvs());
    }
    if (mesh.attributes().hasTangents()) {
        const auto& oldTangents = mesh.attributes().tangents();
        std::vector<nexus::geometry::Vec4> newTangents;
        newTangents.reserve(oldTangents.size());

        for (size_t v = 0; v < oldTangents.size(); ++v) {
            const nexus::geometry::Vec4& tangent = oldTangents[v];

            const nexus::render::Vec4* refReal = nullptr;
            for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
                float weight = jointWeights[v][b];
                if (weight <= 0.f) continue;
                uint32_t boneIdx = jointIndices[v][b];
                if (boneIdx >= boneTransforms.size()) {
                    newTangents.push_back(tangent);
                    goto nextTangent;
                }
                refReal = &boneDQs[boneIdx].real;
                break;
            }

            if (!refReal) {
                newTangents.push_back(tangent);
                continue;
            }

            {
                nexus::render::Vec4 blendedQ{0.f, 0.f, 0.f, 0.f};
                for (size_t b = 0; b < kMaxSkinInfluencesPerVertex; ++b) {
                    float weight = jointWeights[v][b];
                    if (weight <= 0.f) continue;
                    uint32_t boneIdx = jointIndices[v][b];

                    const auto& dq = boneDQs[boneIdx];
                    if (quatDot(*refReal, dq.real) < 0.f) weight = -weight;

                    blendedQ.x += dq.real.x * weight;
                    blendedQ.y += dq.real.y * weight;
                    blendedQ.z += dq.real.z * weight;
                    blendedQ.w += dq.real.w * weight;
                }

                float lenSq = blendedQ.x * blendedQ.x + blendedQ.y * blendedQ.y +
                              blendedQ.z * blendedQ.z + blendedQ.w * blendedQ.w;
                if (lenSq > 1e-12f) {
                    float inv = 1.f / std::sqrt(lenSq);
                    blendedQ.x *= inv;
                    blendedQ.y *= inv;
                    blendedQ.z *= inv;
                    blendedQ.w *= inv;
                }

                const nexus::render::Vec3 tDir{tangent.x, tangent.y, tangent.z};
                const nexus::render::Vec3 rotTangent = quatRotate(blendedQ, tDir);
                newTangents.push_back({rotTangent.x, rotTangent.y, rotTangent.z, tangent.w});
            }
            nextTangent:;
        }
        result.attributes().setTangents(std::move(newTangents));
    }
    result.attributes().setSkinning(
        std::vector<JointIndex4>(jointIndices),
        std::vector<JointWeight4>(jointWeights));

    for (size_t f = 0; f < mesh.topology().faceCount(); ++f) {
        result.topology().addFace(Face{mesh.topology().face(f).indices});
    }

    return result;
}

} // namespace nexus::geometry
