#include <asset/asset.hpp>
#include <asset/asset_io.hpp>
#include <asset/texture_asset_info.hpp>
#include <asset_converter/asset_converter.hpp>

#include <stb/stb_image.h>

#include <iostream>

namespace fs = std::filesystem;

namespace obsidian::asset_converter {

bool convertPngToAsset(fs::path const srcPath, fs::path const& dstPath) {

  int w, h, channelCnt;

  unsigned char* data =
      stbi_load(srcPath.c_str(), &w, &h, &channelCnt, STBI_rgb_alpha);

  asset::Asset outAsset;
  asset::TextureAssetInfo textureAssetInfo;
  textureAssetInfo.unpackedSize = w * h * channelCnt;
  textureAssetInfo.compressionMode = asset::CompressionMode::LZ4;
  textureAssetInfo.format = asset::TextureFormat::R8G8B8A8;
  textureAssetInfo.width = w;
  textureAssetInfo.height = h;

  bool const packResult = asset::packTexture(textureAssetInfo, data, outAsset);

  stbi_image_free(data);

  if (!packResult) {
    return false;
  }

  std::cout << "Successfully converted " << srcPath << " to asset format."
            << std::endl;
  return asset::saveToFile(dstPath, outAsset);
}

bool convertAsset(fs::path const& srcPath, fs::path const& dstPath) {
  std::string extension = srcPath.extension().string();

  if (!extension.size()) {
    std::cout << "Error: File doesn't have extension." << std::endl;
    return false;
  }

  if (extension == ".png") {
    return convertPngToAsset(srcPath, dstPath);
  }

  std::cout << "Error: Unknown file extension." << std::endl;
  return false;
}

} /*namespace obsidian::asset_converter*/
