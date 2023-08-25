#pragma once

#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>

#include <filesystem>
#include <unordered_map>

namespace obsidian::runtime_resource {

class RuntimeResourceManager {
public:
  RuntimeResourceManager() = default;

  RuntimeResourceManager(RuntimeResourceManager const& other) = delete;

  void init(rhi::RHI& rhi);

  RuntimeResource& getResource(std::filesystem::path const& path);

  RuntimeResourceManager&
  operator=(RuntimeResourceManager const& other) = delete;

private:
  rhi::RHI* _rhi = nullptr;
  std::unordered_map<std::filesystem::path, RuntimeResource> _runtimeResources;
};

} /*namespace obsidian::runtime_resource*/
