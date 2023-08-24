#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <filesystem>
#include <optional>

namespace obsidian::rhi {

class RHI;

} /*namespace obsidian::rhi*/

namespace obsidian::runtime_resource {

class RuntimeResource {
public:
  RuntimeResource(std::filesystem::path path, rhi::RHI& rhi);
  RuntimeResource(RuntimeResource const& other) = delete;

  RuntimeResource& operator=(RuntimeResource const& other) = delete;

  bool loadAsset();
  void releaseAsset();
  void uploadToRHI();

private:
  rhi::RHI& _rhi;
  std::filesystem::path _path;
  std::optional<asset::Asset> _asset;
  rhi::ResourceIdRHI _rhiResourceId = rhi::rhiIdUninitialized;
};

} /*namespace obsidian::runtime_resource*/
