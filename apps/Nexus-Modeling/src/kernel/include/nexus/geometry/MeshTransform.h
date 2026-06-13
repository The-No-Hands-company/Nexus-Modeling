#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cmath>
#include <cstdint>

namespace nexus::geometry {

struct Quat {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;

    [[nodiscard]] static Quat identity() noexcept { return {0.f, 0.f, 0.f, 1.f}; }
    [[nodiscard]] static Quat fromAxisAngle(const nexus::render::Vec3& axis, float angle) noexcept;
    [[nodiscard]] nexus::render::Mat4 toMat4() const noexcept;
    [[nodiscard]] Quat normalize() const noexcept;
    bool operator==(const Quat&) const = default;
};

class MeshTransform {
public:
    [[nodiscard]] static Mesh apply(const Mesh& mesh,
                                     const nexus::render::Mat4& matrix,
                                     bool transformNormals = true,
                                     const nexus::render::Vec3& pivot = {0.f, 0.f, 0.f});

    [[nodiscard]] static nexus::render::Mat4 normalMatrix(const nexus::render::Mat4& matrix);

    [[nodiscard]] static Mesh translate(const Mesh& mesh, const nexus::render::Vec3& offset);
    [[nodiscard]] static Mesh rotate(const Mesh& mesh, const Quat& rotation);
    [[nodiscard]] static Mesh scale(const Mesh& mesh, const nexus::render::Vec3& scaleFactors);

    [[nodiscard]] static Mesh centerToOrigin(const Mesh& mesh);
    [[nodiscard]] static Mesh normalizeScale(const Mesh& mesh, float targetSize);
};

} // namespace nexus::geometry
