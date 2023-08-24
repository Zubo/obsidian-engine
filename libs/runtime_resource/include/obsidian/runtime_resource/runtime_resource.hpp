#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/rhi.hpp>

#include <filesystem>
#include <optional>

namespace obsidian::runtime_resource {

class RuntimeResource {
public:
  RuntimeResource(std::filesystem::path path);
  RuntimeResource(RuntimeResource const& other) = delete;

  RuntimeResource& operator=(RuntimeResource const& other) = delete;

  bool loadAsset();
  void releaseAsset();
  void uploadToRHI();

private:
  std::filesystem::path _path;
  std::optional<asset::Asset> _asset;
  rhi::RHIResourceId _rhiResourceId = rhi::rhiIdUninitialized;
};

} /*namespace obsidian::runtime_resource*/
