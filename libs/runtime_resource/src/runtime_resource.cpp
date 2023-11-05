#include <memory>
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
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>

using namespace obsidian;
using namespace obsidian::runtime_resource;

RuntimeResource::RuntimeResource(std::filesystem::path path,
                                 RuntimeResourceManager& runtimeResourceManager,
                                 rhi::RHI& rhi)
    : _runtimeResourceManager(runtimeResourceManager), _rhi{rhi},
      _path{std::move(path)} {}

RuntimeResource::~RuntimeResource() {
  releaseAsset();
  unloadFromRHI();
}

void RuntimeResource::unloadFromRHI() {
  if (_releaseFunc && _resourceRHI &&
      _resourceRHI->state != rhi::ResourceState::invalid) {
    _releaseFunc(_rhi, _resourceRHI->id);
    _releaseFunc = nullptr;
  }
}

bool RuntimeResource::loadAsset() {
  if (!_asset || _asset->isLoaded) {
    _asset = std::make_shared<asset::Asset>();
  }

  return asset::loadAssetFromFile(_path, *_asset);
}

void RuntimeResource::releaseAsset() {
  if (_asset) {
    _asset.reset();
  }
}

rhi::ResourceState RuntimeResource::getResourceState() const {
  if (_resourceRHI) {
    return _resourceRHI->state;
  }

  return rhi::ResourceState::initial;
}

rhi::ResourceIdRHI RuntimeResource::getResourceId() const {
  return _resourceRHI ? _resourceRHI->id : rhi::rhiIdUninitialized;
}

void RuntimeResource::uploadToRHI() {
  if (_resourceRHI && _resourceRHI->state != rhi::ResourceState::initial) {
    OBS_LOG_WARN("Trying to upload laready uploaded resource.");
    return;
  }

  if (!_asset) {
    if (!loadAsset()) {
      OBS_LOG_ERR("Failed to load asset." + _path.string());
      return;
    }
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
    uploadMesh.unpackFunc = [asset = _asset, info](char* dst) {
      asset::unpackAsset(info, asset->binaryBlob.data(),
                         asset->binaryBlob.size(), dst);
    };
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
    uploadTexture.unpackFunc = [asset = _asset, info](char* dst) {
      asset::unpackAsset(info, asset->binaryBlob.data(),
                         asset->binaryBlob.size(), dst);
    };

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

      if (diffuseTexResource.getResourceState() ==
          rhi::ResourceState::initial) {
        diffuseTexResource.uploadToRHI();
      }

      uploadMaterial.diffuseTextureId = diffuseTexResource.getResourceId();
    }

    if (!info.normalMapTexturePath.empty()) {
      RuntimeResource& normalMapResource =
          _runtimeResourceManager.getResource(info.normalMapTexturePath);

      if (normalMapResource.getResourceState() == rhi::ResourceState::initial) {
        normalMapResource.uploadToRHI();
      }

      uploadMaterial.normalTextureId = normalMapResource.getResourceId();
    }

    RuntimeResource& shaderResource =
        _runtimeResourceManager.getResource(info.shaderPath);

    if (shaderResource.getResourceState() == rhi::ResourceState::initial) {
      shaderResource.uploadToRHI();
    }

    uploadMaterial.shaderId = shaderResource.getResourceId();

    uploadMaterial.ambientColor = info.ambientColor;
    uploadMaterial.diffuseColor = info.diffuseColor;
    uploadMaterial.specularColor = info.specularColor;
    uploadMaterial.shininess = info.shininess;
    uploadMaterial.transparent = info.transparent;

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
    uploadShader.unpackFunc = [asset = _asset, info](char* dst) {
      asset::unpackAsset(info, asset->binaryBlob.data(),
                         asset->binaryBlob.size(), dst);
    };

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
}

rhi::ResourceIdRHI RuntimeResource::getResourceIdRHI() const {
  return _resourceRHI ? _resourceRHI->id : rhi::rhiIdUninitialized;
}

std::filesystem::path RuntimeResource::getRelativePath() const {
  return _runtimeResourceManager.getProject().getRelativeToProjectRootPath(
      _path);
}

std::vector<RuntimeResource const*> const&
RuntimeResource::fetchDependencies() {
  if (!_dependencies) {
    _dependencies = {};

    if (!_asset) {
      _asset = std::make_shared<asset::Asset>();
    }

    if (!_asset->metadata) {
      _asset->metadata = {};
      asset::loadAssetMetadataFromFile(_path, *_asset->metadata);
    }

    if (asset::getAssetType(_asset->metadata->type) ==
        asset::AssetType::material) {
      asset::TextureAssetInfo texAssetInfo;
      // asset::readTextureAssetInfo(const Asset &asset, TextureAssetInfo
      // &outTextureAssetInfo)
    }
  }

  return *_dependencies;
}
