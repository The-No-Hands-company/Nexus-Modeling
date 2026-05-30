#include "SoftwareRasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <unordered_map>
#include <vector>

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

// ── Shared screen-space setup ─────────────────────────────────────────────────

struct ScreenTri {
    float sx0, sy0, nz0;
    float sx1, sy1, nz1;
    float sx2, sy2, nz2;
    float area;
};

static bool buildScreenTri(const PixelBuffer& buf,
                            Vec4 c0, Vec4 c1, Vec4 c2,
                            ScreenTri& t) noexcept
{
    if (c0.w <= 0.f || c1.w <= 0.f || c2.w <= 0.f) return false;

    const float W = static_cast<float>(buf.width());
    const float H = static_cast<float>(buf.height());

    const float w0inv = 1.f / c0.w, w1inv = 1.f / c1.w, w2inv = 1.f / c2.w;
    const float nx0 = c0.x * w0inv, ny0 = c0.y * w0inv;
    const float nx1 = c1.x * w1inv, ny1 = c1.y * w1inv;
    const float nx2 = c2.x * w2inv, ny2 = c2.y * w2inv;

    t.sx0 = (nx0 * 0.5f + 0.5f) * W;
    t.sy0 = (1.f - (ny0 * 0.5f + 0.5f)) * H;
    t.sx1 = (nx1 * 0.5f + 0.5f) * W;
    t.sy1 = (1.f - (ny1 * 0.5f + 0.5f)) * H;
    t.sx2 = (nx2 * 0.5f + 0.5f) * W;
    t.sy2 = (1.f - (ny2 * 0.5f + 0.5f)) * H;
    t.nz0 = c0.z * w0inv;
    t.nz1 = c1.z * w1inv;
    t.nz2 = c2.z * w2inv;

    // Signed area negative for front-facing triangles (y-down screen, y-flipped)
    t.area = (t.sx1 - t.sx0) * (t.sy2 - t.sy0) - (t.sy1 - t.sy0) * (t.sx2 - t.sx0);
    return t.area < 0.f; // cull back-faces and degenerates
}

// ── Triangle rasterization ────────────────────────────────────────────────────

