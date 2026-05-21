#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Asset — Scene Asset Importer
//
//  High-level import entrypoint for scene-asset loading workflows.
//  Provides:
//  - single-scene import
//  - multi-scene import with optional dependency-driven orchestration
//
//  Dependency-driven mode delegates to SceneAsset::loadPackage() so callers can
//  opt in without directly invoking package internals.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/asset/SceneAsset.h>

#include <map>
#include <string>
#include <vector>

namespace nexus::asset {

struct SceneAssetImportOptions {
    // When true, importScenes() resolves dependencies via AssetDependencyGraph
    // through SceneAsset::loadPackage().
    // When false, importScenes() loads paths in input order.
    bool dependencyDrivenMultiScene = false;

    // Migration policy used when loading package manifest files.
    SceneAssetPackageMigrationPolicy packageMigrationPolicy{};
};

class SceneAssetImporter {
public:
    // Imports a single scene asset file into outScene.
    [[nodiscard]] static SceneAssetIOReport importScene(const std::string& path,
                                                        SceneAsset& outScene) noexcept;

    // Imports multiple scene assets.
    // - dependencyDrivenMultiScene=false: import in input order
    // - dependencyDrivenMultiScene=true: dependency-first deterministic load order
    [[nodiscard]] static SceneAssetPackageReport importScenes(
        const std::vector<SceneAssetPackageEntry>& packageEntries,
        std::map<std::string, SceneAsset>& outScenes,
        const SceneAssetImportOptions& options = {}) noexcept;

    // Convenience overload for callers that only have path lists.
    // Dependencies are not expressed in this mode.
    [[nodiscard]] static SceneAssetPackageReport importScenes(
        const std::vector<std::string>& paths,
        std::map<std::string, SceneAsset>& outScenes,
        const SceneAssetImportOptions& options = {}) noexcept;

    // Imports package entries from a versioned package-manifest file, then
    // loads scenes using the same dependency-driven/sequential options.
    [[nodiscard]] static SceneAssetPackageReport importPackageManifest(
        const std::string& manifestPath,
        std::map<std::string, SceneAsset>& outScenes,
        const SceneAssetImportOptions& options = {}) noexcept;
};

} // namespace nexus::asset
