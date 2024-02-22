#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/asset/asset_io.hpp>
#include <obsidian/asset/material_asset_info.hpp>
#include <obsidian/asset/mesh_asset_info.hpp>
#include <obsidian/asset/shader_asset_info.hpp>
#include <obsidian/asset/texture_asset_info.hpp>
#include <obsidian/core/logging.hpp>
#include <obsidian/core/material.hpp>
#include <obsidian/core/utils/visitor.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_loader.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

#include <cassert>
#include <memory>
#include <variant>

using namespace obsidian;
using namespace obsidian::runtime_resource;

RuntimeResourceRef::RuntimeResourceRef(RuntimeResource& runtimeResource)
    : _runtimeResource(&runtimeResource) {
  _runtimeResource->acquireRef();
}

RuntimeResourceRef::RuntimeResourceRef(RuntimeResourceRef const& other) {
  *this = other;
}

RuntimeResourceRef::RuntimeResourceRef(RuntimeResourceRef&& other) noexcept {
  *this = std::move(other);
}

RuntimeResourceRef::~RuntimeResourceRef() {
  if (!_runtimeResource) {
    return;
  }

  _runtimeResource->releaseRef();
  _runtimeResource = nullptr;
}

RuntimeResourceRef&
RuntimeResourceRef::operator=(RuntimeResourceRef const& other) noexcept {
  if (&other == this) {
    return *this;
  }

  if (_runtimeResource) {
    _runtimeResource->releaseRef();
  }

  _runtimeResource = other._runtimeResource;
  _runtimeResource->acquireRef();

  return *this;
}

RuntimeResourceRef&
RuntimeResourceRef::operator=(RuntimeResourceRef&& other) noexcept {
  if (&other == this) {
    return *this;
  }

  if (_runtimeResource) {
    _runtimeResource->releaseRef();
  }

  _runtimeResource = other._runtimeResource;
  other._runtimeResource = nullptr;

  return *this;
}

RuntimeResource& RuntimeResourceRef::operator*() {
  assert(_runtimeResource);
  return *_runtimeResource;
}

RuntimeResource const& RuntimeResourceRef::operator*() const {
  assert(_runtimeResource);
  return *_runtimeResource;
}

RuntimeResource* RuntimeResourceRef::operator->() { return _runtimeResource; }

RuntimeResource const* RuntimeResourceRef::operator->() const {
  return _runtimeResource;
}

RuntimeResource& RuntimeResourceRef::get() {
  assert(_runtimeResource);
  return *_runtimeResource;
}