void SoftwareRasterizer::rasterizeFlat(
    PixelBuffer& buf,
    Vec4 c0, Vec4 c1, Vec4 c2,
    RGBA8 color) noexcept
{
    ScreenTri t;
    if (!buildScreenTri(buf, c0, c1, c2, t)) return;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({t.sx0, t.sx1, t.sx2}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({t.sy0, t.sy1, t.sy2}))));
    const int maxX = std::min(static_cast<int>(buf.width())  - 1,
                              static_cast<int>(std::ceil (std::max({t.sx0, t.sx1, t.sx2}))));
    const int maxY = std::min(static_cast<int>(buf.height()) - 1,
                              static_cast<int>(std::ceil (std::max({t.sy0, t.sy1, t.sy2}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float w0 = ((t.sy1-t.sy2)*(px-t.sx2)+(t.sx2-t.sx1)*(py-t.sy2)) / t.area;
            const float w1 = ((t.sy2-t.sy0)*(px-t.sx0)+(t.sx0-t.sx2)*(py-t.sy0)) / t.area;
            const float w2 = 1.f - w0 - w1;
            if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
            const float depth = w0*t.nz0 + w1*t.nz1 + w2*t.nz2;
            if (depth < -1.f || depth > 1.f) continue;
            buf.setPixelDepth(static_cast<uint32_t>(x), static_cast<uint32_t>(y), color, depth);
        }
    }
}

void SoftwareRasterizer::rasterizeGouraud(
    PixelBuffer& buf,
    Vec4 c0, RGBA8 col0,
    Vec4 c1, RGBA8 col1,
    Vec4 c2, RGBA8 col2) noexcept
{
    ScreenTri t;
    if (!buildScreenTri(buf, c0, c1, c2, t)) return;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({t.sx0, t.sx1, t.sx2}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({t.sy0, t.sy1, t.sy2}))));
    const int maxX = std::min(static_cast<int>(buf.width())  - 1,
                              static_cast<int>(std::ceil (std::max({t.sx0, t.sx1, t.sx2}))));
    const int maxY = std::min(static_cast<int>(buf.height()) - 1,
                              static_cast<int>(std::ceil (std::max({t.sy0, t.sy1, t.sy2}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float w0 = ((t.sy1-t.sy2)*(px-t.sx2)+(t.sx2-t.sx1)*(py-t.sy2)) / t.area;
            const float w1 = ((t.sy2-t.sy0)*(px-t.sx0)+(t.sx0-t.sx2)*(py-t.sy0)) / t.area;
            const float w2 = 1.f - w0 - w1;
            if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
            const float depth = w0*t.nz0 + w1*t.nz1 + w2*t.nz2;
            if (depth < -1.f || depth > 1.f) continue;

            // Bilinear blend of per-vertex colors
            const RGBA8 px_color{
                static_cast<uint8_t>(w0*col0.r + w1*col1.r + w2*col2.r),
                static_cast<uint8_t>(w0*col0.g + w1*col1.g + w2*col2.g),
                static_cast<uint8_t>(w0*col0.b + w1*col1.b + w2*col2.b),
                255
            };
            buf.setPixelDepth(static_cast<uint32_t>(x), static_cast<uint32_t>(y), px_color, depth);
        }
    }
}

void SoftwareRasterizer::rasterizeTextured(
    PixelBuffer& buf,
    Vec4 c0, float u0, float v0, RGBA8 lit0,
    Vec4 c1, float u1, float v1, RGBA8 lit1,
    Vec4 c2, float u2, float v2, RGBA8 lit2,
    const Texture2D& tex,
    TextureFilter filter) noexcept
{
    ScreenTri t;
    if (!buildScreenTri(buf, c0, c1, c2, t)) return;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({t.sx0, t.sx1, t.sx2}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({t.sy0, t.sy1, t.sy2}))));
    const int maxX = std::min(static_cast<int>(buf.width())  - 1,
                              static_cast<int>(std::ceil (std::max({t.sx0, t.sx1, t.sx2}))));
    const int maxY = std::min(static_cast<int>(buf.height()) - 1,
                              static_cast<int>(std::ceil (std::max({t.sy0, t.sy1, t.sy2}))));

    // Perspective-correct: divide UV by w, interpolate with barycentrics, then divide by interp(1/w)
    const float w0inv = 1.f / c0.w;
    const float w1inv = 1.f / c1.w;
    const float w2inv = 1.f / c2.w;
    const float u0w = u0 * w0inv, v0w = v0 * w0inv;
    const float u1w = u1 * w1inv, v1w = v1 * w1inv;
    const float u2w = u2 * w2inv, v2w = v2 * w2inv;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float b0 = ((t.sy1-t.sy2)*(px-t.sx2)+(t.sx2-t.sx1)*(py-t.sy2)) / t.area;
            const float b1 = ((t.sy2-t.sy0)*(px-t.sx0)+(t.sx0-t.sx2)*(py-t.sy0)) / t.area;
            const float b2 = 1.f - b0 - b1;
            if (b0 < 0.f || b1 < 0.f || b2 < 0.f) continue;
            const float depth = b0*t.nz0 + b1*t.nz1 + b2*t.nz2;
            if (depth < -1.f || depth > 1.f) continue;

            // Perspective-correct UV
            const float interpW = b0 * w0inv + b1 * w1inv + b2 * w2inv;
            const float u = (b0 * u0w + b1 * u1w + b2 * u2w) / interpW;
            const float v = (b0 * v0w + b1 * v1w + b2 * v2w) / interpW;

            // Interpolate lighting color
            const RGBA8 litPx{
                static_cast<uint8_t>(b0*lit0.r + b1*lit1.r + b2*lit2.r),
                static_cast<uint8_t>(b0*lit0.g + b1*lit1.g + b2*lit2.g),
                static_cast<uint8_t>(b0*lit0.b + b1*lit1.b + b2*lit2.b),
                255
            };

            const RGBA8 texel = tex.sample(u, v, filter);
            // Modulate: multiply normalized lighting by texture sample
            buf.setPixelDepth(static_cast<uint32_t>(x), static_cast<uint32_t>(y), RGBA8{
                static_cast<uint8_t>(static_cast<uint32_t>(litPx.r) * texel.r / 255u),
                static_cast<uint8_t>(static_cast<uint32_t>(litPx.g) * texel.g / 255u),
                static_cast<uint8_t>(static_cast<uint32_t>(litPx.b) * texel.b / 255u),
                255
            }, depth);
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

// ── render / renderInto / renderImpl ─────────────────────────────────────────

