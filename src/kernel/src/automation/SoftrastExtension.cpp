// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SoftwareRasterizer extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SoftrastExtension.h>

#include <nexus/asset/SceneAsset.h>
#include <softrast/ImageWriter.h>
#include <softrast/PixelBuffer.h>
#include <softrast/SoftwareRasterizer.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>

namespace nexus::automation {

namespace {

// ── TRS matrix from AssetTransform ───────────────────────────────────────────

[[nodiscard]] static nexus::render::Mat4 trsMatrix(
    const nexus::asset::AssetTransform& t) noexcept
{
    // Scale
    nexus::render::Mat4 S = nexus::render::Mat4::identity();
    S.m[0][0] = t.scale.x;
    S.m[1][1] = t.scale.y;
    S.m[2][2] = t.scale.z;

    // Rotation (unit quaternion xyzw → rotation matrix, row-major)
    const float qx = t.rotation.x, qy = t.rotation.y;
    const float qz = t.rotation.z, qw = t.rotation.w;
    nexus::render::Mat4 R = nexus::render::Mat4::identity();
    R.m[0][0] = 1.f - 2.f*(qy*qy + qz*qz);
    R.m[0][1] = 2.f*(qx*qy - qz*qw);
    R.m[0][2] = 2.f*(qx*qz + qy*qw);
    R.m[1][0] = 2.f*(qx*qy + qz*qw);
    R.m[1][1] = 1.f - 2.f*(qx*qx + qz*qz);
    R.m[1][2] = 2.f*(qy*qz - qx*qw);
    R.m[2][0] = 2.f*(qx*qz - qy*qw);
    R.m[2][1] = 2.f*(qy*qz + qx*qw);
    R.m[2][2] = 1.f - 2.f*(qx*qx + qy*qy);

    // Translation
    nexus::render::Mat4 T = nexus::render::Mat4::identity();
    T.m[0][3] = t.translation.x;
    T.m[1][3] = t.translation.y;
    T.m[2][3] = t.translation.z;

    return T * R * S; // TRS order
}

// ── Argument helpers (local, not shared with AutomationScript internals) ─────

[[nodiscard]] static std::optional<std::string> softrastGetArg(
    const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return it->second;
}

[[nodiscard]] static std::optional<float> softrastParseFloat(std::string_view sv) {
    float v{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
    return v;
}

[[nodiscard]] static std::optional<uint32_t> softrastParseUint(std::string_view sv) {
    uint32_t v{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
    return v;
}

float floatArg(const ScriptCommand& cmd, std::string_view key, float def) {
    auto s = softrastGetArg(cmd, key);
    if (!s) return def;
    auto v = softrastParseFloat(*s);
    return v.value_or(def);
}

uint32_t uintArg(const ScriptCommand& cmd, std::string_view key, uint32_t def) {
    auto s = softrastGetArg(cmd, key);
    if (!s) return def;
    auto v = softrastParseUint(*s);
    return v.value_or(def);
}

} // namespace

// ── registerSoftrastCommands ─────────────────────────────────────────────────

void registerSoftrastCommands(ScriptBatchHarness& harness) {

    // softrast.render — render context.mesh to a PPM or TGA file.
    //
    // Arguments:
    //   output=<path>          (required; .tga extension selects TGA format)
    //   width=<N>              (default 256)
    //   height=<N>             (default 256)
    //   mode=flat|gouraud|wireframe  (default flat)
    //   eye_x=<f>              (default 2.0)
    //   eye_y=<f>              (default 2.0)
    //   eye_z=<f>              (default 4.0)
    //   fov=<degrees>          (default 45.0)
    //   base_r/g/b=<0-255>     (default 180,180,180 — base mesh color)
    //   bg_r/g/b=<0-255>       (default 30,30,30 — background color)
    //   light_x/y/z=<f>        (default 0.577,0.577,0.577 — light direction, auto-normalized)
    //   wire_r/g/b=<0-255>     (default 220,220,220 — wireframe edge color)
    //   spec_r/g/b=<0-255>     (default 255,255,255 — specular highlight color)
    //   spec_strength=<f>      (default 0.0 — 0=no specular, 1.0=full)
    //   shininess=<f>          (default 32.0 — Blinn-Phong exponent)
    //   texture=checker|uvgrad (optional; requires mesh UVs; checker=checkerboard, uvgrad=UV debug)
    //   tex_size=<N>           (default 64 — texture resolution for procedural textures)
    //   tex_filter=nearest|bilinear (default bilinear)
    //
    // On success messages include "softrast.render ok output=<path> size=WxH nonbg=N/M".
    harness.registry().registerCommand("softrast.render",
        [](ScriptContext& context, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!context.hasMesh) {
                messages.push_back("softrast.render requires a loaded mesh (use mesh.load)");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const auto outputArg = softrastGetArg(cmd, "output");
            if (!outputArg) {
                messages.push_back("softrast.render requires output=<path>");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const uint32_t w   = uintArg(cmd, "width",  256u);
            const uint32_t h   = uintArg(cmd, "height", 256u);
            const float eyeX   = floatArg(cmd, "eye_x", 2.f);
            const float eyeY   = floatArg(cmd, "eye_y", 2.f);
            const float eyeZ   = floatArg(cmd, "eye_z", 4.f);
            const float fov    = floatArg(cmd, "fov",  45.f);

            // mode=flat|gouraud|wireframe
            nexus::softrast::ShadingMode mode = nexus::softrast::ShadingMode::Flat;
            if (const auto m = softrastGetArg(cmd, "mode")) {
                if (*m == "wireframe") mode = nexus::softrast::ShadingMode::Wireframe;
                else if (*m == "gouraud") mode = nexus::softrast::ShadingMode::Gouraud;
            }

            nexus::render::Camera cam;
            cam.setPerspective(fov, static_cast<float>(w) / static_cast<float>(h), 0.1f, 100.f);
            cam.lookAt({eyeX, eyeY, eyeZ}, {0.f, 0.f, 0.f});

            nexus::softrast::PixelBuffer buf(w, h);
            nexus::softrast::RasterizerConfig cfg;
            cfg.mode = mode;

            // Color/light overrides
            cfg.baseColor  = nexus::softrast::RGBA8{
                static_cast<uint8_t>(uintArg(cmd, "base_r", 180u)),
                static_cast<uint8_t>(uintArg(cmd, "base_g", 180u)),
                static_cast<uint8_t>(uintArg(cmd, "base_b", 180u)),
                255
            };
            cfg.background = nexus::softrast::RGBA8{
                static_cast<uint8_t>(uintArg(cmd, "bg_r", 30u)),
                static_cast<uint8_t>(uintArg(cmd, "bg_g", 30u)),
                static_cast<uint8_t>(uintArg(cmd, "bg_b", 30u)),
                255
            };
            cfg.wireColor  = nexus::softrast::RGBA8{
                static_cast<uint8_t>(uintArg(cmd, "wire_r", 220u)),
                static_cast<uint8_t>(uintArg(cmd, "wire_g", 220u)),
                static_cast<uint8_t>(uintArg(cmd, "wire_b", 220u)),
                255
            };
            cfg.specColor  = nexus::softrast::RGBA8{
                static_cast<uint8_t>(uintArg(cmd, "spec_r", 255u)),
                static_cast<uint8_t>(uintArg(cmd, "spec_g", 255u)),
                static_cast<uint8_t>(uintArg(cmd, "spec_b", 255u)),
                255
            };
            cfg.specStrength = floatArg(cmd, "spec_strength", 0.0f);
            cfg.shininess    = floatArg(cmd, "shininess",     32.f);
            cfg.lightDir = nexus::render::Vec3{
                floatArg(cmd, "light_x", 0.577f),
                floatArg(cmd, "light_y", 0.577f),
                floatArg(cmd, "light_z", 0.577f),
            }.normalize();

            // Procedural texture
            if (const auto texArg = softrastGetArg(cmd, "texture")) {
                const uint32_t texSize = uintArg(cmd, "tex_size", 64u);
                if (*texArg == "checker") {
                    cfg.texture = nexus::softrast::Texture2D::makeCheckerboard(texSize);
                } else if (*texArg == "uvgrad") {
                    cfg.texture = nexus::softrast::Texture2D::makeUVGradient(texSize);
                }
                if (const auto flt = softrastGetArg(cmd, "tex_filter")) {
                    if (*flt == "nearest")
                        cfg.texFilter = nexus::softrast::TextureFilter::Nearest;
                }
            }

            nexus::softrast::SoftwareRasterizer sr;
            sr.render(buf, context.mesh, cam, cfg);

            // Select format by extension: .tga → TGA, anything else → PPM
            const bool useTGA = outputArg->size() >= 4 &&
                outputArg->substr(outputArg->size() - 4) == ".tga";
            const bool writeOk = useTGA
                ? nexus::softrast::writeTGA(*outputArg, buf)
                : nexus::softrast::writePPM(*outputArg, buf);
            if (!writeOk) {
                messages.push_back("softrast.render: failed to write image to " + *outputArg);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Count non-background pixels
            const auto& bgCol = cfg.background;
            uint32_t nonBg = 0;
            for (const auto& px : buf.pixels()) {
                if (px.r != bgCol.r || px.g != bgCol.g || px.b != bgCol.b) ++nonBg;
            }
            const uint32_t total = w * h;

            messages.push_back("softrast.render ok output=" + *outputArg
                + " size=" + std::to_string(w) + "x" + std::to_string(h)
                + " nonbg=" + std::to_string(nonBg)
                + "/" + std::to_string(total));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // softrast.describe — report mesh geometry + current render config.
    harness.registry().registerCommand("softrast.describe",
        [](ScriptContext& context, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!context.hasMesh) {
                messages.push_back("softrast.describe no mesh loaded");
                std::sort(messages.begin(), messages.end());
                return true; // not an error — just informational
            }
            const auto& pos = context.mesh.attributes().positions();
            const std::size_t faceCount = context.mesh.topology().faceCount();

            // Compute axis-aligned bounding box
            float minX = 1e30f, minY = 1e30f, minZ = 1e30f;
            float maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;
            for (const auto& p : pos) {
                if (p.x < minX) { minX = p.x; } if (p.x > maxX) { maxX = p.x; }
                if (p.y < minY) { minY = p.y; } if (p.y > maxY) { maxY = p.y; }
                if (p.z < minZ) { minZ = p.z; } if (p.z > maxZ) { maxZ = p.z; }
            }

            auto fmt = [](float f) {
                char buf[32]; std::to_chars(buf, buf+32, f, std::chars_format::fixed, 3);
                return std::string(buf);
            };

            messages.push_back("softrast.describe vertices=" + std::to_string(pos.size())
                + " faces=" + std::to_string(faceCount));
            if (!pos.empty()) {
                messages.push_back("softrast.describe bbox=["
                    + fmt(minX) + "," + fmt(minY) + "," + fmt(minZ) + "] to ["
                    + fmt(maxX) + "," + fmt(maxY) + "," + fmt(maxZ) + "]");
            }
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // softrast.render_scene — render all visible SceneAsset entries into one image.
    //
    // Uses the same camera/mode/color/light args as softrast.render.
    // Each entry is placed using its stored AssetTransform (TRS).
    // Entries are rendered in order (later entries overdraw earlier ones via depth test).
    //
    // Arguments: same as softrast.render; no output= → required.
    // On success: "softrast.render_scene ok output=… size=WxH entries=N nonbg=M/T"
    harness.registry().registerCommand("softrast.render_scene",
        [](ScriptContext& context, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!context.hasScene) {
                messages.push_back("softrast.render_scene requires a loaded scene (use scene.new)");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const auto outputArg = softrastGetArg(cmd, "output");
            if (!outputArg) {
                messages.push_back("softrast.render_scene requires output=<path>");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const uint32_t w   = uintArg(cmd, "width",  256u);
            const uint32_t h   = uintArg(cmd, "height", 256u);
            const float eyeX   = floatArg(cmd, "eye_x", 3.f);
            const float eyeY   = floatArg(cmd, "eye_y", 3.f);
            const float eyeZ   = floatArg(cmd, "eye_z", 5.f);
            const float fov    = floatArg(cmd, "fov",  45.f);

            nexus::softrast::ShadingMode mode = nexus::softrast::ShadingMode::Flat;
            if (const auto m = softrastGetArg(cmd, "mode")) {
                if (*m == "wireframe") mode = nexus::softrast::ShadingMode::Wireframe;
                else if (*m == "gouraud") mode = nexus::softrast::ShadingMode::Gouraud;
            }

            nexus::render::Camera cam;
            cam.setPerspective(fov, static_cast<float>(w) / static_cast<float>(h), 0.1f, 100.f);
            cam.lookAt({eyeX, eyeY, eyeZ}, {0.f, 0.f, 0.f});

            nexus::softrast::RasterizerConfig cfg;
            cfg.mode         = mode;
            cfg.baseColor    = { static_cast<uint8_t>(uintArg(cmd,"base_r",180u)),
                                 static_cast<uint8_t>(uintArg(cmd,"base_g",180u)),
                                 static_cast<uint8_t>(uintArg(cmd,"base_b",180u)), 255 };
            cfg.background   = { static_cast<uint8_t>(uintArg(cmd,"bg_r",30u)),
                                 static_cast<uint8_t>(uintArg(cmd,"bg_g",30u)),
                                 static_cast<uint8_t>(uintArg(cmd,"bg_b",30u)), 255 };
            cfg.wireColor    = { static_cast<uint8_t>(uintArg(cmd,"wire_r",220u)),
                                 static_cast<uint8_t>(uintArg(cmd,"wire_g",220u)),
                                 static_cast<uint8_t>(uintArg(cmd,"wire_b",220u)), 255 };
            cfg.specColor    = { static_cast<uint8_t>(uintArg(cmd,"spec_r",255u)),
                                 static_cast<uint8_t>(uintArg(cmd,"spec_g",255u)),
                                 static_cast<uint8_t>(uintArg(cmd,"spec_b",255u)), 255 };
            cfg.specStrength = floatArg(cmd, "spec_strength", 0.0f);
            cfg.shininess    = floatArg(cmd, "shininess",     32.f);
            cfg.lightDir     = nexus::render::Vec3{
                floatArg(cmd, "light_x", 0.577f),
                floatArg(cmd, "light_y", 0.577f),
                floatArg(cmd, "light_z", 0.577f),
            }.normalize();

            if (const auto texArg = softrastGetArg(cmd, "texture")) {
                const uint32_t texSize = uintArg(cmd, "tex_size", 64u);
                if (*texArg == "checker") {
                    cfg.texture = nexus::softrast::Texture2D::makeCheckerboard(texSize);
                } else if (*texArg == "uvgrad") {
                    cfg.texture = nexus::softrast::Texture2D::makeUVGradient(texSize);
                }
                if (const auto flt = softrastGetArg(cmd, "tex_filter")) {
                    if (*flt == "nearest")
                        cfg.texFilter = nexus::softrast::TextureFilter::Nearest;
                }
            }

            nexus::softrast::PixelBuffer buf(w, h);
            buf.clear(cfg.background);

            nexus::softrast::SoftwareRasterizer sr;
            uint32_t entryCount = 0;
            const std::size_t n = context.scene.entryCount();
            for (std::size_t i = 0; i < n; ++i) {
                const auto& entry = context.scene.entry(i);
                if (!entry.visible || entry.mesh.attributes().positions().empty()) continue;
                const nexus::render::Mat4 model = trsMatrix(entry.transform);
                sr.renderInto(buf, entry.mesh, cam, cfg, model);
                ++entryCount;
            }

            const bool useTGA = outputArg->size() >= 4 &&
                outputArg->substr(outputArg->size() - 4) == ".tga";
            const bool writeOk = useTGA
                ? nexus::softrast::writeTGA(*outputArg, buf)
                : nexus::softrast::writePPM(*outputArg, buf);
            if (!writeOk) {
                messages.push_back("softrast.render_scene: failed to write image to " + *outputArg);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const auto& bgCol = cfg.background;
            uint32_t nonBg = 0;
            for (const auto& px : buf.pixels()) {
                if (px.r != bgCol.r || px.g != bgCol.g || px.b != bgCol.b) ++nonBg;
            }
            const uint32_t total = w * h;

            messages.push_back("softrast.render_scene ok output=" + *outputArg
                + " size=" + std::to_string(w) + "x" + std::to_string(h)
                + " entries=" + std::to_string(entryCount)
                + " nonbg=" + std::to_string(nonBg) + "/" + std::to_string(total));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
