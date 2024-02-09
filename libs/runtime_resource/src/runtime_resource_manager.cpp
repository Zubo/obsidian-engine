#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/project/project.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

#include <cassert>
#include <filesystem>
#include <tuple>
#include <utility>

using namespace obsidian;
using namespace obsidian::runtime_resource;

namespace fs = std::filesystem;

void RuntimeResourceManager::init(rhi::RHI& rhi, project::Project& project,
                                  task::TaskExecutor& taskExecutor) {
  _rhi = &rhi;
  _project = &project;
  _taskExecutor = &taskExecutor;
  _resourceLoader.run(taskExecutor);
}

void RuntimeResourceManager::uploadInitRHIResources() {
  rhi::InitResourcesRHI initResources;
  auto const loadShaderFunc = [this](rhi::UploadShaderRHI& uploadRHI,
                                     char const* path) {
    asset::Asset asset;
    bool result =
        asset::loadAssetFromFile(_project->getAbsolutePath(path), asset);

    assert(result && "Shader asset failed to load");

    asset::ShaderAssetInfo assetInfo;
    result = asset::readShaderAssetInfo(*asset.metadata, assetInfo);

    assert(result && "Depth only shader asset info failed to load");

    uploadRHI.shaderDataSize = assetInfo.unpackedSize;
    uploadRHI.unpackFunc = [asset = std::move(asset),
                            assetInfo = std::move(assetInfo)](char* dst) {
      asset::unpackAsset(assetInfo, asset.binaryBlob.data(),
                         asset.binaryBlob.size(), dst);
    };
  };

  loadShaderFunc(initResources.shadowPassVertexShader,
                 "obsidian/shaders/depth-only-vert.obsshad");
  loadShaderFunc(initResources.shadowPassFragmentShader,
                 "obsidian/shaders/depth-only-frag.obsshad");

  loadShaderFunc(initResources.ssaoVertexShader,
                 "obsidian/shaders/ssao-vert.obsshad");
  loadShaderFunc(initResources.ssaoFragmentShader,
                 "obsidian/shaders/ssao-frag.obsshad");

  loadShaderFunc(initResources.postProcessingVertexShader,
                 "obsidian/shaders/post-processing-vert.obsshad");
  loadShaderFunc(initResources.postProcessingFragmentShader,
                 "obsidian/shaders/post-processing-frag.obsshad");

  _rhi->initResources(initResources);
}

void RuntimeResourceManager::cleanup() {
  _resourceLoader.cleanup();

  if (_runtimeResources.size()) {
    _runtimeResources.clear();
  }
}

RuntimeResourceRef RuntimeResourceManager::getResource(fs::path const& path) {
  assert(_rhi && "RuntimeResourceManager is not initialized.");

  auto const resourceIter = _runtimeResources.find(path);
  if (resourceIter == _runtimeResources.cend()) {
    auto const result = _runtimeResources.emplace(
        std::piecewise_construct, std::forward_as_tuple(path),
        std::forward_as_tuple(_project->getAbsolutePath(path), *this,
                              _resourceLoader, *_rhi));

    return RuntimeResourceRef{result.first->second};
  }

  return RuntimeResourceRef{resourceIter->second};
}

project::Project const& RuntimeResourceManager::getProject() const {
  assert(_project);
  return *_project;
}
