#pragma once

#include <cstddef>
#include <cstdint>

namespace obsidian::asset {

struct Asset;

enum class TextureFormat : std::uint32_t {
  unknown = 0,
  R8G8B8 = 1,
  R8G8B8A8 = 2
};

enum class CompressionMode : std::uint32_t { none = 0, LZ4 = 1 };

struct TextureAssetInfo {
  std::size_t textureSize;
  TextureFormat format;
  CompressionMode compressionMode;
  std::uint32_t width;
  std::uint32_t height;
};

bool readTextureAssetInfo(Asset const& asset,
                          TextureAssetInfo& outTextureAssetInfo);

bool packTexture(TextureAssetInfo const& textureAssetInfo,
                 void const* pixelData, Asset& outAsset);

bool unpackTexture(TextureAssetInfo const& textureInfo, char const* src,
                   std::size_t srcSize, char* dst);

} /*namespace obsidian::asset*/
