#pragma once
// --- Nexus Geometry — RobustPredicates

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

class RobustPredicates {
public:
    [[nodiscard]] static double orient2D(const Vec2& a, const Vec2& b, const Vec2& c) noexcept;
    [[nodiscard]] static double orient3D(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                                         const nexus::render::Vec3& c, const nexus::render::Vec3& d) noexcept;
    [[nodiscard]] static double inCircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) noexcept;
};

} // namespace nexus::geometry
