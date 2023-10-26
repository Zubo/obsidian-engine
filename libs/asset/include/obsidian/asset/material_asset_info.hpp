#pragma once

#include <obsidian/asset/asset.hpp>
#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/material.hpp>

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace obsidian::asset {

struct MaterialAssetInfo : public AssetInfo {
  core::MaterialType materialType;
  std::string shaderPath;
  std::string diffuseTexturePath;
  std::string normalMapTexturePath;
  glm::vec3 diffuseColor;
  float shininess;
};

bool readMaterialAssetInfo(Asset const& asset,
                           MaterialAssetInfo& outMaterialAssetInfo);

bool packMaterial(MaterialAssetInfo const& materialAssetInfo,
                  std::vector<char> materialData, Asset& outAsset);

} /*namespace obsidian::asset*/
