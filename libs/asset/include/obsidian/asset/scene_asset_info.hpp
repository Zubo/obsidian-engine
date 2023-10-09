#pragma once

#include <obsidian/asset/asset_info.hpp>

#include <vector>

namespace obsidian::asset {

struct Asset;

struct SceneAssetInfo : public AssetInfo {};

bool readSceneAssetInfo(Asset const& asset, SceneAssetInfo& outSceneAssetInfo);

bool packSceneAsset(SceneAssetInfo const& sceneAssetInfo,
                    std::vector<char> sceneData, Asset& outAsset);

} // namespace obsidian::asset
