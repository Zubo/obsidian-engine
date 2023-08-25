#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <filesystem>
#include <optional>

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
  rhi::ResourceIdRHI uploadToRHI();
  void unloadFromRHI();
  rhi::ResourceIdRHI getResourceIdRHI() const;

private:
  using ReleaseRHIResource = void (*)(rhi::RHI&, rhi::ResourceIdRHI);
  RuntimeResourceManager& _runtimeResourceManager;
  rhi::RHI& _rhi;
  std::filesystem::path _path;
  std::optional<asset::Asset> _asset;
  rhi::ResourceIdRHI _resourceIdRHI = rhi::rhiIdUninitialized;
  ReleaseRHIResource _releaseFunc = nullptr;
};

} /*namespace obsidian::runtime_resource*/
