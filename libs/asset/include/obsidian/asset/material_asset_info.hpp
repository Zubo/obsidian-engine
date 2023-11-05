#pragma once

#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/material.hpp>

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace obsidian::asset {

struct Asset;
struct AssetMetadata;

struct MaterialAssetInfo : public AssetInfo {
  core::MaterialType materialType;
  std::string shaderPath;
  std::string diffuseTexturePath;
  std::string normalMapTexturePath;
  glm::vec4 ambientColor;
  glm::vec4 diffuseColor;
  glm::vec4 specularColor;
  float shininess;
  bool transparent;
};

bool readMaterialAssetInfo(AssetMetadata const& assetMetadata,
                           MaterialAssetInfo& outMaterialAssetInfo);

bool packMaterial(MaterialAssetInfo const& materialAssetInfo,
                  std::vector<char> materialData, Asset& outAsset);

} /*namespace obsidian::asset*/
