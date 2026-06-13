#include <nexus/animation/AnimationBlend.h>

#include <algorithm>
#include <cmath>

namespace nexus::animation {

using Vec3 = nexus::render::Vec3;
using Vec4 = nexus::render::Vec4;

static Vec4 slerpQuat(const Vec4& aRaw, const Vec4& bRaw, float t) {
    Vec4 qa = aRaw;
    Vec4 qb = bRaw;

    float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;

    if (dot < 0.f) {
        qb = Vec4{-qb.x, -qb.y, -qb.z, -qb.w};
        dot = -dot;
    }

    static constexpr float kEpsilon = 1.0f - 1e-6f;
    if (dot > kEpsilon) {
        Vec4 r;
        r.x = qa.x + t * (qb.x - qa.x);
        r.y = qa.y + t * (qb.y - qa.y);
        r.z = qa.z + t * (qb.z - qa.z);
        r.w = qa.w + t * (qb.w - qa.w);
        float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
        if (len > 1e-10f) {
            float inv = 1.0f / len;
            r.x *= inv; r.y *= inv; r.z *= inv; r.w *= inv;
        }
        return r;
    }

    float theta0 = std::acos(dot);
    float theta = theta0 * t;
    float sinTheta = std::sin(theta);
    float sinTheta0 = std::sin(theta0);

    float scaleA = std::cos(theta) - dot * sinTheta / sinTheta0;
    float scaleB = sinTheta / sinTheta0;

    Vec4 r;
    r.x = scaleA * qa.x + scaleB * qb.x;
    r.y = scaleA * qa.y + scaleB * qb.y;
    r.z = scaleA * qa.z + scaleB * qb.z;
    r.w = scaleA * qa.w + scaleB * qb.w;
    return r;
}

Pose AnimationBlend::blend(const Pose& poseA,
                            const Pose& poseB,
                            float factor) {
    size_t boneCount = std::min(
        static_cast<size_t>(poseA.modelMatrices().size()),
        static_cast<size_t>(poseB.modelMatrices().size()));
    Pose result(boneCount);

    float t = std::clamp(factor, 0.f, 1.f);

    for (size_t b = 0; b < boneCount; ++b) {
        const Transform& ta = poseA.localTransform(b);
        const Transform& tb = poseB.localTransform(b);

        Transform blended;
        blended.translation = Vec3{
            ta.translation.x + t * (tb.translation.x - ta.translation.x),
            ta.translation.y + t * (tb.translation.y - ta.translation.y),
            ta.translation.z + t * (tb.translation.z - ta.translation.z)
        };
        blended.rotation = slerpQuat(ta.rotation, tb.rotation, t);
        blended.scale = Vec3{
            ta.scale.x + t * (tb.scale.x - ta.scale.x),
            ta.scale.y + t * (tb.scale.y - ta.scale.y),
            ta.scale.z + t * (tb.scale.z - ta.scale.z)
        };

        result.setLocalTransform(b, blended);
    }

    return result;
}

} // namespace nexus::animation
