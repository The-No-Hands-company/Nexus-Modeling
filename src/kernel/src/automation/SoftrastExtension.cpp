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

    // softrast.render — render context.mesh to a PPM file.
    //
    // Arguments:
    //   output=<path.ppm>      (required)
    //   width=<N>              (default 256)
    //   height=<N>             (default 256)
    //   mode=flat|wireframe    (default flat)
    //   eye_x=<f>              (default 2.0)
    //   eye_y=<f>              (default 2.0)
    //   eye_z=<f>              (default 4.0)
    //   fov=<degrees>          (default 45.0)
    //
    // On success messages include "softrast.render ok <path> <nonbg>/<total> pixels".
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

            // mode=
            nexus::softrast::ShadingMode mode = nexus::softrast::ShadingMode::Flat;
            if (const auto m = softrastGetArg(cmd, "mode")) {
                if (*m == "wireframe") mode = nexus::softrast::ShadingMode::Wireframe;
            }

            nexus::render::Camera cam;
            cam.setPerspective(fov, static_cast<float>(w) / static_cast<float>(h), 0.1f, 100.f);
            cam.lookAt({eyeX, eyeY, eyeZ}, {0.f, 0.f, 0.f});

            nexus::softrast::PixelBuffer buf(w, h);
            nexus::softrast::RasterizerConfig cfg;
            cfg.mode = mode;

            nexus::softrast::SoftwareRasterizer sr;
            sr.render(buf, context.mesh, cam, cfg);

            if (!nexus::softrast::writePPM(*outputArg, buf)) {
                messages.push_back("softrast.render: failed to write PPM to " + *outputArg);
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
