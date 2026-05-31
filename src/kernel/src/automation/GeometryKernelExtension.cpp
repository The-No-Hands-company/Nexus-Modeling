// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — GeometryKernel, MeshIO, Meshlet, ModelingShell
//  extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/GeometryKernelExtension.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/geometry/Meshlet.h>
#include <nexus/geometry/ModelingShell.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
gkGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = gkGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

struct GeometryKernelState {
    nexus::geometry::ModelingShell shell;
    bool hasSession = false;
    std::vector<uint32_t> lastIndexBuffer;
    nexus::geometry::MeshletGroup lastMeshlets;
    bool hasMeshlets = false;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerGeometryKernelCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<GeometryKernelState>();

    // ── geom_kernel.triangulate ───────────────────────────────────────────────
    //
    // Builds a triangulated index buffer from ctx.mesh via GeometryKernel.
    //
    // On success: "geom_kernel.triangulate ok triangles=<N>"
    // On error:   "geom_kernel.triangulate error: ..."
    harness.registry().registerCommand("geom_kernel.triangulate",
        [state](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto report =
                nexus::geometry::MeshUploadContract::buildTriangulatedIndexBufferWithReport(ctx.mesh);

            if (!report.valid) {
                messages.push_back("geom_kernel.triangulate error: triangulation failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->lastIndexBuffer = report.indices;
            messages.push_back("geom_kernel.triangulate ok"
                " triangles=" + std::to_string(report.triangleCount));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── geom_kernel.validate ──────────────────────────────────────────────────
    //
    // Validates topology of ctx.mesh via GeometryKernel::validateTopology.
    //
    // On success: "geom_kernel.validate ok issues=<N>"
    harness.registry().registerCommand("geom_kernel.validate",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const auto report =
                nexus::geometry::TopologyUtilities::validateTopology(
                    ctx.mesh.topology(), ctx.mesh.attributes().vertexCount());

            messages.push_back("geom_kernel.validate ok"
                " issues=" + std::to_string(report.issues.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── geom_kernel.weld ──────────────────────────────────────────────────────
    //
    // Welds coincident vertices in ctx.mesh.
    //
    // Arguments:
    //   epsilon=<f>   weld tolerance (default 1e-5)
    //
    // On success: "geom_kernel.weld ok"
    // On error:   "geom_kernel.weld error: weld failed"
    harness.registry().registerCommand("geom_kernel.weld",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            nexus::geometry::GeometryCommandDesc desc{};
            desc.type        = nexus::geometry::GeometryCommandType::WeldCoincidentVertices;
            desc.weldEpsilon = floatArg(cmd, "epsilon", 1e-5f);

            const auto report =
                nexus::geometry::GeometryCommandSurface::execute(ctx.mesh, desc);

            if (!report.valid) {
                messages.push_back("geom_kernel.weld error: weld failed");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("geom_kernel.weld ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── mesh_io.export ────────────────────────────────────────────────────────
    //
    // Exports ctx.mesh to a file via MeshIO::exportMesh.
    //
    // Arguments:
    //   path=<P>         output file path (required)
    //   format=OBJ|PLY   export format    (default OBJ)
    //
    // On success: "mesh_io.export ok path=<P>"
    // On error:   "mesh_io.export error: ..."
    harness.registry().registerCommand("mesh_io.export",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const auto pathArg = gkGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("mesh_io.export error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            nexus::geometry::MeshExportOptions opts{};
            const auto fmtArg = gkGetArg(cmd, "format");
            if (fmtArg && *fmtArg == "PLY")
                opts.format = nexus::geometry::MeshExportFormat::PLY;

            const auto report = nexus::geometry::MeshIO::exportMesh(ctx.mesh, path, opts);
            if (!report.valid) {
                messages.push_back("mesh_io.export error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("mesh_io.export ok path=" + path);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── mesh_io.import ────────────────────────────────────────────────────────
    //
    // Imports a mesh from a file into ctx.mesh via MeshIO::importMesh.
    //
    // Arguments:
    //   path=<P>   input file path (required)
    //
    // On success: "mesh_io.import ok path=<P>"
    // On error:   "mesh_io.import error: ..."
    harness.registry().registerCommand("mesh_io.import",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const auto pathArg = gkGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("mesh_io.import error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            nexus::geometry::MeshImportOptions opts{};
            nexus::geometry::Mesh out;
            const auto report = nexus::geometry::MeshIO::importMesh(path, out, opts);
            if (!report.valid) {
                messages.push_back("mesh_io.import error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.mesh = std::move(out);
            messages.push_back("mesh_io.import ok path=" + path);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── meshlet.build ─────────────────────────────────────────────────────────
    //
    // Builds a MeshletGroup from the last triangulated index buffer + ctx.mesh
    // positions.
    //
    // Arguments:
    //   verts_per=<N>      target vertices per meshlet (default 64)
    //   prims_per=<N>      target primitives per meshlet (default 81)
    //
    // On success: "meshlet.build ok meshlets=<N>"
    // On error:   "meshlet.build error: ..."
    harness.registry().registerCommand("meshlet.build",
        [state](ScriptContext& ctx, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            if (state->lastIndexBuffer.empty()) {
                messages.push_back("meshlet.build error: no index buffer (run geom_kernel.triangulate first)");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::geometry::MeshletBuilderConfig cfg{};
            {
                const auto v = gkGetArg(cmd, "verts_per");
                if (v) {
                    uint32_t n = 64;
                    std::from_chars(v->data(), v->data() + v->size(), n);
                    cfg.targetVerticesPerMeshlet = n;
                }
            }
            {
                const auto v = gkGetArg(cmd, "prims_per");
                if (v) {
                    uint32_t n = 81;
                    std::from_chars(v->data(), v->data() + v->size(), n);
                    cfg.targetPrimitivesPerMeshlet = n;
                }
            }

            const auto& positions = ctx.mesh.attributes().positions();
            state->lastMeshlets = nexus::geometry::buildMeshletsFromMesh(
                std::span<const nexus::render::Vec3>(positions.data(), positions.size()),
                std::span<const uint32_t>(state->lastIndexBuffer.data(),
                                          state->lastIndexBuffer.size()),
                cfg);
            state->hasMeshlets = true;

            messages.push_back("meshlet.build ok"
                " meshlets=" + std::to_string(state->lastMeshlets.meshlets.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── meshlet.describe ──────────────────────────────────────────────────────
    //
    // Reports meshlet state.  Always returns true.
    //
    // "meshlet.describe has_meshlets=<0|1> count=<N>"
    harness.registry().registerCommand("meshlet.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("meshlet.describe"
                " has_meshlets=" + std::string(state->hasMeshlets ? "1" : "0") +
                " count=" + std::to_string(state->hasMeshlets
                    ? state->lastMeshlets.meshlets.size() : 0u));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── modeling.start ────────────────────────────────────────────────────────
    //
    // Initialises a ModelingShell session with a starter primitive.
    //
    // Arguments:
    //   primitive=Box|Plane|Sphere|Cylinder|Cone|Capsule   (default Box)
    //   size=<f>                                           (default 1.0)
    //   label=<S>                                          session label
    //
    // On success: "modeling.start ok primitive=<P>"
    harness.registry().registerCommand("modeling.start",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto primStr = gkGetArg(cmd, "primitive");
            const float size   = floatArg(cmd, "size", 1.0f);
            const auto labelArg = gkGetArg(cmd, "label");

            nexus::geometry::StarterPrimitive prim =
                nexus::geometry::StarterPrimitive::Box;
            std::string primName = "Box";
            if (primStr) {
                if (*primStr == "Plane")    { prim = nexus::geometry::StarterPrimitive::Plane;    primName = "Plane"; }
                if (*primStr == "Sphere")   { prim = nexus::geometry::StarterPrimitive::Sphere;   primName = "Sphere"; }
                if (*primStr == "Cylinder") { prim = nexus::geometry::StarterPrimitive::Cylinder; primName = "Cylinder"; }
                if (*primStr == "Cone")     { prim = nexus::geometry::StarterPrimitive::Cone;     primName = "Cone"; }
                if (*primStr == "Capsule")  { prim = nexus::geometry::StarterPrimitive::Capsule;  primName = "Capsule"; }
            }

            if (labelArg)
                state->shell.setSessionLabel(std::string(labelArg->data(), labelArg->size()));

            state->shell.startStarterModel(prim, size);
            state->hasSession = true;

            messages.push_back("modeling.start ok primitive=" + primName);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── modeling.quick_cleanup ────────────────────────────────────────────────
    //
    // Runs ModelingShell::quickCleanup on the active session.
    //
    // On success: "modeling.quick_cleanup ok"
    // On error:   "modeling.quick_cleanup error: no session"
    harness.registry().registerCommand("modeling.quick_cleanup",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasSession) {
                messages.push_back("modeling.quick_cleanup error: no session");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            state->shell.quickCleanup();
            messages.push_back("modeling.quick_cleanup ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── modeling.refresh_diagnostics ─────────────────────────────────────────
    //
    // Refreshes the diagnostic overlay of the active session.
    //
    // On success: "modeling.refresh_diagnostics ok ready=<0|1>"
    // On error:   "modeling.refresh_diagnostics error: no session"
    harness.registry().registerCommand("modeling.refresh_diagnostics",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            if (!state->hasSession) {
                messages.push_back("modeling.refresh_diagnostics error: no session");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            [[maybe_unused]] const bool ok = state->shell.refreshDiagnostics();
            messages.push_back("modeling.refresh_diagnostics ok"
                " ready=" + std::string(state->shell.isReady() ? "1" : "0"));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── modeling.session_report ───────────────────────────────────────────────
    //
    // Returns a summary of the current ModelingShell session.
    //
    // "modeling.session_report success=<0|1> steps=<N> label=<L>"
    harness.registry().registerCommand("modeling.session_report",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto report = state->shell.sessionReport();
            messages.push_back("modeling.session_report"
                " success=" + std::string(report.success ? "1" : "0") +
                " steps="   + std::to_string(report.workflowSteps.size()) +
                " label="   + (report.sessionLabel.empty() ? "(none)" : report.sessionLabel));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── modeling.guided_intro ─────────────────────────────────────────────────
    //
    // Returns the guided intro step count.  Always returns true.
    //
    // "modeling.guided_intro steps=<N>"
    harness.registry().registerCommand("modeling.guided_intro",
        [](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            const auto steps = nexus::geometry::ModelingShell::guidedIntroSteps();
            messages.push_back("modeling.guided_intro"
                " steps=" + std::to_string(steps.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
