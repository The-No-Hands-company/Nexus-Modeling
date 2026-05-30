// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Geometry operation extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/GeometryExtension.h>

#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/InsetFacesOperation.h>
#include <nexus/geometry/Mesh.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
geoGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = geoGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerGeometryCommands(ScriptBatchHarness& harness)
{
    // ── mesh.extrude ──────────────────────────────────────────────────────────
    //
    // Extrudes every face of context.mesh outward along its face normal.
    // Replaces context.mesh with the result.
    //
    // Arguments:
    //   distance=<f>               (default 0.1 — world-space extrusion depth)
    //   keep_original=true|false   (default false — keep original base cap faces)
    //   recompute_normals=true|false (default true)
    //
    // On success: "mesh.extrude ok extruded=N added_faces=M added_verts=V"
    harness.registry().registerCommand("mesh.extrude",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("mesh.extrude requires a loaded mesh");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::geometry::ExtrudeDesc desc;
            desc.distance           = floatArg(cmd, "distance", 0.1f);
            desc.recomputeNormals   = true;
            desc.keepOriginalFaces  = false;

            if (const auto v = geoGetArg(cmd, "keep_original"))
                desc.keepOriginalFaces = (*v == "true" || *v == "1");
            if (const auto v = geoGetArg(cmd, "recompute_normals"))
                desc.recomputeNormals = (*v != "false" && *v != "0");

            nexus::geometry::Mesh output;
            const auto report =
                nexus::geometry::ExtrudeOperation::applyToAllFaces(ctx.mesh, desc, output);

            if (!report.isSuccess()) {
                messages.push_back("mesh.extrude failed diagnostic="
                    + std::to_string(static_cast<uint32_t>(report.diagnostic)));
                for (const auto& m : report.messages)
                    messages.push_back("  " + m);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(output);

            for (const auto& m : report.messages)
                messages.push_back("mesh.extrude warning: " + m);

            messages.push_back("mesh.extrude ok"
                " extruded="    + std::to_string(report.extrudedFaceCount) +
                " added_faces=" + std::to_string(report.addedFaceCount)    +
                " added_verts=" + std::to_string(report.addedVertexCount));

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── mesh.inset ────────────────────────────────────────────────────────────
    //
    // Insets every face of context.mesh toward its centroid.
    // Replaces context.mesh with the result.
    //
    // Arguments:
    //   amount=<f>                  (default 0.1)
    //   mode=factor|distance        (default factor — amount as fraction [0,1])
    //   replace_original=true|false (default true — replace original faces)
    //   recompute_normals=true|false (default true)
    //
    // On success: "mesh.inset ok inset=N added_faces=M added_verts=V"
    harness.registry().registerCommand("mesh.inset",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("mesh.inset requires a loaded mesh");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::geometry::InsetDesc desc;
            desc.amount           = floatArg(cmd, "amount", 0.1f);
            desc.recomputeNormals = true;
            desc.replaceOriginal  = true;
            desc.mode             = nexus::geometry::InsetMode::Factor;

            if (const auto v = geoGetArg(cmd, "mode")) {
                if (*v == "distance")
                    desc.mode = nexus::geometry::InsetMode::Distance;
            }
            if (const auto v = geoGetArg(cmd, "replace_original"))
                desc.replaceOriginal = (*v != "false" && *v != "0");
            if (const auto v = geoGetArg(cmd, "recompute_normals"))
                desc.recomputeNormals = (*v != "false" && *v != "0");

            nexus::geometry::Mesh output;
            const auto report =
                nexus::geometry::InsetFacesOperation::applyToAllFaces(ctx.mesh, desc, output);

            if (!report.isSuccess()) {
                messages.push_back("mesh.inset failed diagnostic="
                    + std::to_string(static_cast<uint32_t>(report.diagnostic)));
                for (const auto& m : report.messages)
                    messages.push_back("  " + m);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(output);

            for (const auto& m : report.messages)
                messages.push_back("mesh.inset warning: " + m);

            messages.push_back("mesh.inset ok"
                " inset="       + std::to_string(report.insetFaceCount)   +
                " added_faces=" + std::to_string(report.addedFaceCount)   +
                " added_verts=" + std::to_string(report.addedVertexCount));

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
