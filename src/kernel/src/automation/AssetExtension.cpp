// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Automation — SceneAsset + GaussianSplatCloud extension commands
//  (compiled into nexus_automation_ext, not nexus_gfx_core)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/automation/AssetExtension.h>
#include <nexus/asset/SceneAsset.h>
#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/geometry/Mesh.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::automation {

namespace {

[[nodiscard]] static std::optional<std::string_view>
assetGetArg(const ScriptCommand& cmd, std::string_view key)
{
    auto it = cmd.args.find(std::string(key));
    if (it == cmd.args.end()) return std::nullopt;
    return std::string_view(it->second);
}

[[nodiscard]] static float floatArg(const ScriptCommand& cmd,
                                    std::string_view key, float def)
{
    if (const auto v = assetGetArg(cmd, key)) {
        float out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

[[nodiscard]] static uint32_t uintArg(const ScriptCommand& cmd,
                                      std::string_view key, uint32_t def)
{
    if (const auto v = assetGetArg(cmd, key)) {
        uint32_t out = def;
        std::from_chars(v->data(), v->data() + v->size(), out);
        return out;
    }
    return def;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

void registerAssetCommands(ScriptBatchHarness& harness)
{
    // ═════════════════════════════════════════════════════════════════════════
    //  SceneAsset
    // ═════════════════════════════════════════════════════════════════════════

    // ── asset.new ─────────────────────────────────────────────────────────────
    //
    // Resets ctx.scene to a fresh empty SceneAsset with an optional name.
    //
    // Arguments:
    //   name=<str>   scene name (default "scene")
    //
    // On success: "asset.new ok name=<N>"
    harness.registry().registerCommand("asset.new",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            std::string name = "scene";
            if (const auto v = assetGetArg(cmd, "name"))
                name = std::string(*v);

            ctx.scene = nexus::asset::SceneAsset();
            ctx.scene.setSceneName(name);
            ctx.sceneName  = name;
            ctx.hasScene   = true;

            messages.push_back("asset.new ok name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset.add_entry ───────────────────────────────────────────────────────
    //
    // Appends a named mesh entry (empty geometry) to ctx.scene.
    //
    // Arguments:
    //   name=<str>         entry name (default "entry")
    //   visible=<0|1>      visibility flag (default 1)
    //   tx=<f> ty=<f> tz=<f>  translation (default 0 0 0)
    //
    // On success: "asset.add_entry ok index=<I> name=<N>"
    // On error:   "asset.add_entry error: scene not initialized"
    harness.registry().registerCommand("asset.add_entry",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasScene) {
                messages.push_back("asset.add_entry error: scene not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string name = "entry";
            if (const auto v = assetGetArg(cmd, "name"))
                name = std::string(*v);

            const uint32_t vis = uintArg(cmd, "visible", 1u);

            nexus::asset::SceneMeshEntry entry;
            entry.name    = name;
            entry.visible = (vis != 0u);
            entry.transform.translation = {floatArg(cmd, "tx", 0.f),
                                           floatArg(cmd, "ty", 0.f),
                                           floatArg(cmd, "tz", 0.f)};

            const size_t index = ctx.scene.entryCount();
            ctx.scene.addEntry(std::move(entry));

            messages.push_back("asset.add_entry ok"
                " index=" + std::to_string(index) +
                " name="  + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset.remove_entry ────────────────────────────────────────────────────
    //
    // Removes the entry at the given index from ctx.scene.
    //
    // Arguments:
    //   index=<N>   entry index (required)
    //
    // On success: "asset.remove_entry ok index=<I> remaining=<R>"
    // On error:   "asset.remove_entry error: ..."
    harness.registry().registerCommand("asset.remove_entry",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasScene) {
                messages.push_back("asset.remove_entry error: scene not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            const uint32_t index = uintArg(cmd, "index", UINT32_MAX);
            if (index == UINT32_MAX || static_cast<size_t>(index) >= ctx.scene.entryCount()) {
                messages.push_back("asset.remove_entry error: index out of range: " +
                    std::to_string(index));
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.scene.removeEntry(static_cast<size_t>(index));

            messages.push_back("asset.remove_entry ok"
                " index="     + std::to_string(index) +
                " remaining=" + std::to_string(ctx.scene.entryCount()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset.set_name ────────────────────────────────────────────────────────
    //
    // Sets the scene name on ctx.scene.
    //
    // Arguments:
    //   name=<str>   new scene name (required)
    //
    // On success: "asset.set_name ok name=<N>"
    // On error:   "asset.set_name error: ..."
    harness.registry().registerCommand("asset.set_name",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasScene) {
                messages.push_back("asset.set_name error: scene not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            std::string name;
            if (const auto v = assetGetArg(cmd, "name"))
                name = std::string(*v);

            if (name.empty()) {
                messages.push_back("asset.set_name error: name is required");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            ctx.scene.setSceneName(name);
            ctx.sceneName = name;

            messages.push_back("asset.set_name ok name=" + name);
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── asset.describe ────────────────────────────────────────────────────────
    //
    // Reports scene metadata and up to 8 entry names/visibility.
    // Always returns true.
    harness.registry().registerCommand("asset.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasScene) {
                messages.push_back("asset.describe entries=0 name=");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            messages.push_back("asset.describe"
                " entries=" + std::to_string(ctx.scene.entryCount()) +
                " name="    + ctx.scene.sceneName());

            const size_t limit = std::min(ctx.scene.entryCount(), size_t{8});
            for (size_t i = 0; i < limit; ++i) {
                const auto& e = ctx.scene.entry(i);
                messages.push_back("asset.describe entry"
                    " index="   + std::to_string(i) +
                    " name="    + e.name +
                    " visible=" + (e.visible ? "1" : "0"));
            }

            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ═════════════════════════════════════════════════════════════════════════
    //  Gaussian Splatting
    // ═════════════════════════════════════════════════════════════════════════

    // ── gaussian.init ─────────────────────────────────────────────────────────
    //
    // Resets ctx.gaussianCloud to an empty cloud.
    //
    // Arguments:
    //   sh_degree=<N>   spherical harmonics degree 0-3 (default 3)
    //
    // On success: "gaussian.init ok sh_degree=<D>"
    harness.registry().registerCommand("gaussian.init",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            const uint32_t deg = uintArg(cmd, "sh_degree", 3u);

            ctx.gaussianCloud = nexus::gfx::GaussianSplatCloud{};
            ctx.gaussianCloud.shDegree = std::min(deg, 3u);
            ctx.hasGaussianCloud = true;

            messages.push_back("gaussian.init ok sh_degree=" +
                std::to_string(ctx.gaussianCloud.shDegree));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gaussian.add_splat ────────────────────────────────────────────────────
    //
    // Appends a Gaussian splat to ctx.gaussianCloud.
    //
    // Arguments:
    //   px=<f> py=<f> pz=<f>   position (default 0 0 0)
    //   opacity=<f>             logit-space opacity (default 0.0)
    //
    // On success: "gaussian.add_splat ok index=<I>"
    // On error:   "gaussian.add_splat error: cloud not initialized"
    harness.registry().registerCommand("gaussian.add_splat",
        [](ScriptContext& ctx, const ScriptCommand& cmd,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasGaussianCloud) {
                messages.push_back("gaussian.add_splat error: cloud not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            nexus::gfx::GaussianSplat splat{};
            splat.position = {floatArg(cmd, "px", 0.f),
                              floatArg(cmd, "py", 0.f),
                              floatArg(cmd, "pz", 0.f)};
            splat.opacity  = floatArg(cmd, "opacity", 0.f);

            const size_t index = ctx.gaussianCloud.splatCount();
            ctx.gaussianCloud.splats.push_back(std::move(splat));

            messages.push_back("gaussian.add_splat ok index=" + std::to_string(index));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gaussian.snapshot ─────────────────────────────────────────────────────
    //
    // Serializes the current gaussianCloud into gaussianBaselineBytes.
    //
    // On success: "gaussian.snapshot ok splats=<N> bytes=<B>"
    // On error:   "gaussian.snapshot error: cloud not initialized"
    harness.registry().registerCommand("gaussian.snapshot",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasGaussianCloud) {
                messages.push_back("gaussian.snapshot error: cloud not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            // Encode splat count + per-splat positions as a lightweight baseline.
            // We store: [uint32_t count] [float px,py,pz per splat]
            const uint32_t n = static_cast<uint32_t>(ctx.gaussianCloud.splatCount());
            ctx.gaussianBaselineBytes.clear();
            ctx.gaussianBaselineBytes.resize(sizeof(uint32_t) + n * 3 * sizeof(float));

            uint8_t* p = ctx.gaussianBaselineBytes.data();
            std::memcpy(p, &n, sizeof(uint32_t));
            p += sizeof(uint32_t);
            for (uint32_t i = 0; i < n; ++i) {
                const auto& s = ctx.gaussianCloud.splats[i];
                std::memcpy(p,              &s.position.x, sizeof(float));
                std::memcpy(p + 4,          &s.position.y, sizeof(float));
                std::memcpy(p + 8,          &s.position.z, sizeof(float));
                p += 12;
            }
            ctx.hasGaussianBaseline = true;

            messages.push_back("gaussian.snapshot ok"
                " splats=" + std::to_string(n) +
                " bytes="  + std::to_string(ctx.gaussianBaselineBytes.size()));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gaussian.diff ─────────────────────────────────────────────────────────
    //
    // Compares current splat count against the baseline snapshot.
    //
    // On success: "gaussian.diff ok baseline=<B> current=<C> added=<A>"
    // On error:   "gaussian.diff error: ..."
    harness.registry().registerCommand("gaussian.diff",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasGaussianBaseline || ctx.gaussianBaselineBytes.size() < sizeof(uint32_t)) {
                messages.push_back("gaussian.diff error: no snapshot");
                std::sort(messages.begin(), messages.end());
                return false;
            }
            if (!ctx.hasGaussianCloud) {
                messages.push_back("gaussian.diff error: cloud not initialized");
                std::sort(messages.begin(), messages.end());
                return false;
            }

            uint32_t baseline = 0;
            std::memcpy(&baseline, ctx.gaussianBaselineBytes.data(), sizeof(uint32_t));
            const uint32_t current = static_cast<uint32_t>(ctx.gaussianCloud.splatCount());
            const uint32_t added   = (current >= baseline) ? (current - baseline) : 0u;

            messages.push_back("gaussian.diff ok"
                " baseline=" + std::to_string(baseline) +
                " current="  + std::to_string(current) +
                " added="    + std::to_string(added));
            std::sort(messages.begin(), messages.end());
            return true;
        });

    // ── gaussian.describe ─────────────────────────────────────────────────────
    //
    // Reports cloud metadata. Always returns true.
    harness.registry().registerCommand("gaussian.describe",
        [](ScriptContext& ctx, const ScriptCommand& /*cmd*/,
           std::vector<std::string>& messages) -> bool {
            if (!ctx.hasGaussianCloud) {
                messages.push_back("gaussian.describe splats=0 sh_degree=0");
                std::sort(messages.begin(), messages.end());
                return true;
            }

            messages.push_back("gaussian.describe"
                " splats="    + std::to_string(ctx.gaussianCloud.splatCount()) +
                " sh_degree=" + std::to_string(ctx.gaussianCloud.shDegree));
            std::sort(messages.begin(), messages.end());
            return true;
        });
}

} // namespace nexus::automation
