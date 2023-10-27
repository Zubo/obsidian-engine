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
  if (_releaseFunc && _resourceIdRHI != rhi::rhiIdUninitialized) {
    _releaseFunc(_rhi, _resourceIdRHI);
    _releaseFunc = nullptr;
  }
}

bool RuntimeResource::loadAsset() {
  asset::Asset& asset = _asset.emplace();
  return asset::loadFromFile(_path, asset);
}

void RuntimeResource::releaseAsset() {
  if (_asset) {
    _asset.reset();
  }
}

rhi::ResourceIdRHI RuntimeResource::uploadToRHI() {
  if (_resourceIdRHI != rhi::rhiIdUninitialized) {
    // Already initialized
    return _resourceIdRHI;
  }

  if (!_asset) {
    if (!loadAsset()) {
      OBS_LOG_ERR("Failed to load asset." + _path.string());
      return _resourceIdRHI;
    }
  }
  asset::AssetType const assetType = asset::getAssetType(_asset->type);

  switch (assetType) {
  case asset::AssetType::mesh: {
    asset::MeshAssetInfo info;
    if (!asset::readMeshAssetInfo(*_asset, info)) {
      OBS_LOG_ERR("Failed to read mesh asset info");
      break;
    }
    rhi::UploadMeshRHI uploadMesh;
    uploadMesh.vertexCount = info.vertexCount;
    uploadMesh.vertexBufferSize = info.vertexBufferSize;
    uploadMesh.indexCount = info.indexCount;
    uploadMesh.indexBufferSizes = info.indexBufferSizes;
    uploadMesh.unpackFunc = [this, info](char* dst) {
      asset::unpackAsset(info, _asset->binaryBlob.data(),
                         _asset->binaryBlob.size(), dst);
    };
    _resourceIdRHI = _rhi.uploadMesh(uploadMesh);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.unloadMesh(id);
    };
    break;
  }
  case asset::AssetType::texture: {
    asset::TextureAssetInfo info;
    if (!asset::readTextureAssetInfo(*_asset, info)) {
      OBS_LOG_ERR("Failed to read texture asset info");
      break;
    }

    rhi::UploadTextureRHI uploadTexture;
    uploadTexture.format = info.format;
    uploadTexture.width = info.width;
    uploadTexture.height = info.height;
    uploadTexture.unpackFunc = [this, info](char* dst) {
      asset::unpackAsset(info, _asset->binaryBlob.data(),
                         _asset->binaryBlob.size(), dst);
    };
    _resourceIdRHI = _rhi.uploadTexture(uploadTexture);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.unloadTexture(id);
    };
    break;
  }
  case asset::AssetType::material: {
    asset::MaterialAssetInfo info;
    if (!asset::readMaterialAssetInfo(*_asset, info)) {
      OBS_LOG_ERR("Failed to read material asset info");
      break;
    }

    rhi::UploadMaterialRHI uploadMaterial;
    uploadMaterial.materialType = info.materialType;

    if (!info.diffuseTexturePath.empty()) {
      uploadMaterial.diffuseTextureId =
          _runtimeResourceManager.getResource(info.diffuseTexturePath)
              .uploadToRHI();
    }

    if (!info.normalMapTexturePath.empty()) {
      uploadMaterial.normalTextureId =
          _runtimeResourceManager.getResource(info.normalMapTexturePath)
              .uploadToRHI();
    }

    uploadMaterial.shaderId =
        _runtimeResourceManager.getResource(info.shaderPath).uploadToRHI();

    uploadMaterial.ambientColor = info.ambientColor;
    uploadMaterial.diffuseColor = info.diffuseColor;
    uploadMaterial.specularColor = info.specularColor;
    uploadMaterial.shininess = info.shininess;
    uploadMaterial.transparent = info.transparent;

    _resourceIdRHI = _rhi.uploadMaterial(uploadMaterial);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.unloadMaterial(id);
    };
    break;
  }
  case asset::AssetType::shader: {
    asset::ShaderAssetInfo info;
    if (!asset::readShaderAssetInfo(*_asset, info)) {
      OBS_LOG_ERR("Failed to read shader asset info");
      break;
    }

    rhi::UploadShaderRHI uploadShader;
    uploadShader.shaderDataSize = info.unpackedSize;
    uploadShader.unpackFunc = [this, info](char* dst) {
      asset::unpackAsset(info, _asset->binaryBlob.data(),
                         _asset->binaryBlob.size(), dst);
    };

    _resourceIdRHI = _rhi.uploadShader(uploadShader);
    _releaseFunc = [](rhi::RHI& rhi, rhi::ResourceIdRHI id) {
      rhi.unloadShader(id);
    };
    break;
  }
  default:
    OBS_LOG_ERR("Trying to upload unknown asset type");
  }

  return _resourceIdRHI;
}

rhi::ResourceIdRHI RuntimeResource::getResourceIdRHI() const {
  return _resourceIdRHI;
}

std::filesystem::path RuntimeResource::getRelativePath() const {
  return _runtimeResourceManager.getProject().getRelativeToProjectRootPath(
      _path);
}
