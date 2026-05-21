#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Asset — Scene Text Adapter v0
//
//  Deterministic text import/export adapter for SceneAsset.
//  Format: NEXUS_SCENE_TEXT_V1 (tab-separated records with escaped text fields).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/asset/SceneAsset.h>

#include <string>

namespace nexus::asset {

inline constexpr const char* kSceneAssetTextFormatMagic = "NEXUS_SCENE_TEXT_V1";

class SceneAssetTextAdapter {
public:
    // Export SceneAsset into a deterministic text scene format.
    [[nodiscard]] static SceneAssetIOReport exportScene(const SceneAsset& scene,
                                                        const std::string& path) noexcept;

    // Import SceneAsset from deterministic text scene format.
    [[nodiscard]] static SceneAssetIOReport importScene(const std::string& path,
                                                        SceneAsset& outScene) noexcept;
};

} // namespace nexus::asset
