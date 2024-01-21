#pragma once

#include <obsidian/asset/asset_info.hpp>
#include <obsidian/core/material.hpp>

#include <glm/glm.hpp>

#include <string>
#include <variant>
#include <vector>

namespace obsidian::asset {

struct Asset;
struct AssetMetadata;

struct UnlitMaterialAssetData {
  std::string colorTexturePath;
  glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct LitMaterialAssetData {
  std::string diffuseTexturePath;
  std::string normalMapTexturePath;
  glm::vec4 ambientColor = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 diffuseColor = {1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 specularColor = {1.0f, 1.0f, 1.0f, 1.0f};
  float shininess;
  bool reflection;
};

struct PBRMaterialAssetData {
  std::string albedoTexturePath;
  std::string normalMapTexturePath;
  std::string metalnessTexturePath;
  std::string roughnessTexturePath;
};

using MaterialSubtypeData =
    std::variant<UnlitMaterialAssetData, LitMaterialAssetData,
                 PBRMaterialAssetData>;

MaterialSubtypeData createSubtypeData(core::MaterialType matType);

struct MaterialAssetInfo : public AssetInfo {
  core::MaterialType materialType;
  MaterialSubtypeData materialSubtypeData;
  std::string vertexShaderPath;
  std::string fragmentShaderPath;
  bool transparent;
  bool hasTimer;
};

bool readMaterialAssetInfo(AssetMetadata const& assetMetadata,
                           MaterialAssetInfo& outMaterialAssetInfo);

bool packMaterial(MaterialAssetInfo const& materialAssetInfo,
                  std::vector<char> materialData, Asset& outAsset);

} /*namespace obsidian::asset*/
