#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace obsidian::rhi {

class RHI;

} /*namespace obsidian::rhi*/

namespace obsidian::runtime_resource {

class RuntimeResourceManager;

class RuntimeResource {
public:
  RuntimeResource(std::filesystem::path path,
                  RuntimeResourceManager& runtimeResourceManager,
                  rhi::RHI& rhi);
  RuntimeResource(RuntimeResource const& other) = delete;

  ~RuntimeResource();

  RuntimeResource& operator=(RuntimeResource const& other) = delete;

  bool loadAsset();
  void releaseAsset();
  rhi::ResourceState getResourceState() const;
  rhi::ResourceIdRHI getResourceId() const;
  void uploadToRHI();
  void unloadFromRHI();
  rhi::ResourceIdRHI getResourceIdRHI() const;
  std::filesystem::path getRelativePath() const;
  std::vector<RuntimeResource const*> const& fetchDependencies();

private:
  using ReleaseRHIResource = void (*)(rhi::RHI&, rhi::ResourceIdRHI);
  RuntimeResourceManager& _runtimeResourceManager;
  rhi::RHI& _rhi;
  std::filesystem::path _path;
  std::shared_ptr<asset::Asset> _asset;
  rhi::ResourceRHI* _resourceRHI = nullptr;
  ReleaseRHIResource _releaseFunc = nullptr;
  std::optional<std::vector<RuntimeResource const*>> _dependencies;
};

} /*namespace obsidian::runtime_resource*/
