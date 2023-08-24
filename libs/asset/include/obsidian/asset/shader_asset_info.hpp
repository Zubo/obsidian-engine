#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>

#include <vector>

namespace obsidian::asset {

struct ShaderAssetInfo : AssetInfo {};

bool readShaderAssetInfo(Asset const& asset,
                         ShaderAssetInfo& outShaderAssetInfo);

bool packShader(ShaderAssetInfo const& shaderAssetInfo,
                std::vector<char> shaderData, Asset& outAsset);

} /*namespace obsidian::asset*/
