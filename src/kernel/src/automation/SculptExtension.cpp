// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Sculpt + BevelChamfer extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/SculptExtension.h>

#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/sculpt/Brush.h>
#include <nexus/sculpt/SculptSession.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
sculptGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = sculptGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = sculptGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerSculptCommands(ScriptBatchHarness& harness)
{
    // ── sculpt.stroke ─────────────────────────────────────────────────────────
    //
    // Applies a single scripted brush stroke to context.mesh via SculptSession.
    // The stroke consists of `samples` evenly-spaced stamps along a line from
    // (ox,oy,oz) to (tx,ty,tz).
    //
    // Arguments:
    //   brush=draw|smooth|inflate|flatten|pinch|crease|layer|grab  (default inflate)
    //   ox/oy/oz=<f>       origin of the stroke (default 0,0,0)
    //   tx/ty/tz=<f>       target / end of the stroke (default 0,1,0)
    //   radius=<f>         brush radius in world space (default 0.5)
    //   strength=<f>       displacement magnitude (default 0.3)
    //   samples=<N>        number of stamps along the stroke (default 4)
    //   falloff=constant|linear|smooth|sharp  (default smooth)
    //   use_vertex_normal=true|false  (default true)
    //   dir_x/y/z=<f>     explicit direction when use_vertex_normal=false (default 0,1,0)
    //
    // On success: "sculpt.stroke ok brush=<B> touched=<N> samples=<S>"
    harness.registry().registerCommand("sculpt.stroke",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("sculpt.stroke requires a loaded mesh");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (ctx.mesh.attributes().positions().empty()) {
                messages.push_back("sculpt.stroke requires a mesh with vertices");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Parse brush kind
            nexus::sculpt::BrushKind kind = nexus::sculpt::BrushKind::Inflate;
            if (const auto v = sculptGetArg(cmd, "brush")) {
                if      (*v == "draw")    kind = nexus::sculpt::BrushKind::Draw;
                else if (*v == "smooth")  kind = nexus::sculpt::BrushKind::Smooth;
                else if (*v == "inflate") kind = nexus::sculpt::BrushKind::Inflate;
                else if (*v == "flatten") kind = nexus::sculpt::BrushKind::Flatten;
                else if (*v == "pinch")   kind = nexus::sculpt::BrushKind::Pinch;
                else if (*v == "crease")  kind = nexus::sculpt::BrushKind::Crease;
                else if (*v == "layer")   kind = nexus::sculpt::BrushKind::Layer;
                else if (*v == "grab")    kind = nexus::sculpt::BrushKind::Grab;
            }

            // Parse falloff
            nexus::sculpt::FalloffShape falloff = nexus::sculpt::FalloffShape::Smooth;
            if (const auto v = sculptGetArg(cmd, "falloff")) {
                if      (*v == "constant") falloff = nexus::sculpt::FalloffShape::Constant;
                else if (*v == "linear")   falloff = nexus::sculpt::FalloffShape::Linear;
                else if (*v == "sharp")    falloff = nexus::sculpt::FalloffShape::Sharp;
            }

            nexus::sculpt::BrushParams params;
            params.radius   = floatArg(cmd, "radius",   0.5f);
            params.strength = floatArg(cmd, "strength", 0.3f);
            params.falloff  = falloff;
            params.direction = {
                floatArg(cmd, "dir_x", 0.f),
                floatArg(cmd, "dir_y", 1.f),
                floatArg(cmd, "dir_z", 0.f)
            };
            if (const auto v = sculptGetArg(cmd, "use_vertex_normal"))
                params.useVertexNormal = (*v != "false" && *v != "0");

            // Ensure normals exist when the brush requires them
            if (params.useVertexNormal &&
                ctx.mesh.attributes().normals().size() !=
                ctx.mesh.attributes().positions().size())
            {
                [[maybe_unused]] bool normOk = ctx.mesh.computeVertexNormals();
            }

            const float ox = floatArg(cmd, "ox", 0.f);
            const float oy = floatArg(cmd, "oy", 0.f);
            const float oz = floatArg(cmd, "oz", 0.f);
            const float tx = floatArg(cmd, "tx", 0.f);
            const float ty = floatArg(cmd, "ty", 1.f);
            const float tz = floatArg(cmd, "tz", 0.f);
            const uint32_t samples = std::max(1u, uintArg(cmd, "samples", 4u));

            // Run the stroke
            nexus::sculpt::SculptSession session(ctx.mesh);
            const nexus::sculpt::StrokeId sid = session.beginStroke(kind, params);
            if (sid == nexus::sculpt::kInvalidStrokeId) {
                messages.push_back("sculpt.stroke failed: beginStroke returned invalid id "
                    "(check radius > 0 and mesh has positions)");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            uint32_t appliedSamples = 0;
            for (uint32_t i = 0; i < samples; ++i) {
                const float t = (samples > 1)
                    ? static_cast<float>(i) / static_cast<float>(samples - 1)
                    : 0.f;
                nexus::sculpt::BrushSample s;
                s.position = {ox + t * (tx - ox),
                               oy + t * (ty - oy),
                               oz + t * (tz - oz)};
                s.pressure = 1.f;
                s.sequence = static_cast<uint64_t>(i);
                if (session.applySample(sid, s)) ++appliedSamples;
            }

            const auto delta = session.endStroke(sid);

            const char* kindName = "inflate";
            switch (kind) {
                case nexus::sculpt::BrushKind::Draw:    kindName = "draw";    break;
                case nexus::sculpt::BrushKind::Smooth:  kindName = "smooth";  break;
                case nexus::sculpt::BrushKind::Inflate: kindName = "inflate"; break;
                case nexus::sculpt::BrushKind::Flatten: kindName = "flatten"; break;
                case nexus::sculpt::BrushKind::Pinch:   kindName = "pinch";   break;
                case nexus::sculpt::BrushKind::Crease:  kindName = "crease";  break;
                case nexus::sculpt::BrushKind::Layer:   kindName = "layer";   break;
                case nexus::sculpt::BrushKind::Grab:    kindName = "grab";    break;
            }

            messages.push_back("sculpt.stroke ok"
                " brush="   + std::string(kindName) +
                " touched=" + std::to_string(delta.touchedVertexCount()) +
                " samples=" + std::to_string(appliedSamples));

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── sculpt.describe ───────────────────────────────────────────────────────
    //
    // Reports basic sculpt-relevant mesh stats: vertex count, position extents
    // before and after the last sculpt stroke (via SculptSession working positions),
    // and a quick displacement magnitude estimate.
    // Informational — always returns true.
    //
    // On success: one or more "sculpt.describe key=value" messages.
    harness.registry().registerCommand("sculpt.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("sculpt.describe no mesh loaded");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            const auto& pos = ctx.mesh.attributes().positions();
            const std::size_t nVerts = pos.size();

            // Compute AABB and mean distance from origin
            float sumDist = 0.f;
            nexus::render::Vec3 bmin = { 1e9f,  1e9f,  1e9f};
            nexus::render::Vec3 bmax = {-1e9f, -1e9f, -1e9f};
            for (const auto& p : pos) {
                bmin.x = std::min(bmin.x, p.x); bmin.y = std::min(bmin.y, p.y);
                bmin.z = std::min(bmin.z, p.z);
                bmax.x = std::max(bmax.x, p.x); bmax.y = std::max(bmax.y, p.y);
                bmax.z = std::max(bmax.z, p.z);
                sumDist += std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
            }
            const float meanDist = nVerts > 0 ? sumDist / static_cast<float>(nVerts) : 0.f;

            auto f2s = [](float v) {
                char buf[32];
                auto [ptr, ec] = std::to_chars(buf, buf+sizeof(buf), v,
                                               std::chars_format::fixed, 4);
                return std::string(buf, ptr);
            };

            messages.push_back("sculpt.describe"
                " vertices=" + std::to_string(nVerts) +
                " faces="    + std::to_string(ctx.mesh.topology().faceCount()));
            messages.push_back("sculpt.describe"
                " bbox=(" + f2s(bmin.x) + "," + f2s(bmin.y) + "," + f2s(bmin.z) +
                ")-("    + f2s(bmax.x) + "," + f2s(bmax.y) + "," + f2s(bmax.z) + ")");
            messages.push_back("sculpt.describe"
                " mean_dist_from_origin=" + f2s(meanDist));

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── mesh.bevel ────────────────────────────────────────────────────────────
    //
    // Applies BevelChamferOperation to context.mesh, softening sharp edges.
    // Replaces context.mesh with the result.
    //
    // Arguments:
    //   mode=bevel|chamfer        (default bevel)
    //   distance=<f>              (default 0.025)
    //   sharp_angle=<f>           (default 30.0 — minimum angle in degrees to qualify as sharp)
    //   include_boundary=true|false (default true)
    //   recompute_normals=true|false (default true)
    //
    // On success: "mesh.bevel ok sharp_edges=N split_edges=M support_faces=F moved_verts=V"
    harness.registry().registerCommand("mesh.bevel",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("mesh.bevel requires a loaded mesh");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::geometry::BevelChamferDesc desc;
            desc.distance             = floatArg(cmd, "distance",    0.025f);
            desc.sharpAngleDegrees    = floatArg(cmd, "sharp_angle", 30.f);
            desc.recomputeNormals     = true;
            desc.includeBoundaryEdges = true;
            desc.mode = nexus::geometry::BevelChamferMode::Bevel;

            if (const auto v = sculptGetArg(cmd, "mode")) {
                if (*v == "chamfer")
                    desc.mode = nexus::geometry::BevelChamferMode::Chamfer;
            }
            if (const auto v = sculptGetArg(cmd, "include_boundary"))
                desc.includeBoundaryEdges = (*v != "false" && *v != "0");
            if (const auto v = sculptGetArg(cmd, "recompute_normals"))
                desc.recomputeNormals = (*v != "false" && *v != "0");

            nexus::geometry::Mesh output;
            const auto report =
                nexus::geometry::BevelChamferOperation::apply(ctx.mesh, desc, output);

            if (!report.isSuccess()) {
                messages.push_back("mesh.bevel failed diagnostic="
                    + std::to_string(static_cast<uint32_t>(report.diagnostic)));
                for (const auto& m : report.messages)
                    messages.push_back("  " + m);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(output);

            for (const auto& m : report.messages)
                messages.push_back("mesh.bevel warning: " + m);

            messages.push_back("mesh.bevel ok"
                " sharp_edges="  + std::to_string(report.sharpEdgeCount)   +
                " split_edges="  + std::to_string(report.splitEdgeCount)   +
                " support_faces="+ std::to_string(report.supportFaceCount) +
                " moved_verts="  + std::to_string(report.movedVertexCount));

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
