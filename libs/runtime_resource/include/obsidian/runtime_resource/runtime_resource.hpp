#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <filesystem>
#include <memory>

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
  rhi::ResourceState getResourceState();
  rhi::ResourceIdRHI uploadToRHI();
  void unloadFromRHI();
  rhi::ResourceIdRHI getResourceIdRHI() const;
  std::filesystem::path getRelativePath() const;

private:
  using ReleaseRHIResource = void (*)(rhi::RHI&, rhi::ResourceIdRHI);
  RuntimeResourceManager& _runtimeResourceManager;
  rhi::RHI& _rhi;
  std::filesystem::path _path;
  std::shared_ptr<asset::Asset> _asset;
  rhi::ResourceRHI* _resourceRHI = nullptr;
  ReleaseRHIResource _releaseFunc = nullptr;
};

} /*namespace obsidian::runtime_resource*/
