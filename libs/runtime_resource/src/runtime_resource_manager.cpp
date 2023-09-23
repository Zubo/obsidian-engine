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

  asset::Asset shaderAsset;
  asset::loadFromFile(
      _project->getAbsolutePath("obsidian/shaders/depth-only.obsshad"),
      shaderAsset);
  asset::ShaderAssetInfo shadowPassShaderAssetInfo;
  asset::readShaderAssetInfo(shaderAsset, shadowPassShaderAssetInfo);

  initResources.shadowPassShader.shaderDataSize =
      shadowPassShaderAssetInfo.unpackedSize;
  initResources.shadowPassShader.unpackFunc =
      [&shaderAsset, &shadowPassShaderAssetInfo](char* dst) {
        asset::unpackAsset(shadowPassShaderAssetInfo,
                           shaderAsset.binaryBlob.data(),
                           shaderAsset.binaryBlob.size(), dst);
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
