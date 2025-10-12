#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/runtime_resource/runtime_resource_util.hpp>

#include <cassert>

using namespace obsidian;

rhi::UploadShaderRHI obsidian::runtime_resource::getUploadShader(
    obsidian::asset::Asset const& asset) {
  obsidian::rhi::UploadShaderRHI uploadRHI;

  asset::ShaderAssetInfo assetInfo;
  bool const result = asset::readShaderAssetInfo(*asset.metadata, assetInfo);

  assert(result && "Depth only shader asset info failed to load");

  std::vector<std::uint32_t> shaderData;
  shaderData.resize(assetInfo.unpackedSize);
  asset::unpackAsset(assetInfo, asset.binaryBlob.data(),
                     asset.binaryBlob.size(),
                     reinterpret_cast<char*>(shaderData.data()));
  if (assetInfo.permutationOffsets) {
    asset::ShaderPermutationInfo const& permutationInfo =
        *assetInfo.permutationOffsets;

    std::size_t currentOffset = 0;

    uploadRHI.baseCode.resize(permutationInfo.baseSize);
    std::memcpy(uploadRHI.baseCode.data(), shaderData.data(),
                permutationInfo.baseSize);
    currentOffset += permutationInfo.baseSize;

    uploadRHI.vertexNormalCode.resize(permutationInfo.vertexNormalSize);
    std::memcpy(uploadRHI.vertexNormalCode.data(),
                shaderData.data() + currentOffset,
                permutationInfo.vertexNormalSize);
    currentOffset += permutationInfo.vertexNormalSize;

    uploadRHI.vertexNormalColorCode.resize(
        permutationInfo.vertexNormalColorSize);
    std::memcpy(uploadRHI.vertexNormalColorCode.data(),
                shaderData.data() + currentOffset,
                permutationInfo.vertexNormalColorSize);
    currentOffset += permutationInfo.vertexNormalColorSize;

    uploadRHI.vertexNormalUVCode.resize(permutationInfo.vertexNormalUVSize);
    std::memcpy(uploadRHI.vertexNormalUVCode.data(),
                shaderData.data() + currentOffset,
                permutationInfo.vertexNormalUVSize);
    return uploadRHI;
  } else {
    uploadRHI.baseCode.resize(assetInfo.unpackedSize);
    std::memcpy(uploadRHI.baseCode.data(), shaderData.data(),
                assetInfo.unpackedSize);
    return uploadRHI;
  }
}
