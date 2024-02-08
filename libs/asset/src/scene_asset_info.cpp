#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/scene_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>
#include <exception>
#include <utility>

namespace obsidian::asset {

bool readSceneAssetInfo(AssetMetadata const& assetMetadata,
                        SceneAssetInfo& outSceneAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json json = nlohmann::json::parse(assetMetadata.json);
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
  ZoneScoped;

  if (!outAsset.metadata) {
    outAsset.metadata.emplace();
  }

  outAsset.metadata->type[0] = 's';
  outAsset.metadata->type[1] = 'c';
  outAsset.metadata->type[2] = 'e';
  outAsset.metadata->type[3] = 'n';

  outAsset.metadata->version = currentAssetVersion;

  try {
    nlohmann::json json;

    json[unpackedSizeJsonName] = sceneAssetInfo.unpackedSize;
    json[compressionModeJsonName] = sceneAssetInfo.compressionMode;

    outAsset.metadata->json = json.dump();

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
