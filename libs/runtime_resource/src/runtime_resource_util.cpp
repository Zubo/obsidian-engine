#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource_util.hpp>

#include <cassert>

using namespace obsidian;

rhi::UploadShaderRHI obsidian::runtime_resource::getUploadShader(
    obsidian::asset::Asset const& asset) {
  obsidian::rhi::UploadShaderRHI uploadRHI;

  asset::ShaderAssetInfo assetInfo;
  bool const result = asset::readShaderAssetInfo(*asset.metadata, assetInfo);

  assert(result && "Depth only shader asset info failed to load");

  std::vector<char> shaderData;
  shaderData.resize(assetInfo.unpackedSize);
  asset::unpackAsset(assetInfo, asset.binaryBlob.data(),
                     asset.binaryBlob.size(), shaderData.data());
  if (assetInfo.permutationOffsets) {
    asset::ShaderPermutationInfo const& permutationInfo =
        *assetInfo.permutationOffsets;

    std::size_t currentOffset = 0;

    uploadRHI.code[rhi::ShaderPermutationRHI::base].resize(
        permutationInfo.baseSize);
    std::memcpy(uploadRHI.code[rhi::ShaderPermutationRHI::base].data(),
                shaderData.data(), permutationInfo.baseSize);
    currentOffset += permutationInfo.baseSize;

    uploadRHI.code[rhi::ShaderPermutationRHI::vertexNormal].resize(
        permutationInfo.vertexNormalSize);
    std::memcpy(uploadRHI.code[rhi::ShaderPermutationRHI::vertexNormal].data(),
                shaderData.data() + currentOffset,
                permutationInfo.vertexNormalSize);
    currentOffset += permutationInfo.vertexNormalSize;

    uploadRHI.code[rhi::ShaderPermutationRHI::vertexNormalColor].resize(
        permutationInfo.vertexNormalColorSize);
    std::memcpy(
        uploadRHI.code[rhi::ShaderPermutationRHI::vertexNormalColor].data(),
        shaderData.data() + currentOffset,
        permutationInfo.vertexNormalColorSize);
    currentOffset += permutationInfo.vertexNormalColorSize;

    uploadRHI.code[rhi::ShaderPermutationRHI::vertexNormalUV].resize(
        permutationInfo.vertexNormalUVSize);
    std::memcpy(
        uploadRHI.code[rhi::ShaderPermutationRHI::vertexNormalUV].data(),
        shaderData.data() + currentOffset, permutationInfo.vertexNormalUVSize);
    return uploadRHI;
  } else {
    uploadRHI.code[rhi::ShaderPermutationRHI::base].resize(
        assetInfo.unpackedSize);
    std::memcpy(uploadRHI.code[rhi::ShaderPermutationRHI::base].data(),
                shaderData.data(), assetInfo.unpackedSize);
    return uploadRHI;
  }
}
