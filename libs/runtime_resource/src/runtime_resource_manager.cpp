#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
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

void RuntimeResourceManager::init(rhi::RHI& rhi,
                                  std::filesystem::path rootPath) {
  _rhi = &rhi;
  _rootPath = std::move(rootPath);
}

void RuntimeResourceManager::uploadInitRHIResources() {
  rhi::InitResourcesRHI initResources;

  asset::Asset vertShaderAsset;
  asset::loadFromFile(_rootPath / "shadow-pass-vert.obsshad", vertShaderAsset);
  asset::ShaderAssetInfo shadowVertShaderAssetInfo;
  asset::readShaderAssetInfo(vertShaderAsset, shadowVertShaderAssetInfo);

  initResources.shadowPassVert.shaderDataSize =
      shadowVertShaderAssetInfo.unpackedSize;
  initResources.shadowPassVert.unpackFunc =
      [&vertShaderAsset, &shadowVertShaderAssetInfo](char* dst) {
        asset::unpackAsset(shadowVertShaderAssetInfo,
                           vertShaderAsset.binaryBlob.data(),
                           vertShaderAsset.binaryBlob.size(), dst);
      };

  asset::Asset emptyShaderAsset;
  asset::loadFromFile(_rootPath / "empty-frag.obsshad", emptyShaderAsset);
  asset::ShaderAssetInfo emptyShaderAssetInfo;
  asset::readShaderAssetInfo(emptyShaderAsset, emptyShaderAssetInfo);

  initResources.shadowPassFrag.shaderDataSize =
      emptyShaderAssetInfo.unpackedSize;
  initResources.shadowPassFrag.unpackFunc = [&emptyShaderAsset,
                                             &emptyShaderAssetInfo](char* dst) {
    asset::unpackAsset(emptyShaderAssetInfo, emptyShaderAsset.binaryBlob.data(),
                       emptyShaderAsset.binaryBlob.size(), dst);
  };

  _rhi->initResources(initResources);
}

void RuntimeResourceManager::cleanup() { _runtimeResources.clear(); }

RuntimeResource& RuntimeResourceManager::getResource(fs::path const& path) {
  assert(_rhi && "Error: RuntimeResourceManager is not initialized.");

  auto const resourceIter = _runtimeResources.find(path);
  if (resourceIter == _runtimeResources.cend()) {
    auto const result = _runtimeResources.emplace(
        std::piecewise_construct, std::forward_as_tuple(path),
        std::forward_as_tuple(_rootPath / path, *this, *_rhi));

    return (*result.first).second;
  }

  return resourceIter->second;
}
