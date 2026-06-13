#include <nexus/geometry/MeshInstancer.h>
#include <nexus/geometry/MeshPointSample.h>
#include "SplitMix64.h"

#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;
using Mat4 = nexus::render::Mat4;

namespace {

Mat4 rodriguesRotation(const Vec3& from, const Vec3& to) noexcept {
    Vec3 fn = from.normalize();
    Vec3 tn = to.normalize();

    float dot = fn.dot(tn);
    if (dot > 0.9999f) return Mat4::identity();
    if (dot < -0.9999f) {
        Mat4 r{};
        r.m[0][0] = -1.f; r.m[1][1] = -1.f; r.m[2][2] = -1.f; r.m[3][3] = 1.f;
        return r;
    }

    Vec3 axis = fn.cross(tn);
    float angle = std::acos(dot);
    Vec3 an = axis.normalize();

    float c = std::cos(angle);
    float s = std::sin(angle);
    float t = 1.f - c;

    Mat4 r{};
    r.m[0][0] = t * an.x * an.x + c;
    r.m[0][1] = t * an.x * an.y - s * an.z;
    r.m[0][2] = t * an.x * an.z + s * an.y;
    r.m[0][3] = 0.f;
    r.m[1][0] = t * an.x * an.y + s * an.z;
    r.m[1][1] = t * an.y * an.y + c;
    r.m[1][2] = t * an.y * an.z - s * an.x;
    r.m[1][3] = 0.f;
    r.m[2][0] = t * an.x * an.z - s * an.y;
    r.m[2][1] = t * an.y * an.z + s * an.x;
    r.m[2][2] = t * an.z * an.z + c;
    r.m[2][3] = 0.f;
    r.m[3][0] = 0.f; r.m[3][1] = 0.f; r.m[3][2] = 0.f; r.m[3][3] = 1.f;

    return r;
}

Mat4 makeTransform(const Vec3& translation, const Vec3& normal, float scale,
                   bool alignToNormal) {
    Mat4 rot = Mat4::identity();
    if (alignToNormal) {
        rot = rodriguesRotation(Vec3{0.f, 1.f, 0.f}, normal);
    }

    Mat4 scl{};
    scl.m[0][0] = scale; scl.m[1][1] = scale; scl.m[2][2] = scale; scl.m[3][3] = 1.f;

    Mat4 trans{};
    trans.m[0][0] = 1.f; trans.m[1][1] = 1.f; trans.m[2][2] = 1.f; trans.m[3][3] = 1.f;
    trans.m[0][3] = translation.x;
    trans.m[1][3] = translation.y;
    trans.m[2][3] = translation.z;

    return trans * rot * scl;
}

} // namespace

Mesh MeshInstancer::scatter(const Mesh& source, const Mesh& target, const InstancerOptions& opts) {
    PointSampleOptions sampOpts;
    sampOpts.count = 1024;
    sampOpts.seed = opts.seed;
    auto samples = MeshPointSample::sample(target, sampOpts);

    return scatterAtPoints(source, samples.points, samples.normals, opts);
}

Mesh MeshInstancer::scatterAtPoints(const Mesh& source,
                                      const std::vector<Vec3>& positions,
                                      const std::vector<Vec3>& normals,
                                      const InstancerOptions& opts) {
    Mesh result;
    if (!source.isValid() || positions.empty()) return result;

    internal::SplitMix64 rng(opts.seed + 1);

    size_t count = std::min(positions.size(), normals.size());

    for (size_t i = 0; i < count; ++i) {
        float t = static_cast<float>(rng.uniform01());
        float scale = opts.scaleMin + t * (opts.scaleMax - opts.scaleMin);

        Mat4 xf = makeTransform(positions[i], normals[i], scale, opts.alignToNormal);

        Mesh instance = source;
        auto& attr = instance.attributes();
        auto pts = attr.positions();
        for (auto& p : pts) {
            nexus::render::Vec4 v{p.x, p.y, p.z, 1.f};
            nexus::render::Vec4 r = xf * v;
            float wInv = 1.f / (std::fabs(r.w) > 1e-8f ? r.w : 1.f);
            p = {r.x * wInv, r.y * wInv, r.z * wInv};
        }
        attr.setPositions(std::move(pts));

        if (attr.hasNormals()) {
            auto npts = attr.normals();
            for (auto& n : npts) {
                float nx = n.x * xf.m[0][0] + n.y * xf.m[1][0] + n.z * xf.m[2][0];
                float ny = n.x * xf.m[0][1] + n.y * xf.m[1][1] + n.z * xf.m[2][1];
                float nz = n.x * xf.m[0][2] + n.y * xf.m[1][2] + n.z * xf.m[2][2];
                float lenSq = nx * nx + ny * ny + nz * nz;
                if (lenSq > 1e-12f) {
                    float inv = 1.f / std::sqrt(lenSq);
                    n = Vec3{nx * inv, ny * inv, nz * inv};
                }
            }
            attr.setNormals(std::move(npts));
        }

        instance.rebuildStableElementIds();

        if (i == 0) {
            result = std::move(instance);
        } else {
            if (!result.appendMesh(instance)) return instance;
        }
    }

    result.rebuildStableElementIds();
    return result;
}

} // namespace nexus::geometry
