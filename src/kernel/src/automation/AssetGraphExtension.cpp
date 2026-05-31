// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — AssetDependencyGraph + text adapter extension commands
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AssetGraphExtension.h>
#include <nexus/asset/AssetDependencyGraph.h>
#include <nexus/asset/SceneAsset.h>
#include <nexus/asset/SceneAssetImporter.h>
#include <nexus/asset/SceneAssetTextAdapter.h>

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
agGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

struct AssetGraphState {
    nexus::asset::AssetDependencyGraph graph;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerAssetGraphCommands(ScriptBatchHarness& harness)
{
    auto state = std::make_shared<AssetGraphState>();

    // ── asset_dep.add ─────────────────────────────────────────────────────────
    //
    // Registers an asset node in the dependency graph.
    //
    // Arguments:
    //   path=<P>   unique path key (required)
    //   name=<N>   optional human-readable label
    //
    // On success: "asset_dep.add ok path=<P> count=<C>"
    // On error:   "asset_dep.add error: ..."
    harness.registry().registerCommand("asset_dep.add",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto pathArg = agGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("asset_dep.add error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const auto nameArg = agGetArg(cmd, "name");

            nexus::asset::AssetNodeDesc desc;
            desc.path = std::string(pathArg->data(), pathArg->size());
            desc.name = nameArg ? std::string(nameArg->data(), nameArg->size()) : desc.path;

            const bool ok = state->graph.addAsset(std::move(desc));
            if (!ok) {
                messages.push_back("asset_dep.add error: path empty or already registered");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("asset_dep.add ok"
                " path="  + std::string(*pathArg) +
                " count=" + std::to_string(state->graph.assetCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset_dep.add_dep ─────────────────────────────────────────────────────
    //
    // Adds a directed dependency: path depends on depends_on.
    //
    // Arguments:
    //   path=<P>        asset that has the dependency
    //   depends_on=<D>  asset being depended upon
    //
    // On success: "asset_dep.add_dep ok path=<P> depends_on=<D>"
    // On error:   "asset_dep.add_dep error: ..."
    harness.registry().registerCommand("asset_dep.add_dep",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto pathArg = agGetArg(cmd, "path");
            const auto depArg  = agGetArg(cmd, "depends_on");
            if (!pathArg || !depArg) {
                messages.push_back("asset_dep.add_dep error: missing path or depends_on");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());
            const std::string dep(depArg->data(), depArg->size());

            const bool ok = state->graph.addDependency(path, dep);
            if (!ok) {
                messages.push_back("asset_dep.add_dep error:"
                    " unknown path, same path, or edge already exists");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("asset_dep.add_dep ok"
                " path="       + path +
                " depends_on=" + dep);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset_dep.remove ──────────────────────────────────────────────────────
    //
    // Removes a registered asset node.
    //
    // Arguments:
    //   path=<P>   path to remove
    //
    // On success: "asset_dep.remove ok path=<P> count=<C>"
    // On error:   "asset_dep.remove error: not found"
    harness.registry().registerCommand("asset_dep.remove",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& cmd,
                std::vector<std::string>& messages) -> bool {
            const auto pathArg = agGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("asset_dep.remove error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            const bool ok = state->graph.removeAsset(path);
            if (!ok) {
                messages.push_back("asset_dep.remove error: not found");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("asset_dep.remove ok"
                " path="  + path +
                " count=" + std::to_string(state->graph.assetCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset_dep.resolve ─────────────────────────────────────────────────────
    //
    // Resolves a dependency-first topological load order.
    //
    // On success: "asset_dep.resolve ok status=Success count=<N>"
    // On cycle:   "asset_dep.resolve ok status=CycleDetected count=<N>"
    // On missing: "asset_dep.resolve ok status=MissingDependency count=<N>"
    harness.registry().registerCommand("asset_dep.resolve",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            const auto report = state->graph.resolveLoadOrder();

            std::string statusStr;
            switch (report.status) {
                case nexus::asset::DependencyResolutionStatus::Success:
                    statusStr = "Success"; break;
                case nexus::asset::DependencyResolutionStatus::CycleDetected:
                    statusStr = "CycleDetected"; break;
                case nexus::asset::DependencyResolutionStatus::MissingDependency:
                    statusStr = "MissingDependency"; break;
            }

            messages.push_back("asset_dep.resolve ok"
                " status=" + statusStr +
                " count="  + std::to_string(report.loadOrder.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset_dep.clear ───────────────────────────────────────────────────────
    //
    // Removes all nodes and edges from the dependency graph.
    //
    // On success: "asset_dep.clear ok"
    harness.registry().registerCommand("asset_dep.clear",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            state->graph.clear();
            messages.push_back("asset_dep.clear ok");
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset_dep.describe ────────────────────────────────────────────────────
    //
    // Reports graph state.  Always returns true.
    //
    // "asset_dep.describe count=<N>"
    harness.registry().registerCommand("asset_dep.describe",
        [state](ScriptContext& /*ctx*/, const ScriptCommand& /*cmd*/,
                std::vector<std::string>& messages) -> bool {
            messages.push_back("asset_dep.describe"
                " count=" + std::to_string(state->graph.assetCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene_text.export ─────────────────────────────────────────────────────
    //
    // Exports ctx.scene to a text file via SceneAssetTextAdapter.
    //
    // Arguments:
    //   path=<P>   output file path
    //
    // On success: "scene_text.export ok path=<P>"
    // On error:   "scene_text.export error: ..."
    harness.registry().registerCommand("scene_text.export",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const auto pathArg = agGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("scene_text.export error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            const auto report =
                nexus::asset::SceneAssetTextAdapter::exportScene(ctx.scene, path);

            if (!report.isSuccess()) {
                messages.push_back("scene_text.export error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            messages.push_back("scene_text.export ok path=" + path);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene_text.import ─────────────────────────────────────────────────────
    //
    // Imports a scene from a text file into ctx.scene via SceneAssetTextAdapter.
    //
    // Arguments:
    //   path=<P>   input file path
    //
    // On success: "scene_text.import ok path=<P>"
    // On error:   "scene_text.import error: ..."
    harness.registry().registerCommand("scene_text.import",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const auto pathArg = agGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("scene_text.import error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            nexus::asset::SceneAsset out;
            const auto report =
                nexus::asset::SceneAssetTextAdapter::importScene(path, out);

            if (!report.isSuccess()) {
                messages.push_back("scene_text.import error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.scene    = std::move(out);
            ctx.hasScene = true;
            messages.push_back("scene_text.import ok path=" + path);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── scene_importer.import ─────────────────────────────────────────────────
    //
    // Imports a single scene file into ctx.scene via SceneAssetImporter.
    //
    // Arguments:
    //   path=<P>   input file path
    //
    // On success: "scene_importer.import ok path=<P>"
    // On error:   "scene_importer.import error: ..."
    harness.registry().registerCommand("scene_importer.import",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const auto pathArg = agGetArg(cmd, "path");
            if (!pathArg) {
                messages.push_back("scene_importer.import error: missing path");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            const std::string path(pathArg->data(), pathArg->size());

            nexus::asset::SceneAsset out;
            const auto report =
                nexus::asset::SceneAssetImporter::importScene(path, out);

            if (!report.isSuccess()) {
                messages.push_back("scene_importer.import error: diagnostic=" +
                    std::to_string(static_cast<uint32_t>(report.diagnostic)));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.scene    = std::move(out);
            ctx.hasScene = true;
            messages.push_back("scene_importer.import ok path=" + path);
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