void SoftwareRasterizer::render(
    PixelBuffer&                 buf,
    const nexus::geometry::Mesh& mesh,
    const nexus::render::Camera& camera,
    const RasterizerConfig&      cfg) const
{
    buf.clear(cfg.background);
    renderImpl(buf, mesh, camera, cfg, Mat4::identity());
}

void SoftwareRasterizer::renderInto(
    PixelBuffer&                 buf,
    const nexus::geometry::Mesh& mesh,
    const nexus::render::Camera& camera,
    const RasterizerConfig&      cfg,
    const Mat4&                  modelMatrix) const
{
    renderImpl(buf, mesh, camera, cfg, modelMatrix);
}

void SoftwareRasterizer::renderImpl(
    PixelBuffer&                 buf,
    const nexus::geometry::Mesh& mesh,
    const nexus::render::Camera& camera,
    const RasterizerConfig&      cfg,
    const Mat4&                  modelMatrix) const
{

    const auto& positions = mesh.attributes().positions();
    if (positions.empty()) return;

    const bool identityModel = (modelMatrix.m[0][0] == 1.f && modelMatrix.m[1][1] == 1.f &&
                                modelMatrix.m[2][2] == 1.f && modelMatrix.m[3][3] == 1.f &&
                                modelMatrix.m[0][3] == 0.f && modelMatrix.m[1][3] == 0.f &&
                                modelMatrix.m[2][3] == 0.f);

    // Transform object-space position to world space via modelMatrix.
    auto toWorld = [&](const Vec3& p) -> Vec3 {
        if (identityModel) return p;
        const Vec4 w = modelMatrix * Vec4{p.x, p.y, p.z, 1.f};
        const float inv = (w.w != 0.f) ? 1.f / w.w : 1.f;
        return {w.x * inv, w.y * inv, w.z * inv};
    };

    // Transform a direction (normal / light) through the model's upper-3×3.
    // For non-uniform scale we should use the inverse-transpose, but TRS with
    // uniform scale is exact; the normalize call in lighting handles the rest.
    auto toWorldDir = [&](const Vec3& d) -> Vec3 {
        if (identityModel) return d;
        const Vec4 wd = modelMatrix * Vec4{d.x, d.y, d.z, 0.f};
        return Vec3{wd.x, wd.y, wd.z}.normalize();
    };

    const float W = static_cast<float>(buf.width());
    const float H = static_cast<float>(buf.height());

    const Vec3  lightDir    = cfg.lightDir;
    const float ambientMin  = cfg.ambientMin;
    const float specStr     = cfg.specStrength;
    const float shininess   = cfg.shininess;
    const Vec3  eyePos      = camera.position();
    const std::size_t nVerts = positions.size();

    // Blinn-Phong lighting for a surface point at worldPos with normal N.
    // Returns a clamped [0,1] intensity to multiply into base color, plus
    // a separate specular contribution (already in [0,1] per channel).
    auto blinnPhong = [&](const Vec3& N, const Vec3& worldPos)
        -> std::pair<float /*diffuse*/, float /*spec*/> {
        const float ndotl    = std::max(ambientMin, N.dot(lightDir));
        float spec = 0.f;
        if (specStr > 0.f) {
            const Vec3 V = (eyePos - worldPos).normalize();
            const Vec3 H = (lightDir + V).normalize();
            const float ndoth = std::max(0.f, N.dot(H));
            spec = specStr * std::pow(ndoth, shininess);
        }
        return {ndotl, spec};
    };

    // Compose diffuse + specular into final RGBA8.
    auto litColor = [&](const Vec3& N, const Vec3& worldPos) -> RGBA8 {
        auto [diff, spec] = blinnPhong(N, worldPos);
        return RGBA8{
            static_cast<uint8_t>(std::min(255.f,
                diff * cfg.baseColor.r + spec * cfg.specColor.r)),
            static_cast<uint8_t>(std::min(255.f,
                diff * cfg.baseColor.g + spec * cfg.specColor.g)),
            static_cast<uint8_t>(std::min(255.f,
                diff * cfg.baseColor.b + spec * cfg.specColor.b)),
            255
        };
    };

    // ── Gouraud: pre-compute per-vertex normals (average of adjacent face normals) ─
    // Pre-transform all positions to world space (identity fast-path avoids the multiply)
    std::vector<Vec3> worldPos(nVerts);
    for (std::size_t vi = 0; vi < nVerts; ++vi)
        worldPos[vi] = toWorld(positions[vi]);

    // Gouraud: build per-vertex normals from world-space face edges
    std::vector<Vec3> vertNormals;
    if (cfg.mode == ShadingMode::Gouraud) {
        vertNormals.assign(nVerts, {0.f, 0.f, 0.f});
        const std::size_t faceCount2 = mesh.topology().faceCount();
        for (std::size_t fi = 0; fi < faceCount2; ++fi) {
            const auto& face = mesh.topology().face(fi);
            if (face.indices.size() < 3) continue;
            for (std::size_t ti = 1; ti + 1 < face.indices.size(); ++ti) {
                const uint32_t i0 = face.indices[0];
                const uint32_t i1 = face.indices[ti];
                const uint32_t i2 = face.indices[ti + 1];
                if (i0 >= nVerts || i1 >= nVerts || i2 >= nVerts) continue;
                const Vec3 fn = (worldPos[i1]-worldPos[i0]).cross(worldPos[i2]-worldPos[i0]);
                vertNormals[i0] = vertNormals[i0] + fn;
                vertNormals[i1] = vertNormals[i1] + fn;
                vertNormals[i2] = vertNormals[i2] + fn;
            }
        }
        for (auto& n : vertNormals) n = toWorldDir(n.normalize());
    }

    const bool hasTexture = !cfg.texture.empty();
    const auto& uvs       = mesh.attributes().uvs();
    const bool  hasUVs    = !uvs.empty() && uvs.size() == nVerts;

    const std::size_t faceCount = mesh.topology().faceCount();
    for (std::size_t fi = 0; fi < faceCount; ++fi) {
        const auto& face = mesh.topology().face(fi);
        if (face.indices.size() < 3) continue;

        // Fan-triangulate the polygon: (0,1,2), (0,2,3), …
        for (std::size_t ti = 1; ti + 1 < face.indices.size(); ++ti) {
            const uint32_t i0 = face.indices[0];
            const uint32_t i1 = face.indices[ti];
            const uint32_t i2 = face.indices[ti + 1];

            if (i0 >= nVerts || i1 >= nVerts || i2 >= nVerts) continue;

            const Vec3& p0 = worldPos[i0];
            const Vec3& p1 = worldPos[i1];
            const Vec3& p2 = worldPos[i2];

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
            } else if (hasTexture && hasUVs) {
                // Textured path: compute lit colors per-vertex (Gouraud) or per-face (Flat),
                // then modulate by perspective-correct texture sample.
                RGBA8 l0, l1, l2;
                if (cfg.mode == ShadingMode::Gouraud) {
                    l0 = litColor(vertNormals[i0], p0);
                    l1 = litColor(vertNormals[i1], p1);
                    l2 = litColor(vertNormals[i2], p2);
                } else {
                    const Vec3 rawNormal  = (p1 - p0).cross(p2 - p0);
                    const Vec3 faceNormal = toWorldDir(rawNormal.normalize());
                    const Vec3 centroid   = {(p0.x+p1.x+p2.x)/3.f,
                                             (p0.y+p1.y+p2.y)/3.f,
                                             (p0.z+p1.z+p2.z)/3.f};
                    l0 = l1 = l2 = litColor(faceNormal, centroid);
                }
                rasterizeTextured(buf,
                    c0, uvs[i0].u, uvs[i0].v, l0,
                    c1, uvs[i1].u, uvs[i1].v, l1,
                    c2, uvs[i2].u, uvs[i2].v, l2,
                    cfg.texture, cfg.texFilter);
            } else if (cfg.mode == ShadingMode::Gouraud) {
                rasterizeGouraud(buf,
                    c0, litColor(vertNormals[i0], p0),
                    c1, litColor(vertNormals[i1], p1),
                    c2, litColor(vertNormals[i2], p2));
            } else {
                // Flat: face normal from world-space edges; centroid for specular
                const Vec3 rawNormal  = (p1 - p0).cross(p2 - p0);
                const Vec3 faceNormal = toWorldDir(rawNormal.normalize());
                const Vec3 centroid   = {(p0.x+p1.x+p2.x)/3.f,
                                         (p0.y+p1.y+p2.y)/3.f,
                                         (p0.z+p1.z+p2.z)/3.f};
                rasterizeFlat(buf, c0, c1, c2, litColor(faceNormal, centroid));
            }
        }
    }
}

} // namespace nexus::softrast
