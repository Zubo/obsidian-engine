#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace obsidian::rhi {

class RHI;

} /*namespace obsidian::rhi*/

namespace obsidian::runtime_resource {

class RuntimeResourceManager;
class RuntimeResourceLoader;

enum class RuntimeResourceState {
  initial,
  pendingLoad,
  assetLoading,
  assetLoaded,
  uploadingToRhi,
  uploadedToRhi,
  assetLoadingFailed
};

class RuntimeResource {
public:
  RuntimeResource(std::filesystem::path path,
                  RuntimeResourceManager& runtimeResourceManager,
                  RuntimeResourceLoader& runtimeResourceLoader, rhi::RHI& rhi);
  RuntimeResource(RuntimeResource const& other) = delete;

  ~RuntimeResource();

  RuntimeResource& operator=(RuntimeResource const& other) = delete;

  RuntimeResourceState getResourceState() const;
  bool isResourceReady() const;
  rhi::ResourceIdRHI getResourceId() const;
  void load();
  std::filesystem::path getRelativePath() const;
  void declareRef();
  void releaseRef();

private:
  void releaseFromRHI();
  bool performAssetLoad();
  void releaseAsset();
  void performUploadToRHI();
  std::vector<RuntimeResource*> const& fetchDependencies();

  using ReleaseRHIResource = void (*)(rhi::RHI&, rhi::ResourceIdRHI);
  RuntimeResourceManager& _runtimeResourceManager;
  RuntimeResourceLoader& _runtimeResourceLoader;
  rhi::RHI& _rhi;
  std::filesystem::path _path;
  std::shared_ptr<asset::Asset> _asset;
  rhi::ResourceRHI* _resourceRHI = nullptr;
  ReleaseRHIResource _releaseFunc = nullptr;
  std::optional<std::vector<RuntimeResource*>> _dependencies;
  std::atomic<RuntimeResourceState> _resourceState =
      RuntimeResourceState::initial;
  std::uint32_t _refCount = 0;

  friend class RuntimeResourceLoader;
};

} /*namespace obsidian::runtime_resource*/
