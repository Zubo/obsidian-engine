#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>

namespace obsidian::asset {

constexpr char const* shaderTypeJsonName = "shaderType";

bool readShaderAssetInfo(AssetMetadata const& assetMetadata,
                         ShaderAssetInfo& outShaderAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json json = nlohmann::json::parse(assetMetadata.json);
    outShaderAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outShaderAssetInfo.compressionMode = json[compressionModeJsonName];
    outShaderAssetInfo.shaderType = json[shaderTypeJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packShader(ShaderAssetInfo const& shaderAssetInfo,
                std::vector<char> shaderData, Asset& outAsset) {
  ZoneScoped;

  if (!outAsset.metadata) {
    outAsset.metadata.emplace();
  }

  outAsset.metadata->type[0] = 's';
  outAsset.metadata->type[1] = 'h';
  outAsset.metadata->type[2] = 'a';
  outAsset.metadata->type[3] = 'd';

  outAsset.metadata->version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = shaderAssetInfo.unpackedSize;
    json[compressionModeJsonName] = shaderAssetInfo.compressionMode;
    json[shaderTypeJsonName] = shaderAssetInfo.shaderType;

    outAsset.metadata->json = json.dump();

    if (shaderAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(shaderData);
    } else if (shaderAssetInfo.compressionMode == CompressionMode::LZ4) {
      assert(shaderAssetInfo.unpackedSize == shaderData.size());
      return compress(shaderData, outAsset.binaryBlob);
    } else {
      OBS_LOG_ERR("Unknown compression mode.");
      return false;
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset */
