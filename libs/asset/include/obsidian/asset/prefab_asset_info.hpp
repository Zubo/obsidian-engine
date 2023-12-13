#pragma once

#include <obsidian/asset/asset_info.hpp>

#include <vector>

namespace obsidian::asset {

struct Asset;
struct AssetMetadata;

struct PrefabAssetInfo : public AssetInfo {};

bool readPrefabAssetInfo(AssetMetadata const& assetMetadata,
                         PrefabAssetInfo& outPrefabAssetInfo);

bool packPrefab(PrefabAssetInfo const& prefabAssetInfo,
                std::vector<char> prefabData, Asset& outAsset);

} /*namespace obsidian::asset */
