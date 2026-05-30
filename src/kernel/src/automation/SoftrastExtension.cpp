// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SoftwareRasterizer extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SoftrastExtension.h>

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
                if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
                if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
                if (p.z < minZ) minZ = p.z; if (p.z > maxZ) maxZ = p.z;
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
}

} // namespace nexus::automation
