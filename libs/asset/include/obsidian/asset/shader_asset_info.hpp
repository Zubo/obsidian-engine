#pragma once

#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/shader.hpp>

#include <vector>

namespace obsidian::asset {

struct Asset;
struct AssetMetadata;

struct ShaderAssetInfo : AssetInfo {
  core::ShaderType shaderType;
};

bool readShaderAssetInfo(AssetMetadata const& assetMetadata,
                         ShaderAssetInfo& outShaderAssetInfo);

bool packShader(ShaderAssetInfo const& shaderAssetInfo,
                std::vector<char> shaderData, Asset& outAsset);

} /*namespace obsidian::asset*/
