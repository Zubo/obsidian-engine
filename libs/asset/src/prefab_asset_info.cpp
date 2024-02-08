#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/prefab_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

namespace obsidian::asset {

bool readPrefabAssetInfo(AssetMetadata const& assetMetadata,
                         PrefabAssetInfo& outPrefabAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json json = nlohmann::json::parse(assetMetadata.json);
    outPrefabAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outPrefabAssetInfo.compressionMode = json[compressionModeJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packPrefab(PrefabAssetInfo const& prefabAssetInfo,
                std::vector<char> prefabData, Asset& outAsset) {
  ZoneScoped;

  if (!outAsset.metadata) {
    outAsset.metadata.emplace();
  }

  outAsset.metadata->type[0] = 'p';
  outAsset.metadata->type[1] = 'r';
  outAsset.metadata->type[2] = 'e';
  outAsset.metadata->type[3] = 'f';

  outAsset.metadata->version = currentAssetVersion;

  try {
    nlohmann::json json;

    json[unpackedSizeJsonName] = prefabAssetInfo.unpackedSize;
    json[compressionModeJsonName] = prefabAssetInfo.compressionMode;

    outAsset.metadata->json = json.dump();

    if (prefabAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(prefabData);
    } else {
      assert(prefabAssetInfo.unpackedSize == prefabData.size());
      return compress(prefabData, outAsset.binaryBlob);
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset*/
