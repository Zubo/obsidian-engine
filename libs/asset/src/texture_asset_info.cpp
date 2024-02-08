#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/asset/utility.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>
#include <tracy/Tracy.hpp>

#include <exception>

namespace obsidian::asset {

constexpr char const* formatJsonName = "format";
constexpr char const* textureWidthJsonName = "width";
constexpr char const* textureHeightJsonName = "height";
constexpr char const* mipLevelsJsonName = "mipLevels";
constexpr char const* transparentJsonName = "transparent";

bool readTextureAssetInfo(AssetMetadata const& assetMetadata,
                          TextureAssetInfo& outTextureAssetInfo) {
  ZoneScoped;

  try {
    nlohmann::json textureJson = nlohmann::json::parse(assetMetadata.json);

    outTextureAssetInfo.unpackedSize = textureJson[unpackedSizeJsonName];
    outTextureAssetInfo.compressionMode = textureJson[compressionModeJsonName];
    outTextureAssetInfo.format = textureJson[formatJsonName];
    outTextureAssetInfo.width = textureJson[textureWidthJsonName];
    outTextureAssetInfo.height = textureJson[textureHeightJsonName];
    outTextureAssetInfo.mipLevels = textureJson[mipLevelsJsonName];
    outTextureAssetInfo.transparent = textureJson[transparentJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packTexture(TextureAssetInfo const& textureAssetInfo,
                 void const* pixelData, Asset& outAsset) {
  ZoneScoped;

  if (!outAsset.metadata) {
    outAsset.metadata.emplace();
  }

  outAsset.metadata->type[0] = 't';
  outAsset.metadata->type[1] = 'e';
  outAsset.metadata->type[2] = 'x';
  outAsset.metadata->type[3] = 'i';

  outAsset.metadata->version = currentAssetVersion;

  if (!updateTextureAssetInfo(textureAssetInfo, outAsset)) {
    return false;
  }

  try {
    if (textureAssetInfo.compressionMode == CompressionMode::none) {
      outAsset.binaryBlob.resize(textureAssetInfo.unpackedSize);
      std::memcpy(outAsset.binaryBlob.data(), pixelData,
                  outAsset.binaryBlob.size());
    } else if (textureAssetInfo.compressionMode == CompressionMode::LZ4) {
      return compress(std::span(reinterpret_cast<char const*>(pixelData),
                                textureAssetInfo.unpackedSize),
                      outAsset.binaryBlob);
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

bool updateTextureAssetInfo(TextureAssetInfo const& textureAssetInfo,
                            Asset& outAsset) {
  try {
    nlohmann::json assetJson;
    assetJson[unpackedSizeJsonName] = textureAssetInfo.unpackedSize;
    assetJson[formatJsonName] = textureAssetInfo.format;
    assetJson[compressionModeJsonName] = textureAssetInfo.compressionMode;
    assetJson[textureWidthJsonName] = textureAssetInfo.width;
    assetJson[textureHeightJsonName] = textureAssetInfo.height;
    assetJson[mipLevelsJsonName] = textureAssetInfo.mipLevels;
    assetJson[transparentJsonName] = textureAssetInfo.transparent;

    outAsset.metadata->json = assetJson.dump();
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset*/
