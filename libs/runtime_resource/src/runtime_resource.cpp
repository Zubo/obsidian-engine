#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_loader.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

#include <memory>

using namespace obsidian;
using namespace obsidian::runtime_resource;

RuntimeResource::RuntimeResource(std::filesystem::path path,
                                 RuntimeResourceManager& runtimeResourceManager,
                                 RuntimeResourceLoader& runtimeResourceLoader,
                                 rhi::RHI& rhi)
    : _runtimeResourceManager{runtimeResourceManager},
      _runtimeResourceLoader{runtimeResourceLoader}, _rhi{rhi},
      _path{std::move(path)} {}

RuntimeResource::~RuntimeResource() {
  std::scoped_lock l{_resourceMutex};

  releaseAsset();
  releaseFromRHI();
}

RuntimeResourceState RuntimeResource::getResourceState() const {
  return _resourceState;
}

bool RuntimeResource::isResourceReady() const {
  return _resourceState == RuntimeResourceState::uploadedToRhi &&
         _resourceRHI->state == rhi::ResourceState::uploaded;
}

rhi::ResourceIdRHI RuntimeResource::getResourceId() const {
  return _resourceRHI ? _resourceRHI->id : rhi::rhiIdUninitialized;
}

void RuntimeResource::requestLoad() {
  RuntimeResourceState expected = RuntimeResourceState::initial;

  if (_resourceState.compare_exchange_strong(
          expected, RuntimeResourceState::pendingLoad)) {
    std::vector<RuntimeResource*> const& deps = fetchDependencies();

    for (RuntimeResource* const d : deps) {
      if (d->getResourceState() == RuntimeResourceState::initial) {
        d->requestLoad();
      }
    }

    _runtimeResourceLoader.loadResource(*this);
    return;
  }

  std::scoped_lock l{_resourceMutex};

  if (_resourceState == RuntimeResourceState::assetLoadingFailed) {
    assert(!_resourceRHI && !_releaseFunc &&
           "RuntimeResourceState::assetLoadingFailed implies "
           "the rhi resource was not created.");
    releaseAsset();
    _resourceState = RuntimeResourceState::initial;
  } else if (_resourceState == RuntimeResourceState::uploadedToRhi &&
             _resourceRHI &&
             _resourceRHI->state == rhi::ResourceState::invalid) {
    assert(!_asset && "RuntimeResourceState::uploadedToRhi implies the asset "
                      "is released from main memory.");
    releaseFromRHI();
    _resourceState = RuntimeResourceState::initial;
  }
}

std::filesystem::path RuntimeResource::getRelativePath() const {
  return _runtimeResourceManager.getProject().getRelativeToProjectRootPath(
      _path);
}

void RuntimeResource::declareRef() { ++_refCount; }

void RuntimeResource::releaseRef() {
  assert(_refCount);

  --_refCount;

  if (!_refCount) {
    releaseFromRHI();
    _dependencies->clear();
    _resourceState = RuntimeResourceState::initial;
  }
}

void RuntimeResource::releaseFromRHI() {
  if (_releaseFunc && _resourceRHI) {
    _releaseFunc(_rhi, _resourceRHI->id);
    _releaseFunc = nullptr;
    _resourceRHI = nullptr;
  }
}

bool RuntimeResource::performAssetLoad() {
  std::scoped_lock l{_resourceMutex};

  if (_resourceState != RuntimeResourceState::pendingLoad) {
    OBS_LOG_ERR("Expected resource state in the method performAssetLoad is "
                "RuntimeResourceState::pendingLoad. The actual state is " +
                std::to_string((int)_resourceState.load()));
    return false;
  }

  if (!_asset) {
    _asset = std::make_shared<asset::Asset>();
  }

  bool const loadResult =
      _asset->isLoaded || asset::loadAssetFromFile(_path, *_asset);

  if (!loadResult) {
    OBS_LOG_ERR("Failed to load asset on path " + _path.string());
  }

  _resourceState = loadResult ? RuntimeResourceState::assetLoaded
                              : RuntimeResourceState::assetLoadingFailed;

  return loadResult;
}

void RuntimeResource::releaseAsset() {
  if (_asset) {
    _asset.reset();
  }
}

