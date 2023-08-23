#pragma once

#include <cstddef>
#include <cstdint>

namespace obsidian::asset {

struct Asset;

enum class TextureFormat : std::uint32_t { unknown = 0, RGBA8 = 1 };

enum class CompressionMode : std::uint32_t { none = 0, LZ4 = 1 };

struct TextureAssetInfo {
  std::size_t textureSize;
  TextureFormat format;
  CompressionMode compressionMode;
  std::uint32_t width;
  std::uint32_t height;
};

bool readTextureAssetInfo(Asset const& asset,
                          TextureAssetInfo const& outTextureAssetInfo);

bool packTexture(TextureAssetInfo const& textureAssetInfo, void* pixelData,
                 Asset& outAsset);

bool unpackTexture(TextureAssetInfo const& textureInfo, char const* src,
                   std::size_t srcSize, char* dst);

} /*namespace obsidian::asset*/
