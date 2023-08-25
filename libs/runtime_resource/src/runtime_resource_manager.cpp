#include <filesystem>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

#include <cassert>
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
