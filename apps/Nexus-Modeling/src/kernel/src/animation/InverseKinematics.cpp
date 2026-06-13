#include <nexus/animation/InverseKinematics.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::animation {

using Vec3 = nexus::render::Vec3;
using Vec4 = nexus::render::Vec4;
using Mat4 = nexus::render::Mat4;

namespace {

Vec4 quatMul(const Vec4& a, const Vec4& b) noexcept {
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}

Vec4 quatConjugate(const Vec4& q) noexcept {
    return {-q.x, -q.y, -q.z, q.w};
}

Vec3 quatRotateVec(const Vec4& q, const Vec3& v) noexcept {
    Vec3 u{q.x, q.y, q.z};
    Vec3 t = u.cross(v) * 2.0f;
    return v + t * q.w + u.cross(t);
}

Vec4 axisAngleToQuat(const Vec3& axis, float angle) noexcept {
    float halfAngle = angle * 0.5f;
    float s = std::sin(halfAngle);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(halfAngle)};
}

Vec4 normaliseQuat(const Vec4& q) noexcept {
    float lenSq = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (lenSq < 1e-12f) return {0.f, 0.f, 0.f, 1.f};
    float invLen = 1.0f / std::sqrt(lenSq);
    return {q.x*invLen, q.y*invLen, q.z*invLen, q.w*invLen};
}

Vec3 mat4GetTranslation(const Mat4& m) noexcept {
    return {m.m[0][3], m.m[1][3], m.m[2][3]};
}

Vec4 mat4ExtractRotation(const Mat4& m) noexcept {
    float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
    Vec4 q{};
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.x = (m.m[2][1] - m.m[1][2]) / s;
        q.y = (m.m[0][2] - m.m[2][0]) / s;
        q.z = (m.m[1][0] - m.m[0][1]) / s;
        q.w = 0.25f * s;
    } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f;
        q.x = 0.25f * s;
        q.y = (m.m[0][1] + m.m[1][0]) / s;
        q.z = (m.m[0][2] + m.m[2][0]) / s;
        q.w = (m.m[2][1] - m.m[1][2]) / s;
    } else if (m.m[1][1] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f;
        q.x = (m.m[0][1] + m.m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[1][2] + m.m[2][1]) / s;
        q.w = (m.m[0][2] - m.m[2][0]) / s;
    } else {
        float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f;
        q.x = (m.m[0][2] + m.m[2][0]) / s;
        q.y = (m.m[1][2] + m.m[2][1]) / s;
        q.z = 0.25f * s;
        q.w = (m.m[0][1] - m.m[1][0]) / s;
    }
    return normaliseQuat(q);
}

} // namespace

IKResult InverseKinematics::solveCCD(
    Skeleton& skeleton,
    Pose& pose,
    uint32_t endEffectorBone,
    const Vec3& target,
    const IKSettings& settings) {

    IKResult result;
    if (endEffectorBone >= skeleton.boneCount()) {
        result.error = "End effector bone index out of range";
        return result;
    }
    if (settings.maxIterations == 0) {
        result.error = "maxIterations must be > 0";
        return result;
    }

    if (pose.modelMatrices().size() != skeleton.boneCount()) {
        pose.resize(skeleton.boneCount());
    }

    int32_t currBone = skeleton.parentIndex(endEffectorBone);
    if (currBone < 0) {
        result.error = "End effector has no parent bone to rotate";
        return result;
    }

    for (uint32_t iter = 0; iter < settings.maxIterations; ++iter) {
        pose.computeModelMatrices(skeleton);
        const auto& modelMats = pose.modelMatrices();

        Vec3 eePos = mat4GetTranslation(modelMats[endEffectorBone]);
        float dist = (target - eePos).length();
        if (dist < settings.tolerance) {
            result.ok = true;
            result.iterations = iter;
            result.residual = dist;
            return result;
        }

        int32_t bone = skeleton.parentIndex(endEffectorBone);
        while (bone >= 0) {
            size_t bIdx = static_cast<size_t>(bone);
            Vec3 bonePos = mat4GetTranslation(modelMats[bIdx]);
            Vec3 toEndEffector = eePos - bonePos;
            Vec3 toTarget = target - bonePos;

            float eeLen = toEndEffector.length();
            float tLen = toTarget.length();
            if (eeLen < 1e-8f || tLen < 1e-8f) {
                bone = skeleton.parentIndex(bIdx);
                continue;
            }

            Vec3 dirEE = toEndEffector * (1.0f / eeLen);
            Vec3 dirTarget = toTarget * (1.0f / tLen);
            float d = dirEE.dot(dirTarget);
            d = std::clamp(d, -1.0f, 1.0f);
            float angle = std::acos(d) * settings.damping;

            if (angle < 1e-8f) {
                bone = skeleton.parentIndex(bIdx);
                continue;
            }

            // Joint angle limit: clamp rotation magnitude per bone
            for (const auto& [limBone, limRad] : settings.jointLimits) {
                if (static_cast<uint32_t>(bIdx) == limBone && angle > limRad) {
                    angle = limRad;
                    break;
                }
            }

            Vec3 worldAxis = dirEE.cross(dirTarget);
            float axisLen = worldAxis.length();
            if (axisLen < 1e-8f) {
                bone = skeleton.parentIndex(bIdx);
                continue;
            }
            worldAxis = worldAxis * (1.0f / axisLen);

            int32_t parent = skeleton.parentIndex(bIdx);
            Vec4 parentWorldRot{0.f, 0.f, 0.f, 1.f};
            if (parent >= 0) {
                parentWorldRot = mat4ExtractRotation(modelMats[static_cast<size_t>(parent)]);
            }
            Vec4 parentWorldRotInv = quatConjugate(parentWorldRot);

            Vec3 localAxis = quatRotateVec(parentWorldRotInv, worldAxis);
            Vec4 deltaQuat = axisAngleToQuat(localAxis, angle);

            Transform localT = pose.localTransform(bIdx);
            Vec4 newRot = quatMul(deltaQuat, localT.rotation);
            localT.rotation = normaliseQuat(newRot);
            pose.setLocalTransform(bIdx, localT);

            pose.computeModelMatrices(skeleton);
            eePos = mat4GetTranslation(pose.modelMatrices()[endEffectorBone]);

            bone = skeleton.parentIndex(bIdx);
        }

        result.iterations = iter + 1;
        result.residual = (target - eePos).length();
    }

    result.ok = (result.residual < settings.tolerance);
    return result;
}

} // namespace nexus::animation
