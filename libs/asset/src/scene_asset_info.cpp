#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/scene_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <exception>
#include <utility>

namespace obsidian::asset {

bool readSceneAssetInfo(Asset const& asset, SceneAssetInfo& outSceneAssetInfo) {
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outSceneAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outSceneAssetInfo.compressionMode = json[compressionModeJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packSceneAsset(SceneAssetInfo const& sceneAssetInfo,
                    std::vector<char> sceneData, Asset& outAsset) {
  outAsset.type[0] = 's';
  outAsset.type[1] = 'c';
  outAsset.type[2] = 'e';
  outAsset.type[3] = 'n';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json json;

    json[unpackedSizeJsonName] = sceneAssetInfo.unpackedSize;
    json[compressionModeJsonName] = sceneAssetInfo.compressionMode;

    outAsset.json = json.dump();

    if (sceneAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(sceneData);
    } else {
      assert(sceneAssetInfo.unpackedSize == sceneData.size());
      return compress(sceneData, outAsset.binaryBlob);
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset*/
