#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/core/logging.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

#include <exception>

namespace obsidian::asset {

constexpr char const* formatJsonName = "format";
constexpr char const* textureWidthJsonName = "width";
constexpr char const* textureHeightJsonName = "height";

bool readTextureAssetInfo(Asset const& asset,
                          TextureAssetInfo& outTextureAssetInfo) {
  try {
    nlohmann::json textureJson = nlohmann::json::parse(asset.json);

    outTextureAssetInfo.unpackedSize = textureJson[unpackedSizeJsonName];
    outTextureAssetInfo.compressionMode = textureJson[compressionModeJsonName];
    outTextureAssetInfo.format = textureJson[formatJsonName];
    outTextureAssetInfo.width = textureJson[textureWidthJsonName];
    outTextureAssetInfo.height = textureJson[textureHeightJsonName];
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

bool packTexture(TextureAssetInfo const& textureAssetInfo,
                 void const* pixelData, Asset& outAsset) {
  outAsset.type[0] = 't';
  outAsset.type[1] = 'e';
  outAsset.type[2] = 'x';
  outAsset.type[3] = 'i';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json assetJson;
    assetJson[unpackedSizeJsonName] = textureAssetInfo.unpackedSize;
    assetJson[formatJsonName] = textureAssetInfo.format;
    assetJson[compressionModeJsonName] = textureAssetInfo.compressionMode;
    assetJson[textureWidthJsonName] = textureAssetInfo.width;
    assetJson[textureHeightJsonName] = textureAssetInfo.height;

    outAsset.json = assetJson.dump();

    std::size_t const compressBound =
        LZ4_compressBound(textureAssetInfo.unpackedSize);

    outAsset.binaryBlob.resize(compressBound);

    std::size_t compressedSize = LZ4_compress_default(
        reinterpret_cast<char const*>(pixelData), outAsset.binaryBlob.data(),
        textureAssetInfo.unpackedSize, outAsset.binaryBlob.size());

    outAsset.binaryBlob.resize(compressedSize);
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return false;
  }

  return true;
}

} /*namespace obsidian::asset*/
