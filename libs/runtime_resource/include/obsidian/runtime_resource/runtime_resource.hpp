#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
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

class RuntimeResource;

class RuntimeResourceRef {
public:
  explicit RuntimeResourceRef(RuntimeResource& runtimeResource);
  RuntimeResourceRef(RuntimeResourceRef const& other);
  RuntimeResourceRef(RuntimeResourceRef&& other) noexcept;
  ~RuntimeResourceRef();

  RuntimeResourceRef& operator=(RuntimeResourceRef const& other) noexcept;
  RuntimeResourceRef& operator=(RuntimeResourceRef&& other) noexcept;
  RuntimeResource& operator*();
  RuntimeResource const& operator*() const;
  RuntimeResource* operator->();
  RuntimeResource const* operator->() const;

  RuntimeResource& get();
  RuntimeResource const& get() const;

private:
  RuntimeResource* _runtimeResource = nullptr;
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
  void requestLoad();
  std::filesystem::path getRelativePath() const;

private:
  void acquireRef();
  void releaseRef();
  void releaseFromRHI();
  bool performAssetLoad();
  void releaseAsset();
  void performUploadToRHI();
  std::span<RuntimeResourceRef> fetchDependencies();
  std::function<void(char*)> getUnpackFunc(auto const& info);

  using ReleaseRHIResource = void (*)(rhi::RHI&, rhi::ResourceIdRHI);
  RuntimeResourceManager& _runtimeResourceManager;
  RuntimeResourceLoader& _runtimeResourceLoader;
  rhi::RHI& _rhi;
  std::filesystem::path _path;
  std::shared_ptr<asset::Asset> _asset;
  rhi::ResourceRHI* _resourceRHI = nullptr;
  ReleaseRHIResource _releaseFunc = nullptr;
  std::optional<std::vector<RuntimeResourceRef>> _dependencies;
  std::mutex _resourceMutex;
  std::atomic<RuntimeResourceState> _resourceState =
      RuntimeResourceState::initial;
  std::atomic<std::uint32_t> _refCount = 0;
  rhi::ResourceTransferRHI _transferRHI;

  friend class RuntimeResourceLoader;
  friend class RuntimeResourceRef;
};

} /*namespace obsidian::runtime_resource*/
