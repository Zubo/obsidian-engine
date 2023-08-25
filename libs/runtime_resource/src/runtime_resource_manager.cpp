#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

#include <tuple>
#include <utility>

using namespace obsidian;
using namespace obsidian::runtime_resource;

RuntimeResourceManager::RuntimeResourceManager(rhi::RHI& rhi) : _rhi{rhi} {}

RuntimeResource&
RuntimeResourceManager::getResource(std::filesystem::path const& path) {
  auto const resourceIter = _runtimeResources.find(path);
  if (resourceIter == _runtimeResources.cend()) {
    auto const result = _runtimeResources.emplace(
        std::piecewise_construct, std::forward_as_tuple(path),
        std::forward_as_tuple(path, *this, _rhi));

    return (*result.first).second;
  }

  return resourceIter->second;
}
