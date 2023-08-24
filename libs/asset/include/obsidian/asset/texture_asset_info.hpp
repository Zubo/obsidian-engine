#pragma once

#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/texture_format.hpp>

#include <cstddef>
#include <cstdint>

namespace obsidian::asset {

struct Asset;

struct TextureAssetInfo : public AssetInfo {
  core::TextureFormat format;
  std::uint32_t width;
  std::uint32_t height;
};

bool readTextureAssetInfo(Asset const& asset,
                          TextureAssetInfo& outTextureAssetInfo);

bool packTexture(TextureAssetInfo const& textureAssetInfo,
                 void const* pixelData, Asset& outAsset);

} /*namespace obsidian::asset*/
