#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

namespace obsidian::asset {

bool readShaderAssetInfo(Asset const& asset,
                         ShaderAssetInfo& outShaderAssetInfo) {
  try {
    nlohmann::json json = nlohmann::json::parse(asset.json);
    outShaderAssetInfo.unpackedSize = json[unpackedSizeJsonName];
    outShaderAssetInfo.compressionMode = json[compressionModeJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packShader(ShaderAssetInfo const& shaderAssetInfo,
                std::vector<char> shaderData, Asset& outAsset) {
  outAsset.type[0] = 's';
  outAsset.type[1] = 'h';
  outAsset.type[2] = 'a';
  outAsset.type[3] = 'd';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json json;
    json[unpackedSizeJsonName] = shaderAssetInfo.unpackedSize;
    json[compressionModeJsonName] = shaderAssetInfo.compressionMode;

    outAsset.json = json.dump();

    if (shaderAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob = std::move(shaderData);
    } else if (shaderAssetInfo.compressionMode == CompressionMode::LZ4) {
      std::size_t compressedSize =
          LZ4_compressBound(shaderAssetInfo.unpackedSize);

      outAsset.binaryBlob.resize(compressedSize);

      LZ4_compress_default(reinterpret_cast<char const*>(shaderData.data()),
                           outAsset.binaryBlob.data(),
                           shaderAssetInfo.unpackedSize, compressedSize);
    } else {
      OBS_LOG_ERR("Error: Unknown compression mode.");
      return false;
    }
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }

  return true;
}

} /*namespace obsidian::asset */
