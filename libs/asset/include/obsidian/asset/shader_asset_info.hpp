#pragma once

#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/shader.hpp>

#include <optional>
#include <vector>

namespace obsidian::asset {

struct Asset;
struct AssetMetadata;

struct ShaderPermutationInfo {
  std::size_t baseSize;
  std::size_t vertexNormalSize;
  std::size_t vertexNormalColorSize;
  std::size_t vertexNormalUVSize;
};

struct ShaderAssetInfo : AssetInfo {
  core::ShaderType shaderType;
  std::optional<ShaderPermutationInfo> permutationOffsets;
};

bool readShaderAssetInfo(AssetMetadata const& assetMetadata,
                         ShaderAssetInfo& outShaderAssetInfo);

bool packShader(ShaderAssetInfo const& shaderAssetInfo,
                std::vector<char> shaderData, Asset& outAsset);

} /*namespace obsidian::asset*/
