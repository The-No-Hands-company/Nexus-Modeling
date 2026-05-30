// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — Remesh/mesh-processing extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/RemeshExtension.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/RemeshOperation.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>

namespace nexus::automation {

namespace {

// ── Argument helpers (mirror of SoftrastExtension helpers) ───────────────────

[[nodiscard]] static std::optional<std::string_view>
remeshGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = remeshGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = remeshGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerRemeshCommands(ScriptBatchHarness& harness)
{
    // ── mesh.remesh ───────────────────────────────────────────────────────────
    //
    // Applies RemeshOperation (isotropic edge-split + optional collapse) to
    // context.mesh and stores the result back into context.mesh.
    //
    // Arguments:
    //   target_edge_length=<f>        (default 0.25)
    //   max_iterations=<N>            (default 2)
    //   split_threshold=<f>           (default 1.25 — edges > target*threshold are split)
    //   collapse_below=<f>            (default 0.0 — 0 disables collapse pass)
    //   max_collapse_iterations=<N>   (default 2)
    //   recompute_normals=true|false  (default true)
    //
    // On success: "mesh.remesh ok faces_in=N faces_out=M splits=S collapses=C iters=I"
    harness.registry().registerCommand("mesh.remesh",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("mesh.remesh requires a loaded mesh");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::geometry::RemeshDesc desc;
            desc.targetEdgeLength       = floatArg(cmd, "target_edge_length", 0.25f);
            desc.maxIterations          = uintArg(cmd,  "max_iterations",     2u);
            desc.splitThresholdMultiplier
                                        = floatArg(cmd, "split_threshold",    1.25f);
            desc.collapseEdgesBelow     = floatArg(cmd, "collapse_below",     0.0f);
            desc.maxCollapseIterations  = uintArg(cmd,  "max_collapse_iterations", 2u);

            if (const auto v = remeshGetArg(cmd, "recompute_normals"))
                desc.recomputeNormals = (*v != "false" && *v != "0");

            nexus::geometry::Mesh output;
            const auto report = nexus::geometry::RemeshOperation::apply(ctx.mesh, desc, output);

            if (!report.isSuccess()) {
                messages.push_back("mesh.remesh failed diagnostic="
                    + std::to_string(static_cast<uint32_t>(report.diagnostic)));
                for (const auto& m : report.messages) messages.push_back("  " + m);
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Promote remeshed mesh into context
            ctx.mesh = std::move(output);

            // Propagate any warnings from the report
            for (const auto& m : report.messages)
                messages.push_back("mesh.remesh warning: " + m);

            messages.push_back("mesh.remesh ok"
                " faces_in="    + std::to_string(report.inputFaceCount)  +
                " faces_out="   + std::to_string(report.outputFaceCount) +
                " splits="      + std::to_string(report.splitCount)      +
                " collapses="   + std::to_string(report.collapseCount)   +
                " iters="       + std::to_string(report.iterationsRan));

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── mesh.weld ─────────────────────────────────────────────────────────────
    //
    // Welds coincident vertices in context.mesh (in-place, within epsilon).
    //
    // Arguments:
    //   epsilon=<f>  (default 1e-5)
    //
    // On success: "mesh.weld ok verts_before=N verts_after=M"
    harness.registry().registerCommand("mesh.weld",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("mesh.weld requires a loaded mesh");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const float epsilon = floatArg(cmd, "epsilon", 1e-5f);
            const uint32_t before =
                static_cast<uint32_t>(ctx.mesh.attributes().positions().size());

            [[maybe_unused]] bool welded = ctx.mesh.weldCoincidentVertices(epsilon);

            const uint32_t after =
                static_cast<uint32_t>(ctx.mesh.attributes().positions().size());

            messages.push_back("mesh.weld ok"
                " verts_before=" + std::to_string(before) +
                " verts_after="  + std::to_string(after));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── mesh.describe ─────────────────────────────────────────────────────────
    //
    // Reports vertex count, face count, edge-length stats, and whether the mesh
    // has normals/UVs/tangents. Informational — always returns true.
    //
    // On success: multi-line messages with "key=value" pairs.
    harness.registry().registerCommand("mesh.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasMesh) {
                messages.push_back("mesh.describe no mesh loaded");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            const auto& pos   = ctx.mesh.attributes().positions();
            const std::size_t verts = pos.size();
            const std::size_t faces = ctx.mesh.topology().faceCount();

            // Compute bounding box and average edge length estimate
            nexus::render::Vec3 bmin = {  1e9f,  1e9f,  1e9f};
            nexus::render::Vec3 bmax = { -1e9f, -1e9f, -1e9f};
            for (const auto& p : pos) {
                if (p.x < bmin.x) bmin.x = p.x;
                if (p.y < bmin.y) bmin.y = p.y;
                if (p.z < bmin.z) bmin.z = p.z;
                if (p.x > bmax.x) bmax.x = p.x;
                if (p.y > bmax.y) bmax.y = p.y;
                if (p.z > bmax.z) bmax.z = p.z;
            }

            // Mean edge length from first up-to-1000 triangles (fast estimate)
            double edgeSum = 0.0;
            uint32_t edgeCount = 0;
            for (std::size_t fi = 0; fi < faces && edgeCount < 3000; ++fi) {
                const auto& face = ctx.mesh.topology().face(fi);
                if (face.indices.size() < 3) continue;
                for (std::size_t ti = 1; ti + 1 < face.indices.size() && edgeCount < 3000; ++ti) {
                    const auto& p0 = pos[face.indices[0]];
                    const auto& p1 = pos[face.indices[ti]];
                    const auto& p2 = pos[face.indices[ti+1]];
                    auto len = [](const nexus::render::Vec3& a, const nexus::render::Vec3& b) {
                        const float dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
                        return std::sqrt(dx*dx + dy*dy + dz*dz);
                    };
                    edgeSum += len(p0, p1) + len(p1, p2) + len(p2, p0);
                    edgeCount += 3;
                }
            }
            const float meanEdge = edgeCount > 0
                ? static_cast<float>(edgeSum / edgeCount) : 0.f;

            auto f2s = [](float v) {
                char buf[32];
                auto [ptr, ec] = std::to_chars(buf, buf+sizeof(buf), v,
                                               std::chars_format::fixed, 4);
                return std::string(buf, ptr);
            };

            messages.push_back("mesh.describe"
                " faces="    + std::to_string(faces)  +
                " vertices=" + std::to_string(verts));
            messages.push_back("mesh.describe"
                " bbox_min=(" + f2s(bmin.x) + "," + f2s(bmin.y) + "," + f2s(bmin.z) + ")"
                " bbox_max=(" + f2s(bmax.x) + "," + f2s(bmax.y) + "," + f2s(bmax.z) + ")");
            messages.push_back("mesh.describe"
                " mean_edge_len=" + f2s(meanEdge));
            messages.push_back("mesh.describe"
                " has_normals="  + std::string(ctx.mesh.attributes().hasNormals()  ? "true"  : "false") +
                " has_uvs="      + std::string(ctx.mesh.attributes().hasUVs()      ? "true"  : "false") +
                " has_tangents=" + std::string(ctx.mesh.attributes().hasTangents() ? "true"  : "false"));

            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
