#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>

#include <string>
#include <vector>

namespace obsidian::asset {

struct MaterialAssetInfo : public AssetInfo {
  std::vector<std::string> textures;
};

bool readMaterialAssetInfo(Asset const& asset,
                           MaterialAssetInfo& outMaterialAssetInfo);

bool packMaterial(MaterialAssetInfo const& materialAssetInfo,
                  std::vector<char> materialData, Asset& outAsset);

} /*namespace obsidian::asset*/