#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

#include <cassert>
#include <filesystem>
#include <tuple>
#include <utility>

using namespace obsidian;
using namespace obsidian::runtime_resource;

namespace fs = std::filesystem;

void RuntimeResourceManager::init(rhi::RHI& rhi, project::Project& project) {
  _rhi = &rhi;
  _project = &project;
}

void RuntimeResourceManager::uploadInitRHIResources() {
  rhi::InitResourcesRHI initResources;

  asset::Asset depthShaderAsset;
  bool result = asset::loadFromFile(
      _project->getAbsolutePath("obsidian/shaders/depth-only-dbg.obsshad"),
      depthShaderAsset);

  assert(result && "Depth only shader asset failed to load");

  asset::ShaderAssetInfo depthShaderAssetInfo;
  result = asset::readShaderAssetInfo(depthShaderAsset, depthShaderAssetInfo);

  assert(result && "Depth only shader asset info failed to load");

  initResources.shadowPassShader.shaderDataSize =
      depthShaderAssetInfo.unpackedSize;
  initResources.shadowPassShader.unpackFunc =
      [&depthShaderAsset, &depthShaderAssetInfo](char* dst) {
        asset::unpackAsset(depthShaderAssetInfo,
                           depthShaderAsset.binaryBlob.data(),
                           depthShaderAsset.binaryBlob.size(), dst);
      };

  asset::Asset ssaoShaderAsset;
  result = asset::loadFromFile(
      _project->getAbsolutePath("obsidian/shaders/ssao-dbg.obsshad"),
      ssaoShaderAsset);

  assert(result && "Ssao shader asset failed to load");

  asset::ShaderAssetInfo ssaoShaderAssetInfo;
  result = asset::readShaderAssetInfo(ssaoShaderAsset, ssaoShaderAssetInfo);

  assert(result && "Ssao shader asset info failed to load");

  initResources.ssaoShader.shaderDataSize = ssaoShaderAssetInfo.unpackedSize;
  initResources.ssaoShader.unpackFunc = [&ssaoShaderAsset,
                                         &ssaoShaderAssetInfo](char* dst) {
    asset::unpackAsset(ssaoShaderAssetInfo, ssaoShaderAsset.binaryBlob.data(),
                       ssaoShaderAsset.binaryBlob.size(), dst);
  };

  asset::Asset postProcessingShaderAsset;
  result = asset::loadFromFile(
      _project->getAbsolutePath("obsidian/shaders/post-processing-dbg.obsshad"),
      postProcessingShaderAsset);

  assert(result && "Post processing shader asset failed to load");

  asset::ShaderAssetInfo postProcessingShaderAssetInfo;
  result = asset::readShaderAssetInfo(postProcessingShaderAsset,
                                      postProcessingShaderAssetInfo);

  assert(result && "Post processing shader asset info failed to load");

  initResources.postProcessingShader.shaderDataSize =
      postProcessingShaderAssetInfo.unpackedSize;
  initResources.postProcessingShader.unpackFunc =
      [&postProcessingShaderAsset, &postProcessingShaderAssetInfo](char* dst) {
        asset::unpackAsset(postProcessingShaderAssetInfo,
                           postProcessingShaderAsset.binaryBlob.data(),
                           postProcessingShaderAsset.binaryBlob.size(), dst);
      };

  _rhi->initResources(initResources);
}

void RuntimeResourceManager::cleanup() {
  _rhi->waitDeviceIdle();
  _runtimeResources.clear();
}

RuntimeResource& RuntimeResourceManager::getResource(fs::path const& path) {
  assert(_rhi && "Error: RuntimeResourceManager is not initialized.");

  auto const resourceIter = _runtimeResources.find(path);
  if (resourceIter == _runtimeResources.cend()) {
    auto const result = _runtimeResources.emplace(
        std::piecewise_construct, std::forward_as_tuple(path),
        std::forward_as_tuple(_project->getAbsolutePath(path), *this, *_rhi));

    return (*result.first).second;
  }

  return resourceIter->second;
}
