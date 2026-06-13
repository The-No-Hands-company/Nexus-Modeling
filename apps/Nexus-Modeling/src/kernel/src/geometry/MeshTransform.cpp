#include <nexus/geometry/MeshTransform.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;
using RVec4 = nexus::render::Vec4;
using Mat4 = nexus::render::Mat4;

namespace {

[[nodiscard]] bool isFiniteFloat(float value) noexcept {
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] Mat4 translationMatrix(const Vec3& offset) noexcept {
    Mat4 m = Mat4::identity();
    m.m[0][3] = offset.x;
    m.m[1][3] = offset.y;
    m.m[2][3] = offset.z;
    return m;
}

[[nodiscard]] Mat4 scaleMatrix(const Vec3& s) noexcept {
    Mat4 m = Mat4::identity();
    m.m[0][0] = s.x;
    m.m[1][1] = s.y;
    m.m[2][2] = s.z;
    return m;
}

} // namespace

Quat Quat::fromAxisAngle(const Vec3& axis, float angle) noexcept {
    const float halfAngle = angle * 0.5f;
    const float s = std::sin(halfAngle);
    const Vec3 n = axis.normalize();
    return {n.x * s, n.y * s, n.z * s, std::cos(halfAngle)};
}

Mat4 Quat::toMat4() const noexcept {
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;

    Mat4 mat{};
    mat.m[0][0] = 1.f - 2.f * (yy + zz);
    mat.m[0][1] = 2.f * (xy - wz);
    mat.m[0][2] = 2.f * (xz + wy);
    mat.m[0][3] = 0.f;

    mat.m[1][0] = 2.f * (xy + wz);
    mat.m[1][1] = 1.f - 2.f * (xx + zz);
    mat.m[1][2] = 2.f * (yz - wx);
    mat.m[1][3] = 0.f;

    mat.m[2][0] = 2.f * (xz - wy);
    mat.m[2][1] = 2.f * (yz + wx);
    mat.m[2][2] = 1.f - 2.f * (xx + yy);
    mat.m[2][3] = 0.f;

    mat.m[3][0] = 0.f;
    mat.m[3][1] = 0.f;
    mat.m[3][2] = 0.f;
    mat.m[3][3] = 1.f;
    return mat;
}

Quat Quat::normalize() const noexcept {
    const float lenSq = x * x + y * y + z * z + w * w;
    if (lenSq < 1e-12f) return identity();
    const float inv = 1.f / std::sqrt(lenSq);
    return {x * inv, y * inv, z * inv, w * inv};
}

Mat4 MeshTransform::normalMatrix(const Mat4& matrix) {
    const float sx = std::sqrt(matrix.m[0][0] * matrix.m[0][0] +
                                matrix.m[1][0] * matrix.m[1][0] +
                                matrix.m[2][0] * matrix.m[2][0]);
    const float sy = std::sqrt(matrix.m[0][1] * matrix.m[0][1] +
                                matrix.m[1][1] * matrix.m[1][1] +
                                matrix.m[2][1] * matrix.m[2][1]);
    const float sz = std::sqrt(matrix.m[0][2] * matrix.m[0][2] +
                                matrix.m[1][2] * matrix.m[1][2] +
                                matrix.m[2][2] * matrix.m[2][2]);

    const bool anyZero = (sx < 1e-8f) || (sy < 1e-8f) || (sz < 1e-8f);
    if (anyZero) {
        return Mat4::identity();
    }

    const bool nonUniform =
        (std::fabs(sx - sy) > 1e-5f) ||
        (std::fabs(sy - sz) > 1e-5f) ||
        (std::fabs(sx - sz) > 1e-5f);

    if (!nonUniform) {
        Mat4 nm = Mat4::identity();
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                nm.m[i][j] = matrix.m[i][j];
        return nm;
    }

    Mat4 inv = matrix.inverse();
    Mat4 transp = inv.transpose();
    return transp;
}

