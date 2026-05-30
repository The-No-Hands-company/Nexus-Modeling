#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — CPU triangle rasterizer
//
//  Renders nexus::geometry::Mesh triangles (or quads fan-triangulated) into a
//  PixelBuffer using camera matrices from nexus::render::Camera.
//
//  Modes:
//    Flat      — per-face Lambert + optional Blinn-Phong specular
//    Gouraud   — per-vertex shading interpolated across triangles
//    Wireframe — Bresenham edge lines, no fill
//
//  Texturing:
//    When RasterizerConfig::texture is non-empty and the mesh carries UV
//    coordinates, each triangle is rendered with perspective-correct UV
//    interpolation. The texture sample is multiplied by the lighting result.
// ─────────────────────────────────────────────────────────────────────────────
#include "PixelBuffer.h"
#include "Texture2D.h"
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

namespace nexus::softrast {

enum class ShadingMode : uint8_t {
    Flat,       // per-face Lambert, constant color per triangle
    Gouraud,    // per-vertex Lambert, interpolated across triangle
    Wireframe,
};

struct RasterizerConfig {
    ShadingMode          mode         = ShadingMode::Flat;
    RGBA8                background   = {30,  30,  30,  255};
    RGBA8                baseColor    = {180, 180, 180, 255};
    RGBA8                wireColor    = {220, 220, 220, 255};
    RGBA8                specColor    = {255, 255, 255, 255};
    nexus::render::Vec3  lightDir     = {0.577f, 0.577f, 0.577f}; // world-space, unit length
    float                ambientMin   = 0.15f;
    float                specStrength = 0.0f;   // 0 = no specular (default)
    float                shininess    = 32.0f;  // Blinn-Phong exponent

    // Optional texture. When non-empty and the mesh has UVs, the texture
    // sample is multiplied by the per-pixel lighting color.
    Texture2D            texture;
    TextureFilter        texFilter    = TextureFilter::Bilinear;
};

class SoftwareRasterizer {
public:
    /// Render mesh into buf, clearing it first.
    void render(PixelBuffer&                 buf,
                const nexus::geometry::Mesh& mesh,
                const nexus::render::Camera& camera,
                const RasterizerConfig&      cfg = {}) const;

    /// Render mesh into buf applying modelMatrix (TRS) in world space.
    /// Does NOT clear the buffer — call buf.clear() before the first renderInto,
    /// then chain multiple renderInto calls to composite a scene.
    void renderInto(PixelBuffer&                  buf,
                    const nexus::geometry::Mesh&  mesh,
                    const nexus::render::Camera&  camera,
                    const RasterizerConfig&        cfg,
                    const nexus::render::Mat4&     modelMatrix) const;

private:
    // Core rendering implementation (no clear, uses modelMatrix).
    void renderImpl(PixelBuffer&                  buf,
                    const nexus::geometry::Mesh&  mesh,
                    const nexus::render::Camera&  camera,
                    const RasterizerConfig&        cfg,
                    const nexus::render::Mat4&     modelMatrix) const;

    static void rasterizeFlat(
        PixelBuffer&        buf,
        nexus::render::Vec4 c0,
        nexus::render::Vec4 c1,
        nexus::render::Vec4 c2,
        RGBA8               color) noexcept;

    static void rasterizeGouraud(
        PixelBuffer&        buf,
        nexus::render::Vec4 c0, RGBA8 col0,
        nexus::render::Vec4 c1, RGBA8 col1,
        nexus::render::Vec4 c2, RGBA8 col2) noexcept;

    // Perspective-correct UV interpolation; litColor is the per-pixel
    // lighting modulator (from flat or Gouraud path).  When Gouraud, pass
    // the three per-vertex lit colors; for flat pass the same color thrice.
    static void rasterizeTextured(
        PixelBuffer&        buf,
        nexus::render::Vec4 c0, float u0, float v0, RGBA8 lit0,
        nexus::render::Vec4 c1, float u1, float v1, RGBA8 lit1,
        nexus::render::Vec4 c2, float u2, float v2, RGBA8 lit2,
        const Texture2D&    tex,
        TextureFilter       filter) noexcept;

    static void drawLine(
        PixelBuffer& buf,
        int x0, int y0,
        int x1, int y1,
        RGBA8 color) noexcept;
};

} // namespace nexus::softrast