void RuntimeResource::performUploadToRHI() {
  std::scoped_lock l{_resourceMutex};

  if (_resourceRHI && _resourceRHI->state != rhi::ResourceState::initial) {
    OBS_LOG_WARN("Trying to upload laready uploaded resource. Resource path: " +
                 _path.string());
    return;
  }

  if (!_asset || !_asset->isLoaded) {
    OBS_LOG_ERR("Can't upload resource to RHI before the asset is loaded. "
                "Resource path: " +
                _path.string());
    return;
  }

  if (_resourceState != RuntimeResourceState::assetLoaded) {
    OBS_LOG_ERR("_resourceState of the RuntimeResource has to be in the "
                "RuntimeResourceState::assetLoaded state before uploading it "
                "to the RHI. "
                "Resource path: " +
                _path.string());
    return;
  }

  asset::AssetType const assetType =
      asset::getAssetType(_asset->metadata->type);

  switch (assetType) {
  case asset::AssetType::mesh: {
    asset::MeshAssetInfo info;

    if (!asset::readMeshAssetInfo(*_asset->metadata, info)) {
      OBS_LOG_ERR("Failed to read mesh asset info");
      break;
    }

    rhi::UploadMeshRHI uploadMesh;
    uploadMesh.vertexCount = info.vertexCount;
    uploadMesh.vertexBufferSize = info.vertexBufferSize;
    uploadMesh.indexCount = info.indexCount;
    uploadMesh.indexBufferSizes = info.indexBufferSizes;
    uploadMesh.unpackFunc = getUnpackFunc(info);

    uploadMesh.aabb = info.aabb;
    uploadMesh.hasNormals = info.hasNormals;
    uploadMesh.hasColors = info.hasColors;
    uploadMesh.hasUV = info.hasUV;
    uploadMesh.hasTangents = info.hasTangents;

    _resourceRHI = &_rhi.initMeshResource();
    _rhi.uploadMesh(_resourceRHI->id, uploadMesh);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.releaseMesh(id);
    };
    break;
  }
  case asset::AssetType::texture: {
    asset::TextureAssetInfo info;

    if (!asset::readTextureAssetInfo(*_asset->metadata, info)) {
      OBS_LOG_ERR("Failed to read texture asset info");
      break;
    }

    rhi::UploadTextureRHI uploadTexture;
    uploadTexture.format = info.format;
    uploadTexture.width = info.width;
    uploadTexture.height = info.height;
    uploadTexture.mipLevels = info.mipLevels;
    uploadTexture.unpackFunc = getUnpackFunc(info);

    _resourceRHI = &_rhi.initTextureResource();
    _rhi.uploadTexture(_resourceRHI->id, std::move(uploadTexture));

    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.releaseTexture(id);
    };
    break;
  }
  case asset::AssetType::material: {
    asset::MaterialAssetInfo info;

    if (!asset::readMaterialAssetInfo(*_asset->metadata, info)) {
      OBS_LOG_ERR("Failed to read material asset info");
      break;
    }

    rhi::UploadMaterialRHI uploadMaterial;
    uploadMaterial.materialType = info.materialType;

    if (!info.diffuseTexturePath.empty()) {
      RuntimeResource& diffuseTexResource =
          _runtimeResourceManager.getResource(info.diffuseTexturePath);

      uploadMaterial.diffuseTextureId = diffuseTexResource.getResourceId();
    }

    if (!info.normalMapTexturePath.empty()) {
      RuntimeResource& normalMapResource =
          _runtimeResourceManager.getResource(info.normalMapTexturePath);

      uploadMaterial.normalTextureId = normalMapResource.getResourceId();
    }

    RuntimeResource& shaderResource =
        _runtimeResourceManager.getResource(info.shaderPath);

    uploadMaterial.shaderId = shaderResource.getResourceId();

    uploadMaterial.ambientColor = info.ambientColor;
    uploadMaterial.diffuseColor = info.diffuseColor;
    uploadMaterial.specularColor = info.specularColor;
    uploadMaterial.shininess = info.shininess;
    uploadMaterial.transparent = info.transparent;
    uploadMaterial.reflection = info.reflection;
    uploadMaterial.hasTimer = info.hasTimer;

    _resourceRHI = &_rhi.initMaterialResource();
    _rhi.uploadMaterial(_resourceRHI->id, uploadMaterial);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.releaseMaterial(id);
    };
    break;
  }
  case asset::AssetType::shader: {
    asset::ShaderAssetInfo info;
    if (!asset::readShaderAssetInfo(*_asset->metadata, info)) {
      OBS_LOG_ERR("Failed to read shader asset info");
      break;
    }

    rhi::UploadShaderRHI uploadShader;
    uploadShader.shaderDataSize = info.unpackedSize;
    uploadShader.unpackFunc = getUnpackFunc(info);

    _resourceRHI = &_rhi.initShaderResource();
    _rhi.uploadShader(_resourceRHI->id, uploadShader);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.releaseShader(id);
    };
    break;
  }
  default:
    OBS_LOG_ERR("Trying to upload unknown asset type");
  }

  _resourceState = RuntimeResourceState::uploadedToRhi;
}

std::vector<RuntimeResource*> const& RuntimeResource::fetchDependencies() {
  if (!_dependencies) {
    _dependencies.emplace();

    if (!_asset) {
      _asset = std::make_shared<asset::Asset>();
    }

    if (!_asset->metadata) {
      _asset->metadata.emplace();
      asset::loadAssetMetadataFromFile(_path, *_asset->metadata);
    }

    if (asset::getAssetType(_asset->metadata->type) ==
        asset::AssetType::material) {
      asset::MaterialAssetInfo materialAssetInfo;
      asset::readMaterialAssetInfo(*_asset->metadata, materialAssetInfo);

      if (!materialAssetInfo.diffuseTexturePath.empty()) {
        _dependencies->push_back(&_runtimeResourceManager.getResource(
            materialAssetInfo.diffuseTexturePath));
      }

      if (!materialAssetInfo.normalMapTexturePath.empty()) {
        _dependencies->push_back(&_runtimeResourceManager.getResource(
            materialAssetInfo.normalMapTexturePath));
      }

      _dependencies->push_back(
          &_runtimeResourceManager.getResource(materialAssetInfo.shaderPath));
    }
  }

  return *_dependencies;
}

std::function<void(char*)> RuntimeResource::getUnpackFunc(auto const& info) {
  return [this, info](char* dst) {
    if (!_asset) {
      OBS_LOG_ERR("Unpack function called while the asset is not loaded.");
      return;
    }

    asset::unpackAsset(info, _asset->binaryBlob.data(),
                       _asset->binaryBlob.size(), dst);
    releaseAsset();
  };
}