RuntimeResource const& RuntimeResourceRef::get() const {
  assert(_runtimeResource);
  return *_runtimeResource;
}

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
    std::span<RuntimeResourceRef> deps = fetchDependencies();

    for (RuntimeResourceRef& d : deps) {
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

void RuntimeResource::acquireRef() { ++_refCount; }

void RuntimeResource::releaseRef() {
  assert(_refCount);

  std::uint32_t const cnt = --_refCount;

  if (!cnt) {
    releaseFromRHI();
    _dependencies.reset();
    _resourceState = RuntimeResourceState::initial;
  }
}

void RuntimeResource::releaseFromRHI() {
  if (_releaseFunc && _resourceRHI) {
    if (_transferRHI.transferStarted()) {
      _transferRHI.waitCompleted();
    }

    _transferRHI = {};
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
    std::string const debugNameStr = _path.string();
    uploadMesh.debugName = debugNameStr.c_str();

    _resourceRHI = &_rhi.initMeshResource();
    _transferRHI = _rhi.uploadMesh(_resourceRHI->id, uploadMesh);
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
    std::string const debugNameStr = _path.string();
    uploadTexture.debugName = debugNameStr.c_str();

    _resourceRHI = &_rhi.initTextureResource();
    _transferRHI =
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
    std::string const debugNameStr = _path.string();
    uploadMaterial.debugName = debugNameStr.c_str();

    switch (uploadMaterial.materialType) {
    case core::MaterialType::unlit: {
      rhi::UploadUnlitMaterialRHI& uploadUnlitMat =
          uploadMaterial.uploadMaterialSubtype
              .emplace<rhi::UploadUnlitMaterialRHI>();

      asset::UnlitMaterialAssetData const& unlitInfo =
          std::get<asset::UnlitMaterialAssetData>(info.materialSubtypeData);

      uploadUnlitMat.color = unlitInfo.color;

      if (!unlitInfo.colorTexturePath.empty()) {
        RuntimeResourceRef colorTexResourceRef =
            _runtimeResourceManager.getResource(unlitInfo.colorTexturePath);

        uploadUnlitMat.colorTextureId = colorTexResourceRef->getResourceId();
      }
      break;
    }
    case core::MaterialType::lit: {
      rhi::UploadLitMaterialRHI& uploadLitMat =
          uploadMaterial.uploadMaterialSubtype
              .emplace<rhi::UploadLitMaterialRHI>();

      asset::LitMaterialAssetData const& litInfo =
          std::get<asset::LitMaterialAssetData>(info.materialSubtypeData);

      if (!litInfo.diffuseTexturePath.empty()) {
        RuntimeResourceRef diffuseTexResourceRef =
            _runtimeResourceManager.getResource(litInfo.diffuseTexturePath);

        uploadLitMat.diffuseTextureId = diffuseTexResourceRef->getResourceId();
      }

      if (!litInfo.normalMapTexturePath.empty()) {
        RuntimeResourceRef normalMapResourceRef =
            _runtimeResourceManager.getResource(litInfo.normalMapTexturePath);

        uploadLitMat.normalTextureId = normalMapResourceRef->getResourceId();
      }

      uploadLitMat.ambientColor = litInfo.ambientColor;
      uploadLitMat.diffuseColor = litInfo.diffuseColor;
      uploadLitMat.specularColor = litInfo.specularColor;
      uploadLitMat.shininess = litInfo.shininess;
      uploadLitMat.reflection = litInfo.reflection;

      break;
    }
    case core::MaterialType::pbr: {
      rhi::UploadPBRMaterialRHI& uploadPbrMat =
          uploadMaterial.uploadMaterialSubtype
              .emplace<rhi::UploadPBRMaterialRHI>();

      asset::PBRMaterialAssetData const& pbrInfo =
          std::get<asset::PBRMaterialAssetData>(info.materialSubtypeData);

      uploadPbrMat.albedoTextureId =
          _runtimeResourceManager.getResource(pbrInfo.albedoTexturePath)
              ->getResourceId();
      uploadPbrMat.normalTextureId =
          _runtimeResourceManager.getResource(pbrInfo.normalMapTexturePath)
              ->getResourceId();
      uploadPbrMat.metalnessTextureId =
          _runtimeResourceManager.getResource(pbrInfo.metalnessTexturePath)
              ->getResourceId();

      if (!pbrInfo.roughnessTexturePath.empty()) {
        uploadPbrMat.roughnessTextureId =
            _runtimeResourceManager.getResource(pbrInfo.roughnessTexturePath)
                ->getResourceId();
      }

      break;
    }
    }

    RuntimeResourceRef vertexShaderResource =
        _runtimeResourceManager.getResource(info.vertexShaderPath);

    uploadMaterial.vertexShaderId = vertexShaderResource->getResourceId();

    RuntimeResourceRef fragmentShaderResource =
        _runtimeResourceManager.getResource(info.fragmentShaderPath);

    uploadMaterial.fragmentShaderId = fragmentShaderResource->getResourceId();

    uploadMaterial.transparent = info.transparent;
    uploadMaterial.hasTimer = info.hasTimer;

    _resourceRHI = &_rhi.initMaterialResource();
    _transferRHI = _rhi.uploadMaterial(_resourceRHI->id, uploadMaterial);
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

    std::string const debugNameStr = _path.string();
    uploadShader.debugName = debugNameStr.c_str();

    _resourceRHI = &_rhi.initShaderResource();
    _transferRHI = _rhi.uploadShader(_resourceRHI->id, uploadShader);
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

std::span<RuntimeResourceRef> RuntimeResource::fetchDependencies() {
  if (!_dependencies) {
    _dependencies.emplace();

    if (!_asset) {
      _asset = std::make_shared<asset::Asset>();
    }

    if (!_asset->metadata) {
      _asset->metadata.emplace();
      if (!asset::loadAssetMetadataFromFile(_path, *_asset->metadata)) {
        OBS_LOG_WARN("Failed to load asset metadata file at path " +
                     _path.string());
        return *_dependencies;
      }
    }

    if (asset::getAssetType(_asset->metadata->type) ==
        asset::AssetType::material) {
      asset::MaterialAssetInfo materialAssetInfo;
      asset::readMaterialAssetInfo(*_asset->metadata, materialAssetInfo);

      std::visit(
          core::visitor{
              [this](asset::UnlitMaterialAssetData const& unlitData) {
                if (!unlitData.colorTexturePath.empty()) {
                  _dependencies->push_back(_runtimeResourceManager.getResource(
                      unlitData.colorTexturePath));
                }
              },
              [this](asset::LitMaterialAssetData const& litData) {
                if (!litData.diffuseTexturePath.empty()) {
                  _dependencies->push_back(_runtimeResourceManager.getResource(
                      litData.diffuseTexturePath));
                }

                if (!litData.normalMapTexturePath.empty()) {
                  _dependencies->push_back(_runtimeResourceManager.getResource(
                      litData.normalMapTexturePath));
                }
              },
              [this](asset::PBRMaterialAssetData const& pbrData) {
                _dependencies->push_back(_runtimeResourceManager.getResource(
                    pbrData.albedoTexturePath));
                _dependencies->push_back(_runtimeResourceManager.getResource(
                    pbrData.normalMapTexturePath));
                _dependencies->push_back(_runtimeResourceManager.getResource(
                    pbrData.metalnessTexturePath));

                if (!pbrData.roughnessTexturePath.empty()) {
                  _dependencies->push_back(_runtimeResourceManager.getResource(
                      pbrData.roughnessTexturePath));
                }
              }},
          materialAssetInfo.materialSubtypeData);

      _dependencies->push_back(_runtimeResourceManager.getResource(
          materialAssetInfo.vertexShaderPath));
      _dependencies->push_back(_runtimeResourceManager.getResource(
          materialAssetInfo.fragmentShaderPath));
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