Mesh MeshTransform::apply(const Mesh& mesh,
                           const Mat4& matrix,
                           bool transformNormals,
                           const Vec3& pivot) {
    Mesh result = mesh;
    if (!result.isValid()) return result;

    const bool usePivot = (pivot.x != 0.f || pivot.y != 0.f || pivot.z != 0.f);
    Mat4 effective = matrix;
    if (usePivot) {
        effective = translationMatrix(pivot) * matrix * translationMatrix({-pivot.x, -pivot.y, -pivot.z});
    }

    const auto& pos = mesh.attributes().positions();
    std::vector<Vec3> newPos;
    newPos.reserve(pos.size());

    for (const auto& p : pos) {
        RVec4 v{p.x, p.y, p.z, 1.f};
        RVec4 t = effective * v;
        const float wInv = 1.f / (std::fabs(t.w) > 1e-8f ? t.w : 1.f);
        newPos.push_back({t.x * wInv, t.y * wInv, t.z * wInv});
    }

    result.attributes().setPositions(std::move(newPos));

    if (transformNormals && mesh.attributes().hasNormals()) {
        const Mat4 nm = normalMatrix(matrix);

        const auto& norms = mesh.attributes().normals();
        std::vector<Vec3> newNorms;
        newNorms.reserve(norms.size());

        for (const auto& n : norms) {
            RVec4 v{n.x, n.y, n.z, 0.f};
            RVec4 t = nm * v;
            Vec3 tn = {t.x, t.y, t.z};
            newNorms.push_back(tn.normalize());
        }

        result.attributes().setNormals(std::move(newNorms));

        if (mesh.attributes().hasTangents()) {
            const auto& tans = mesh.attributes().tangents();
            std::vector<nexus::geometry::Vec4> newTans;
            newTans.reserve(tans.size());

            for (const auto& tan : tans) {
                RVec4 v{tan.x, tan.y, tan.z, 0.f};
                RVec4 t = nm * v;
                Vec3 tn(t.x, t.y, t.z);
                newTans.push_back({tn.x, tn.y, tn.z, tan.w});
            }

            result.attributes().setTangents(std::move(newTans));
        }
    }

    result.rebuildStableElementIds();
    return result;
}

Mesh MeshTransform::translate(const Mesh& mesh, const Vec3& offset) {
    return apply(mesh, translationMatrix(offset));
}

Mesh MeshTransform::rotate(const Mesh& mesh, const Quat& rotation) {
    return apply(mesh, rotation.toMat4());
}

Mesh MeshTransform::scale(const Mesh& mesh, const Vec3& scaleFactors) {
    return apply(mesh, scaleMatrix(scaleFactors));
}

Mesh MeshTransform::centerToOrigin(const Mesh& mesh) {
    if (!mesh.isValid()) return mesh;

    const auto& pos = mesh.attributes().positions();
    if (pos.empty()) return mesh;

    Vec3 lo = pos.front();
    Vec3 hi = pos.front();
    for (const auto& p : pos) {
        if (!isFiniteFloat(p.x) || !isFiniteFloat(p.y) || !isFiniteFloat(p.z))
            return mesh;
        lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
    }

    const Vec3 center = (lo + hi) * 0.5f;
    return translate(mesh, {-center.x, -center.y, -center.z});
}

Mesh MeshTransform::normalizeScale(const Mesh& mesh, float targetSize) {
    if (!mesh.isValid() || targetSize <= 0.f) return mesh;

    const auto& pos = mesh.attributes().positions();
    if (pos.empty()) return mesh;

    Vec3 lo = pos.front();
    Vec3 hi = pos.front();
    for (const auto& p : pos) {
        if (!isFiniteFloat(p.x) || !isFiniteFloat(p.y) || !isFiniteFloat(p.z))
            return mesh;
        lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
    }

    const Vec3 ext = hi - lo;
    const float currentSize = std::max({ext.x, ext.y, ext.z});
    if (currentSize < 1e-8f) return mesh;

    const float s = targetSize / currentSize;
    return apply(mesh, scaleMatrix({s, s, s}));
}

} // namespace nexus::geometry
