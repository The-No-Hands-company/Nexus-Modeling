#pragma once
// ── Nexus Asset — SceneAssetTextAdapter

#include <nexus/asset/SceneAsset.h>

#include <string>

namespace nexus::asset {

inline constexpr const char* kSceneAssetTextFormatMagic = "NEXUS_SCENE_TEXT_V1";

class SceneAssetTextAdapter {
public:
    static SceneAssetIOReport exportScene(const SceneAsset& scene,
                                          const std::string& path) noexcept;

    static SceneAssetIOReport importScene(const std::string& path,
                                          SceneAsset& outScene) noexcept;
};

} // namespace nexus::asset
