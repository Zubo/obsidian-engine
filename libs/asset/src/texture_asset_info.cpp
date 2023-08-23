#include <asset/asset.hpp>
#include <asset/texture_asset_info.hpp>

#include <lz4.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <iostream>

namespace obsidian::asset {
constexpr char const* textureSizeJsonName = "textureSize";
constexpr char const* formatJsonName = "format";
constexpr char const* compressionModeJsonName = "compressionMode";
constexpr char const* textureWidthJsonName = "width";
constexpr char const* textureHeightJsonName = "height";

bool readTextureAssetInfo(Asset const& asset,
                          TextureAssetInfo& outTextureAssetInfo) {
  try {
    nlohmann::json textureJson = nlohmann::json::parse(asset.json);

    outTextureAssetInfo.textureSize = textureJson[textureSizeJsonName];
    outTextureAssetInfo.format = textureJson[formatJsonName];
    outTextureAssetInfo.compressionMode = textureJson[compressionModeJsonName];
    outTextureAssetInfo.width = textureJson[textureWidthJsonName];
    outTextureAssetInfo.height = textureJson[textureHeightJsonName];
  } catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
    return false;
  }

  return true;
}

bool packTexture(TextureAssetInfo const& textureAssetInfo, void* pixelData,
                 Asset& outAsset) {
  outAsset.type[0] = 'T';
  outAsset.type[1] = 'E';
  outAsset.type[2] = 'X';
  outAsset.type[3] = 'I';

  outAsset.version = currentAssetVersion;

  try {
    nlohmann::json assetJson;
    assetJson[textureSizeJsonName] = textureAssetInfo.textureSize;
    assetJson[formatJsonName] = textureAssetInfo.format;
    assetJson[compressionModeJsonName] = textureAssetInfo.compressionMode;
    assetJson[textureWidthJsonName] = textureAssetInfo.width;
    assetJson[textureHeightJsonName] = textureAssetInfo.height;

    outAsset.json = assetJson.dump();
  } catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
    return false;
  }

  std::size_t const compressBound =
      LZ4_compressBound(textureAssetInfo.textureSize);

  outAsset.binaryBlob.resize(compressBound);

  std::size_t compressedSize = LZ4_compress_default(
      reinterpret_cast<char*>(pixelData), outAsset.binaryBlob.data(),
      textureAssetInfo.textureSize, outAsset.binaryBlob.size());

  outAsset.binaryBlob.resize(compressedSize);

  return true;
}

bool unpackTexture(TextureAssetInfo const& textureInfo, char const* src,
                   std::size_t srcSize, char* dst) {
  switch (textureInfo.compressionMode) {
  case CompressionMode::none:
    std::memcpy(dst, src, srcSize);
    return true;
  case CompressionMode::LZ4:
    LZ4_decompress_safe(src, dst, srcSize, textureInfo.textureSize);
    return true;
  default:
    return false;
  }
}

} /*namespace obsidian::asset*/
