#include "SoftwareRasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace nexus::softrast {

using nexus::render::Vec3;
using nexus::render::Vec4;
using nexus::render::Mat4;
using nexus::render::Camera;

// ── CPU-side vertex transform ─────────────────────────────────────────────────
//
// The Camera's stored matrices are row-major on the host but interpreted as
// column-major on the GPU (standard GLSL/Vulkan convention). Applying viewProj
// directly in CPU code produces incorrect clip-space w values.
//
// Instead we:
//  1. Apply camera.ubo().view directly — this IS correct row-major world→view.
//  2. Build a CPU-side OpenGL-style perspective (y-up, depth [-1,1]) from
//     the camera's fovY/aspect/near/far scalars.
//  3. Flip y in the viewport mapping so y=0 is the top of the screen.
//
// This gives a self-consistent CPU rasterizer independent of the GPU matrix layout.

static Vec4 transformVertexCpu(const Camera& cam, const Vec3& worldPos) noexcept {
    // Step 1 — world → view (camera.ubo().view is correct row-major view matrix)
    const Vec4 vp = cam.ubo().view * Vec4{worldPos.x, worldPos.y, worldPos.z, 1.f};

    // Step 2 — view → clip (standard OpenGL perspective, clip_w = -view_z)
    const float fovY    = cam.ubo().fovY;
    const float aspect  = cam.ubo().aspectRatio > 0.f ? cam.ubo().aspectRatio : 1.f;
    const float nearZ   = cam.ubo().nearPlane;
    const float farZ    = cam.ubo().farPlane;
    const float tanHalf = std::tan((fovY * static_cast<float>(std::numbers::pi) / 180.f) * 0.5f);
    const float invT    = (tanHalf > 1e-8f) ? 1.f / tanHalf : 1.f;

    Vec4 clip;
    clip.x = vp.x * invT / aspect;
    clip.y = vp.y * invT;
    // Standard right-handed OpenGL depth [-1,1]:
    //   ndc_z = -(far+near)/(far-near) + (-2*far*near/(far-near)) * (1/w)
    //   clip_z = (-(far+near)/(far-near)) * vz - (2*far*near/(far-near))
    const float denom = (farZ - nearZ > 1e-8f) ? (farZ - nearZ) : 1e-8f;
    clip.z = (-(farZ + nearZ) / denom) * vp.z - (2.f * farZ * nearZ / denom);
    clip.w = -vp.z;  // perspective divide: w = -view_z (positive for points in front)
    return clip;
}

// ── Triangle rasterization ────────────────────────────────────────────────────

