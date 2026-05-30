#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — CPU triangle rasterizer
//
//  Renders nexus::geometry::Mesh triangles (or quads fan-triangulated) into a
//  PixelBuffer using camera matrices from nexus::render::Camera.
//
//  Modes:
//    Flat      — per-face Lambert shading, fixed directional light
//    Wireframe — Bresenham edge lines, no fill
// ─────────────────────────────────────────────────────────────────────────────
#include "PixelBuffer.h"
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

namespace nexus::softrast {

enum class ShadingMode : uint8_t {
    Flat,
    Wireframe,
};

struct RasterizerConfig {
    ShadingMode mode       = ShadingMode::Flat;
    RGBA8       background = {30,  30,  30,  255};
    RGBA8       baseColor  = {180, 180, 180, 255};
    RGBA8       wireColor  = {220, 220, 220, 255};
};

class SoftwareRasterizer {
public:
    /// Render mesh into buf, clearing it first.
    void render(PixelBuffer&                      buf,
                const nexus::geometry::Mesh&      mesh,
                const nexus::render::Camera&      camera,
                const RasterizerConfig&           cfg = {}) const;

private:
    // Transform world-space position through MVP to clip space.
    // Fill a triangle with barycentric rasterization + depth test.
    static void rasterizeTriangle(
        PixelBuffer&              buf,
        nexus::render::Vec4       c0,
        nexus::render::Vec4       c1,
        nexus::render::Vec4       c2,
        RGBA8                     color) noexcept;

    // Draw a Bresenham line.
    static void drawLine(
        PixelBuffer& buf,
        int x0, int y0,
        int x1, int y1,
        RGBA8 color) noexcept;
};

} // namespace nexus::softrast
