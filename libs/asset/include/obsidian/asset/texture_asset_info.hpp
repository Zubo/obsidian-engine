#pragma once

#include <obsidian/asset/asset_info.hpp>

#include <cstddef>
#include <cstdint>

namespace obsidian::asset {

struct Asset;

enum class TextureFormat : std::uint32_t {
  unknown = 0,
  R8G8B8 = 1,
  R8G8B8A8 = 2
};

struct TextureAssetInfo : public AssetInfo {
  TextureFormat format;
  std::uint32_t width;
  std::uint32_t height;
};

bool readTextureAssetInfo(Asset const& asset,
                          TextureAssetInfo& outTextureAssetInfo);

bool packTexture(TextureAssetInfo const& textureAssetInfo,
                 void const* pixelData, Asset& outAsset);

} /*namespace obsidian::asset*/