void SoftwareRasterizer::rasterizeTriangle(
    PixelBuffer& buf,
    Vec4 c0, Vec4 c1, Vec4 c2,
    RGBA8 color) noexcept
{
    const float W = static_cast<float>(buf.width());
    const float H = static_cast<float>(buf.height());

    // Cull triangles where any vertex is behind the camera (w <= 0).
    if (c0.w <= 0.f || c1.w <= 0.f || c2.w <= 0.f) return;

    // Perspective divide → NDC
    const float w0inv = 1.f / c0.w, w1inv = 1.f / c1.w, w2inv = 1.f / c2.w;
    const float nx0 = c0.x * w0inv, ny0 = c0.y * w0inv, nz0 = c0.z * w0inv;
    const float nx1 = c1.x * w1inv, ny1 = c1.y * w1inv, nz1 = c1.z * w1inv;
    const float nx2 = c2.x * w2inv, ny2 = c2.y * w2inv, nz2 = c2.z * w2inv;

    // NDC → screen. y-flip: NDC y=+1 (top) → screen y=0, NDC y=-1 → screen y=H.
    const float sx0 = (nx0 * 0.5f + 0.5f) * W;
    const float sy0 = (1.f - (ny0 * 0.5f + 0.5f)) * H;
    const float sx1 = (nx1 * 0.5f + 0.5f) * W;
    const float sy1 = (1.f - (ny1 * 0.5f + 0.5f)) * H;
    const float sx2 = (nx2 * 0.5f + 0.5f) * W;
    const float sy2 = (1.f - (ny2 * 0.5f + 0.5f)) * H;

    // Signed area: negative for CCW triangles in y-down screen space.
    // (With y-flip, world-CCW front faces are CW in screen space → cross < 0.)
    const float area = (sx1 - sx0) * (sy2 - sy0) - (sy1 - sy0) * (sx2 - sx0);
    if (area >= 0.f) return; // back-face or degenerate

    // Bounding box clamped to viewport
    const int minX = std::max(0, static_cast<int>(std::floor(std::min({sx0, sx1, sx2}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({sy0, sy1, sy2}))));
    const int maxX = std::min(static_cast<int>(buf.width())  - 1,
                              static_cast<int>(std::ceil (std::max({sx0, sx1, sx2}))));
    const int maxY = std::min(static_cast<int>(buf.height()) - 1,
                              static_cast<int>(std::ceil (std::max({sy0, sy1, sy2}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;

            // Standard barycentric weights (correct for any sign of area)
            const float w0 = ((sy1 - sy2) * (px - sx2) + (sx2 - sx1) * (py - sy2)) / area;
            const float w1 = ((sy2 - sy0) * (px - sx0) + (sx0 - sx2) * (py - sy0)) / area;
            const float w2 = 1.f - w0 - w1;

            if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;

            // Depth in NDC z ∈ [-1, 1]
            const float depth = w0 * nz0 + w1 * nz1 + w2 * nz2;
            if (depth < -1.f || depth > 1.f) continue;

            buf.setPixelDepth(static_cast<uint32_t>(x), static_cast<uint32_t>(y), color, depth);
        }
    }
}

void SoftwareRasterizer::drawLine(
    PixelBuffer& buf,
    int x0, int y0, int x1, int y1,
    RGBA8 color) noexcept
{
    const int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        buf.setPixel(static_cast<uint32_t>(x0), static_cast<uint32_t>(y0), color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

// ── render ────────────────────────────────────────────────────────────────────

void SoftwareRasterizer::render(
    PixelBuffer&                 buf,
    const nexus::geometry::Mesh& mesh,
    const nexus::render::Camera& camera,
    const RasterizerConfig&      cfg) const
{
    buf.clear(cfg.background);

    const auto& positions = mesh.attributes().positions();
    if (positions.empty()) return;

    const float W = static_cast<float>(buf.width());
    const float H = static_cast<float>(buf.height());

    // Fixed directional light (world space)
    const Vec3 lightDir{0.577f, 0.577f, 0.577f};
    const float ambientMin = 0.15f;

    const std::size_t faceCount = mesh.topology().faceCount();
    for (std::size_t fi = 0; fi < faceCount; ++fi) {
        const auto& face = mesh.topology().face(fi);
        if (face.indices.size() < 3) continue;

        // Fan-triangulate the polygon: (0,1,2), (0,2,3), …
        for (std::size_t ti = 1; ti + 1 < face.indices.size(); ++ti) {
            const uint32_t i0 = face.indices[0];
            const uint32_t i1 = face.indices[ti];
            const uint32_t i2 = face.indices[ti + 1];

            if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
                continue;

            const Vec3& p0 = positions[i0];
            const Vec3& p1 = positions[i1];
            const Vec3& p2 = positions[i2];

            const Vec4 c0 = transformVertexCpu(camera, p0);
            const Vec4 c1 = transformVertexCpu(camera, p1);
            const Vec4 c2 = transformVertexCpu(camera, p2);

            if (cfg.mode == ShadingMode::Wireframe) {
                auto toScreen = [&](const Vec4& c) -> std::pair<int,int> {
                    if (c.w <= 0.f) return {-1, -1};
                    const float inv = 1.f / c.w;
                    return {
                        static_cast<int>((c.x * inv * 0.5f + 0.5f) * W),
                        static_cast<int>((1.f - (c.y * inv * 0.5f + 0.5f)) * H)
                    };
                };
                auto [x0,y0] = toScreen(c0);
                auto [x1,y1] = toScreen(c1);
                auto [x2,y2] = toScreen(c2);
                if (c0.w > 0.f && c1.w > 0.f) drawLine(buf, x0,y0, x1,y1, cfg.wireColor);
                if (c1.w > 0.f && c2.w > 0.f) drawLine(buf, x1,y1, x2,y2, cfg.wireColor);
                if (c2.w > 0.f && c0.w > 0.f) drawLine(buf, x2,y2, x0,y0, cfg.wireColor);
            } else {
                // Flat shading: face normal from world-space edge cross product
                const Vec3 faceNormal = (p1 - p0).cross(p2 - p0).normalize();
                const float ndotl = std::max(ambientMin, faceNormal.dot(lightDir));
                const RGBA8 shaded{
                    static_cast<uint8_t>(static_cast<float>(cfg.baseColor.r) * ndotl),
                    static_cast<uint8_t>(static_cast<float>(cfg.baseColor.g) * ndotl),
                    static_cast<uint8_t>(static_cast<float>(cfg.baseColor.b) * ndotl),
                    255
                };
                rasterizeTriangle(buf, c0, c1, c2, shaded);
            }
        }
    }
}

} // namespace nexus::softrast
